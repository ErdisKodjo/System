/** @file
  Minimal recursive-descent parser for Apple-style config.plist (XML property
  list) used by OpenCore/Clover configurations.

  The parser supports a subset of plist sufficient for the AppleBootHelper:
  <dict>, <key>, <string>, <integer>, <array>, <true/>, <false/>. It exposes
  three public entry points:

    - ParseConfigPlist()  : parse a buffer into a CONFIG_PLIST tree.
    - ConfigGetString()   : look up a dotted path, return a string value.
    - ConfigGetInteger()  : look up a dotted path, return a UINT64 value.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>

//
// Maximum nesting depth supported by the parser (defensive limit).
//
#define PLIST_MAX_DEPTH       16
#define PLIST_MAX_TOKEN_LEN   256

//
// Public plist value types.
//
typedef enum {
  PlistTypeNone,
  PlistTypeString,
  PlistTypeInteger,
  PlistTypeDict,
  PlistTypeArray,
  PlistTypeTrue,
  PlistTypeFalse
} PLIST_VALUE_TYPE;

typedef struct _CONFIG_VALUE CONFIG_VALUE;

//
// A key/value entry inside a <dict>.
//
typedef struct {
  CHAR8           Key[PLIST_MAX_TOKEN_LEN];
  CONFIG_VALUE    *Value;
} PLIST_DICT_ENTRY;

//
// A generic plist value node. For dicts, Entries/Count hold the children;
// for arrays, Items/Count hold the children.
//
struct _CONFIG_VALUE {
  PLIST_VALUE_TYPE   Type;
  CHAR8              StrValue[PLIST_MAX_TOKEN_LEN];
  UINT64             IntValue;
  PLIST_DICT_ENTRY   *Entries;   // for PlistTypeDict
  CONFIG_VALUE       **Items;    // for PlistTypeArray
  UINTN              Count;
};

typedef struct {
  CONFIG_VALUE  *Root;
} CONFIG_PLIST;

//
// Internal parser state.
//
typedef struct {
  CONST CHAR8   *Cursor;
  UINTN         Length;
  UINTN         Pos;
  UINTN         Depth;
} PLIST_PARSER;

/**
  Advances the cursor past whitespace characters.
**/
STATIC
VOID
ParserSkipWs (
  IN OUT PLIST_PARSER  *P
  )
{
  while (P->Pos < P->Length) {
    CHAR8  C = P->Cursor[P->Pos];
    if ((C == ' ') || (C == '\t') || (C == '\n') || (C == '\r')) {
      P->Pos++;
    } else {
      break;
    }
  }
}

/**
  Returns TRUE if the cursor matches the given literal string. Does not
  advance on mismatch.
**/
STATIC
BOOLEAN
ParserMatch (
  IN OUT PLIST_PARSER  *P,
  IN     CONST CHAR8   *Lit
  )
{
  UINTN  Len;

  Len = AsciiStrLen (Lit);
  if ((P->Pos + Len) > P->Length) {
    return FALSE;
  }
  if (CompareMem (&P->Cursor[P->Pos], Lit, Len) != 0) {
    return FALSE;
  }
  P->Pos += Len;
  return TRUE;
}

/**
  Allocates a fresh plist value node.
**/
STATIC
CONFIG_VALUE *
PlistNewValue (
  IN PLIST_VALUE_TYPE  Type
  )
{
  CONFIG_VALUE  *V;

  V = (CONFIG_VALUE *)AllocateZeroPool (sizeof (CONFIG_VALUE));
  if (V == NULL) {
    return NULL;
  }
  V->Type = Type;
  return V;
}

/**
  Parses a tag of the form <name ...>body</name> or <name .../>.

  @param[in,out] P         Parser state.
  @param[out]    OutValue  Receives the parsed value (must be freed by caller).

  @retval EFI_SUCCESS      Parsed successfully.
**/
STATIC
EFI_STATUS
PlistParseValue (
  IN OUT PLIST_PARSER  *P,
  OUT    CONFIG_VALUE  **OutValue
  );

