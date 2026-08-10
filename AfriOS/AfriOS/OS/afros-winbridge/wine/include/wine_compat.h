#ifndef AFROS_WINBRIDGE_WINE_COMPAT_H
#define AFROS_WINBRIDGE_WINE_COMPAT_H

/*
 * wine_compat.h — Types et macros de compatibilité Win32/NT pour afros-winbridge.
 *
 * Ce fichier centralise les typedefs Wine/Windows (NTSTATUS, HANDLE, DWORD,
 * BOOL, LARGE_INTEGER, etc.) ainsi que les constantes de statut NT les plus
 * courantes. Il permet aux modules C de afros-winbridge de compiler sans
 * dépendre des en-têtes Windows natifs: on reste purement ISO C99.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* --- Types scalaires Win32/NT ------------------------------------------- */

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef int             BOOL;
typedef unsigned char   BYTE;
typedef unsigned short  WORD;
typedef unsigned long   DWORD;
typedef long            LONG;
typedef unsigned long   ULONG;
typedef uint64_t        ULONGLONG;
typedef int64_t         LONGLONG;
typedef uint32_t        NTSTATUS;
typedef int64_t         LONG_PTR;
typedef uint64_t        ULONG_PTR;
typedef void           *HANDLE;
typedef void           *PVOID;
typedef const void     *LPCVOID;
typedef char           *LPSTR;
typedef const char     *LPCSTR;
typedef wchar_t        *LPWSTR;
typedef const wchar_t  *LPCWSTR;
typedef ULONG           ACCESS_MASK;
typedef DWORD          *LPDWORD;
typedef WORD           *LPWORD;
typedef BYTE           *LPBYTE;
typedef void           *HMODULE;
typedef ULONG_PTR       HKEY_DATA;
typedef void           *HKEY;
typedef void           *HWND;
typedef ULONG           WPARAM;
typedef LONG            LPARAM;
typedef LONG_PTR        LRESULT;
typedef long            HRESULT;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* --- Constantes NTSTATUS les plus utiles -------------------------------- */

#define STATUS_SUCCESS                 ((NTSTATUS)0x00000000)
#define STATUS_UNSUCCESSFUL            ((NTSTATUS)0xC0000001)
#define STATUS_NOT_IMPLEMENTED         ((NTSTATUS)0xC0000002)
#define STATUS_INVALID_INFO_CLASS      ((NTSTATUS)0xC0000003)
#define STATUS_INVALID_PARAMETER       ((NTSTATUS)0xC000000D)
#define STATUS_INVALID_HANDLE          ((NTSTATUS)0xC0000008)
#define STATUS_HANDLE_NOT_CLOSABLE     ((NTSTATUS)0xC0000235)
#define STATUS_NO_MEMORY               ((NTSTATUS)0xC0000017)
#define STATUS_ACCESS_DENIED           ((NTSTATUS)0xC0000022)
#define STATUS_BUFFER_TOO_SMALL        ((NTSTATUS)0xC0000023)
#define STATUS_OBJECT_NAME_NOT_FOUND   ((NTSTATUS)0xC0000034)
#define STATUS_OBJECT_NAME_COLLISION   ((NTSTATUS)0xC0000035)
#define STATUS_NOT_FOUND               ((NTSTATUS)0xC0000225)
#define STATUS_NOT_SUPPORTED           ((NTSTATUS)0xC00000BB)
#define STATUS_INSUFFICIENT_RESOURCES  ((NTSTATUS)0xC000009A)
#define STATUS_NO_MORE_ENTRIES         ((NTSTATUS)0x8000001A)
#define STATUS_TIMEOUT                 ((NTSTATUS)0x00000102)
#define STATUS_PENDING                 ((NTSTATUS)0x00000103)
#define STATUS_END_OF_FILE             ((NTSTATUS)0xC0000011)
#define STATUS_FILE_NOT_FOUND          ((NTSTATUS)0xC000000F)

#define NT_SUCCESS(s) ((NTSTATUS)(s) >= 0)
#define NT_ERROR(s)   (((NTSTATUS)(s) >> 30) == 3)

/* --- Codes d'erreur Win32 classiques ------------------------------------ */

#define ERROR_SUCCESS              0L
#define ERROR_INVALID_PARAMETER    87L
#define ERROR_FILE_NOT_FOUND       2L
#define ERROR_PATH_NOT_FOUND       3L
#define ERROR_ACCESS_DENIED        5L
#define ERROR_NOT_ENOUGH_MEMORY    8L
#define ERROR_NO_MORE_ITEMS        259L
#define ERROR_INSUFFICIENT_BUFFER  122L

/* --- Attributs de fichier Win32 ----------------------------------------- */

#define FILE_ATTRIBUTE_READONLY         0x00000001
#define FILE_ATTRIBUTE_HIDDEN           0x00000002
#define FILE_ATTRIBUTE_SYSTEM           0x00000004
#define FILE_ATTRIBUTE_DIRECTORY        0x00000010
#define FILE_ATTRIBUTE_ARCHIVE          0x00000020
#define FILE_ATTRIBUTE_NORMAL           0x00000080
#define FILE_ATTRIBUTE_TEMPORARY        0x00000100
#define FILE_ATTRIBUTE_REPARSE_POINT    0x00000400
#define FILE_ATTRIBUTE_COMPRESSED       0x00000800
#define FILE_ATTRIBUTE_OFFLINE          0x00001000

/* --- Macros de compilation --------------------------------------------- */

#define WINAPI
#define NTAPI
#define CALLBACK
#define STDMETHODCALLTYPE
#define __cdecl
#define IN
#define OUT
#define OPTIONAL
#define UNREFERENCED_PARAMETER(p) ((void)(p))

#ifndef _countof
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#endif
#ifndef FIELD_OFFSET
#define FIELD_OFFSET(t, f) ((DWORD)(ptrdiff_t)(&(((t *)0)->f)))
#endif

/* --- Grand entier 64 bits ---------------------------------------------- */

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG  HighPart; } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct { DWORD LowPart; DWORD HighPart; } u;
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

/* --- Codes d'attente WaitForSingleObject ------------------------------- */

#define WAIT_FAILED    ((DWORD)0xFFFFFFFF)
#define WAIT_OBJECT_0  ((DWORD)0x00000000)
#define WAIT_TIMEOUT   ((DWORD)0x00000102)
#define WAIT_ABANDONED ((DWORD)0x00000080)
#define INFINITE       0xFFFFFFFF

#endif /* AFROS_WINBRIDGE_WINE_COMPAT_H */