/**
  Parses the contents of a <dict>...</dict> element.
**/
STATIC
EFI_STATUS
PlistParseDict (
  IN OUT PLIST_PARSER  *P,
  OUT    CONFIG_VALUE  *Dict
  )
{
  EFI_STATUS          Status;
  PLIST_DICT_ENTRY    *Entries;
  UINTN               Capacity;
  CONST CHAR8         *KeyStart;
  UINTN               KeyLen;
  UINTN               Index;

  Entries  = NULL;
  Capacity = 0;
  Dict->Count = 0;

  for (;;) {
    ParserSkipWs (P);
    if (ParserMatch (P, "</dict>")) {
      break;
    }
    if (!ParserMatch (P, "<key>")) {
      DEBUG ((DEBUG_WARN, "PlistParser: expected <key> in <dict>\n"));
      return EFI_VOLUME_CORRUPTED;
    }
    KeyStart = &P->Cursor[P->Pos];
    KeyLen   = 0;
    while ((P->Pos < P->Length) && (P->Cursor[P->Pos] != '<')) {
      P->Pos++;
      KeyLen++;
    }
    if (!ParserMatch (P, "</key>")) {
      return EFI_VOLUME_CORRUPTED;
    }

    if (Dict->Count == Capacity) {
      Capacity = (Capacity == 0) ? 8 : (Capacity * 2);
      Entries = (PLIST_DICT_ENTRY *)ReallocatePool (
                                       Dict->Count * sizeof (*Entries),
                                       Capacity * sizeof (*Entries),
                                       Entries
                                       );
      if (Entries == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }
    }

    Index = Dict->Count++;
    ZeroMem (&Entries[Index], sizeof (Entries[Index]));
    if (KeyLen >= PLIST_MAX_TOKEN_LEN) {
      KeyLen = PLIST_MAX_TOKEN_LEN - 1;
    }
    CopyMem (Entries[Index].Key, KeyStart, KeyLen);
    Entries[Index].Key[KeyLen] = '\0';

    ParserSkipWs (P);
    Status = PlistParseValue (P, &Entries[Index].Value);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  Dict->Entries = Entries;
  return EFI_SUCCESS;
}

/**
  Parses the contents of an <array>...</array> element.
**/
STATIC
EFI_STATUS
PlistParseArray (
  IN OUT PLIST_PARSER  *P,
  OUT    CONFIG_VALUE  *Array
  )
{
  EFI_STATUS    Status;
  CONFIG_VALUE  **Items;
  UINTN         Capacity;

  Items    = NULL;
  Capacity = 0;
  Array->Count = 0;

  for (;;) {
    ParserSkipWs (P);
    if (ParserMatch (P, "</array>")) {
      break;
    }
    if (Array->Count == Capacity) {
      Capacity = (Capacity == 0) ? 8 : (Capacity * 2);
      Items = (CONFIG_VALUE **)ReallocatePool (
                                 Array->Count * sizeof (*Items),
                                 Capacity * sizeof (*Items),
                                 Items
                                 );
      if (Items == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }
    }
    Status = PlistParseValue (P, &Items[Array->Count]);
    if (EFI_ERROR (Status)) {
      return Status;
    }
    Array->Count++;
  }

  Array->Items = Items;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PlistParseValue (
  IN OUT PLIST_PARSER  *P,
  OUT    CONFIG_VALUE  **OutValue
  )
{
  EFI_STATUS    Status;
  CONFIG_VALUE  *V;
  CONST CHAR8   *StrStart;
  UINTN         StrLen;
  CHAR8         *EndPtr;

  if (P->Depth >= PLIST_MAX_DEPTH) {
    return EFI_ABORTED;
  }
  P->Depth++;

  ParserSkipWs (P);
  V = NULL;

  if (ParserMatch (P, "<dict>")) {
    V = PlistNewValue (PlistTypeDict);
    Status = PlistParseDict (P, V);
  } else if (ParserMatch (P, "<array>")) {
    V = PlistNewValue (PlistTypeArray);
    Status = PlistParseArray (P, V);
  } else if (ParserMatch (P, "<true/>")) {
    V = PlistNewValue (PlistTypeTrue);
    V->IntValue = 1;
    Status = EFI_SUCCESS;
  } else if (ParserMatch (P, "<false/>")) {
    V = PlistNewValue (PlistTypeFalse);
    Status = EFI_SUCCESS;
  } else if (ParserMatch (P, "<string>")) {
    StrStart = &P->Cursor[P->Pos];
    StrLen   = 0;
    while ((P->Pos < P->Length) && (P->Cursor[P->Pos] != '<')) {
      P->Pos++;
      StrLen++;
    }
    if (!ParserMatch (P, "</string>")) {
      Status = EFI_VOLUME_CORRUPTED;
    } else {
      V = PlistNewValue (PlistTypeString);
      if (StrLen >= PLIST_MAX_TOKEN_LEN) {
        StrLen = PLIST_MAX_TOKEN_LEN - 1;
      }
      CopyMem (V->StrValue, StrStart, StrLen);
      V->StrValue[StrLen] = '\0';
      Status = EFI_SUCCESS;
    }
  } else if (ParserMatch (P, "<integer>")) {
    StrStart = &P->Cursor[P->Pos];
    StrLen   = 0;
    while ((P->Pos < P->Length) && (P->Cursor[P->Pos] != '<')) {
      P->Pos++;
      StrLen++;
    }
    if (!ParserMatch (P, "</integer>")) {
      Status = EFI_VOLUME_CORRUPTED;
    } else {
      CHAR8  Tmp[PLIST_MAX_TOKEN_LEN];
      V = PlistNewValue (PlistTypeInteger);
      if (StrLen >= PLIST_MAX_TOKEN_LEN) {
        StrLen = PLIST_MAX_TOKEN_LEN - 1;
      }
      CopyMem (Tmp, StrStart, StrLen);
      Tmp[StrLen] = '\0';
      V->IntValue = AsciiStrDecimalToUintn (Tmp);
      AsciiStrToUnicode (Tmp, &EndPtr);  // suppress unused-warning if no Unicode build.
      Status = EFI_SUCCESS;
    }
  } else {
    DEBUG ((DEBUG_WARN, "PlistParser: unknown element at pos %lu\n", (UINT64)P->Pos));
    Status = EFI_VOLUME_CORRUPTED;
  }

  P->Depth--;
  if (EFI_ERROR (Status)) {
    if (V != NULL) {
      FreePool (V);
    }
    return Status;
  }

  *OutValue = V;
  return EFI_SUCCESS;
}

/**
  Parses a config.plist XML buffer into a CONFIG_PLIST tree.

  @param[in]  Buffer   Pointer to the raw plist bytes (UTF-8, null-terminated
                       is not required, Length is used).
  @param[in]  Length   Number of bytes in Buffer.
  @param[out] Plist    Receives the parsed plist tree. Caller must free it via
                       FreePool on Plist->Root (a deep-free is left to caller).

  @retval EFI_SUCCESS           Parsed successfully.
  @retval EFI_INVALID_PARAMETER Null parameter.
  @retval EFI_VOLUME_CORRUPTED  XML is malformed.
**/
EFI_STATUS
EFIAPI
ParseConfigPlist (
  IN  CONST VOID    *Buffer,
  IN  UINTN         Length,
  OUT CONFIG_PLIST  *Plist
  )
{
  PLIST_PARSER  P;
  EFI_STATUS    Status;

  if ((Buffer == NULL) || (Plist == NULL) || (Length == 0)) {
    return EFI_INVALID_PARAMETER;
  }

  ZeroMem (Plist, sizeof (*Plist));
  ZeroMem (&P, sizeof (P));
  P.Cursor = (CONST CHAR8 *)Buffer;
  P.Length = Length;
  P.Pos    = 0;
  P.Depth  = 0;

  ParserSkipWs (&P);
  //
  // Skip an optional XML declaration <?xml ... ?>.
  //
  if (ParserMatch (&P, "<?xml")) {
    while (P.Pos < P.Length) {
      if (ParserMatch (&P, "?>")) {
        break;
      }
      P.Pos++;
    }
    ParserSkipWs (&P);
  }

  //
  // Skip an optional <!DOCTYPE ... > declaration.
  //
  if (ParserMatch (&P, "<!DOCTYPE")) {
    while (P.Pos < P.Length) {
      if (ParserMatch (&P, ">")) {
        break;
      }
      P.Pos++;
    }
    ParserSkipWs (&P);
  }

  //
  // Skip an optional <plist version="1.0"> wrapper.
  //
  if (ParserMatch (&P, "<plist")) {
    while (P.Pos < P.Length) {
      if (ParserMatch (&P, ">")) {
        break;
      }
      P.Pos++;
    }
    ParserSkipWs (&P);
  }

  Status = PlistParseValue (&P, &Plist->Root);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "PlistParser: parse failed (%r)\n", Status));
    return Status;
  }

  //
  // Consume optional closing </plist> if present.
  //
  ParserSkipWs (&P);
  ParserMatch (&P, "</plist>");

  return EFI_SUCCESS;
}

/**
  Walks a dotted path like "Kernel.BootArgs" through dict children and returns
  the leaf value, or NULL if any segment is missing or not a dict.

  @param[in] Plist   The parsed plist tree.
  @param[in] Path    Dotted ASCII path, e.g. "Kernel.BootArgs".

  @return Pointer to the leaf CONFIG_VALUE, or NULL.
**/
STATIC
CONFIG_VALUE *
ConfigLookup (
  IN CONFIG_PLIST  *Plist,
  IN CONST CHAR8   *Path
  )
{
  CONFIG_VALUE  *Node;
  CONST CHAR8   *Seg;
  CONST CHAR8   *Next;
  UINTN         SegLen;
  UINTN         Index;

  if ((Plist == NULL) || (Path == NULL) || (Plist->Root == NULL)) {
    return NULL;
  }

  Node = Plist->Root;
  Seg  = Path;

  while ((Seg != NULL) && (*Seg != '\0')) {
    Next = Seg;
    while ((*Next != '\0') && (*Next != '.')) {
      Next++;
    }
    SegLen = (UINTN)(Next - Seg);

    if (Node->Type != PlistTypeDict) {
      return NULL;
    }
    Index = 0;
    while (Index < Node->Count) {
      if (AsciiStrnCmp (Node->Entries[Index].Key, Seg, SegLen) == 0
          && Node->Entries[Index].Key[SegLen] == '\0') {
        break;
      }
      Index++;
    }
    if (Index >= Node->Count) {
      return NULL;
    }
    Node = Node->Entries[Index].Value;
    Seg  = (*Next == '\0') ? NULL : (Next + 1);
  }

  return Node;
}

/**
  Returns a string value for the given dotted path.

  @param[in]  Plist    Parsed plist tree.
  @param[in]  Path     Dotted path.
  @param[out] Value    Buffer receiving the ASCII string (null-terminated).

  @retval EFI_SUCCESS           Value retrieved.
  @retval EFI_NOT_FOUND         Path not present.
  @retval EFI_INVALID_PARAMETER Bad parameters.
**/
EFI_STATUS
EFIAPI
ConfigGetString (
  IN  CONFIG_PLIST  *Plist,
  IN  CONST CHAR8   *Path,
  OUT CHAR8         *Value
  )
{
  CONFIG_VALUE  *V;

  if ((Plist == NULL) || (Path == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  V = ConfigLookup (Plist, Path);
  if ((V == NULL) || (V->Type != PlistTypeString)) {
    return EFI_NOT_FOUND;
  }

  AsciiStrCpyS (Value, PLIST_MAX_TOKEN_LEN, V->StrValue);
  return EFI_SUCCESS;
}

/**
  Returns an integer value for the given dotted path.

  @param[in]  Plist    Parsed plist tree.
  @param[in]  Path     Dotted path.
  @param[out] Value    Receives the integer.

  @retval EFI_SUCCESS           Value retrieved.
  @retval EFI_NOT_FOUND         Path not present or not an integer.
**/
EFI_STATUS
EFIAPI
ConfigGetInteger (
  IN  CONFIG_PLIST  *Plist,
  IN  CONST CHAR8   *Path,
  OUT UINT64        *Value
  )
{
  CONFIG_VALUE  *V;

  if ((Plist == NULL) || (Path == NULL) || (Value == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  V = ConfigLookup (Plist, Path);
  if (V == NULL) {
    return EFI_NOT_FOUND;
  }
  if (V->Type == PlistTypeInteger) {
    *Value = V->IntValue;
    return EFI_SUCCESS;
  }
  if (V->Type == PlistTypeTrue) {
    *Value = 1;
    return EFI_SUCCESS;
  }
  if (V->Type == PlistTypeFalse) {
    *Value = 0;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}
