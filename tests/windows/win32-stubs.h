/*
 * win32-stubs.h — Minimal Win32 type/macro/function stubs for syntax-checking
 *                 AfriOS Windows test programs on a Linux host (no mingw-w64).
 *
 * INCLUDED FROM:
 *   Each `tests/windows/<test>/source.{c,cpp}` test program.  The test source
 *   must include this header FIRST, then guard the real `#include <windows.h>`
 *   behind `#ifdef _WIN32` so that:
 *     - On a Linux host (gcc / g++ without _WIN32 defined) the stubs below
 *       provide just enough surface area for `-fsyntax-only -Wall` to succeed.
 *     - On a real Windows target — or when cross-compiling with mingw-w64,
 *       which DOES define `_WIN32` — the stubs are skipped and the real
 *       `<windows.h>` / `<objbase.h>` provide every type/macro/function.
 *
 * DESIGN RULES:
 *   - Only the types, macros, and functions actually used by the 5 Windows
 *     test programs (hello-world, file-io, gdi-draw, registry-access,
 *     com-basic) are declared here.  Every extra typedef is a future
 *     maintenance burden and a possible name clash with the real Windows
 *     headers when this file is bypassed on _WIN32.
 *   - All function declarations are exactly that — declarations.  They are
 *     never called when the stubs are active (the test programs only run on
 *     Windows where the real implementations are linked).  The declarations
 *     exist purely so that `gcc -fsyntax-only` succeeds.
 *   - Handle types (HANDLE, HKEY, HDC, ...) are typedef'd as `void*` exactly
 *     like the real Win32 headers do (`typedef void *HANDLE;` etc.).  This
 *     means a NULL literal binds cleanly to any handle parameter, and a
 *     HBRUSH variable can be passed where HGDIOBJ is expected without a
 *     cast — both of which the test sources rely on.
 *   - DWORD / LONG are typedef'd as `unsigned long` / `long` (NOT the
 *     `<stdint.h>` fixed-width types).  This is *intentionally* wrong from a
 *     strict 32-bit-width point of view, but it makes the test sources'
 *     `printf("...%lu", GetLastError())` and `printf("...%ld", rc)` format
 *     specifiers match on the Linux x86_64 ABI (where `long` is 64-bit) so
 *     `-Wall` doesn't emit `-Wformat=` warnings.  On Windows the real
 *     Win32 typedefs are used instead.
 */
#ifndef AFROS_TESTS_WINDOWS_WIN32_STUBS_H
#define AFROS_TESTS_WINDOWS_WIN32_STUBS_H

#ifdef _WIN32
  /* Real Windows target (or mingw-w64 cross-compile).  Defer to <windows.h>
   * which the test source includes right after this header.  Nothing to do
   * here on that branch. */
#else
  /* ----------------------------------------------------------------------- */
  /* Linux host stubs — active for `gcc -fsyntax-only -Wall`.                */
  /* ----------------------------------------------------------------------- */

  #include <stdint.h>

  /* ----- Scalar typedefs ------------------------------------------------- */
  /* DWORD / LONG / HRESULT are `unsigned long` / `long` (matching the Linux
   * x86_64 ABI width) so that `%lu` / `%ld` / `%08lx` format specifiers in
   * the test sources don't trip `-Wformat=`.  This is a host-only
   * approximation — on real Windows the SDK typedefs are 32-bit.            */
  typedef unsigned long DWORD;     /* matches %lu                            */
  typedef long          LONG;      /* matches %ld                            */
  typedef unsigned long ULONG;     /* matches %lu                            */
  typedef long          HRESULT;   /* (unsigned long) cast matches %08lx     */
  typedef unsigned int  UINT;      /* Win32 UINT is 32-bit unsigned          */
  typedef unsigned short WORD;     /* 16-bit unsigned                        */
  typedef unsigned char  BYTE;     /*  8-bit unsigned                        */
  typedef int           BOOL;      /* Win32 BOOL is `int` (NOT C99 bool)     */

  /* ----- Handle typedefs ------------------------------------------------- */
  /* All Win32 handle types are `void *` — same convention as the real SDK.
   * This makes NULL bind cleanly to any handle parameter and lets e.g.
   * `HBRUSH` flow into an `HGDIOBJ` parameter without a cast (both are
   * `void *`), which the gdi-draw test relies on.                          */
  typedef void *HANDLE;
  typedef HANDLE HKEY;
  typedef HANDLE HDC;
  typedef HANDLE HWND;
  typedef HANDLE HGDIOBJ;
  typedef HANDLE HBRUSH;

  /* ----- Pointer typedefs (LP = Long Pointer, a 16-bit-era relic) ------- */
  typedef void*       LPVOID;
  typedef const void* LPCVOID;
  typedef char*       LPSTR;
  typedef const char* LPCSTR;
  typedef BYTE*       LPBYTE;
  typedef DWORD*      LPDWORD;
  typedef HKEY*       PHKEY;

  /* Registry access mask: in Win32 this is `ACCESS_MASK` (a DWORD alias).  */
  typedef DWORD ACCESS_MASK;
  typedef ACCESS_MASK REGSAM;

  /* Security attributes + overlapped I/O are opaque to the test programs
   * (they only ever pass NULL through), so a pointer-to-incomplete-struct
   * stub is enough — and crucially it lets NULL bind without a cast.       */
  typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
  typedef struct _OVERLAPPED          OVERLAPPED;
  typedef OVERLAPPED                  *LPOVERLAPPED;

  /* ----- Macros ---------------------------------------------------------- */

  /* CreateFileA dwDesiredAccess / dwShareMode                              */
  #define GENERIC_READ    0x80000000ul
  #define GENERIC_WRITE   0x40000000ul
  #define FILE_SHARE_READ 0x00000001ul

  /* CreateFileA dwCreationDisposition                                      */
  #define OPEN_EXISTING 3ul
  #define CREATE_ALWAYS 2ul

  /* CreateFileA dwFlagsAndAttributes                                       */
  #define FILE_ATTRIBUTE_NORMAL 0x00000080ul

  /* Special handle sentinel (matches Win32 `(HANDLE)(-1)`).                */
  #define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

  /* Registry success code (returned by Reg* functions as LONG).            */
  #define ERROR_SUCCESS 0L

  /* Predefined registry root key used by the registry-access test.         */
  #define HKEY_CURRENT_USER ((HKEY)(intptr_t)0x80000001)

  /* Registry access masks (REGSAM).                                        */
  #define KEY_READ  0x20019ul
  #define KEY_WRITE 0x20006ul

  /* Registry value type for a null-terminated string.                      */
  #define REG_SZ 1ul

  /* COM class-context flag (CoCreateInstance dwClsContext).                */
  #define CLSCTX_INPROC_SERVER 0x1ul

  /* COINIT_* — passed to CoInitializeEx.                                   */
  #define COINIT_APARTMENTTHREADED 0x2ul

  /* FAILED macro: sign-test on HRESULT (per Win32 convention).  Under the
   * Linux x86_64 ABI `long` is 64-bit, so 0x80004005L is a positive value
   * — but that's irrelevant for `-fsyntax-only`: the macro just needs to
   * expand to a syntactically valid expression.                            */
  #define FAILED(hr) (((HRESULT)(hr)) < 0)

  /* GDI RGB() macro: assembles a 0x00BBGGRR COLORREF from 3 byte channels. */
  #define RGB(r,g,b) \
      ((DWORD)(((BYTE)(r)) | (((WORD)((BYTE)(g))) << 8) | \
               (((DWORD)((BYTE)(b))) << 16)))

  /* ----- GUID / CLSID / IID --------------------------------------------- */
  typedef struct _GUID {
      uint32_t Data1;
      uint16_t Data2;
      uint16_t Data3;
      uint8_t  Data4[8];
  } GUID;

  typedef GUID CLSID;
  typedef GUID IID;

  /* `REFCLSID` / `REFIID` are passed by reference in C++ (matching the
   * real Win32 headers), by pointer in C.  The com-basic test compiles as
   * C++ and passes a `const CLSID` lvalue directly to CoCreateInstance.   */
  #ifdef __cplusplus
    typedef const CLSID& REFCLSID;
    typedef const IID&   REFIID;
  #else
    typedef const CLSID* REFCLSID;
    typedef const IID*   REFIID;
  #endif

  /* ----- IUnknown (minimal, for the com-basic test) --------------------- */
  /* Forward-declared so the vtable's function-pointer members can refer to
   * `struct IUnknown*` without a circular typedef.                         */
  struct IUnknown;
  typedef struct IUnknownVtbl IUnknownVtbl;

  struct IUnknownVtbl {
      HRESULT (*QueryInterface)(struct IUnknown*, REFIID, void**);
      ULONG   (*AddRef)(struct IUnknown*);
      ULONG   (*Release)(struct IUnknown*);
  };

  struct IUnknown {
      IUnknownVtbl *lpVtbl;
  };

  /* ----- Function declarations (stubs — never linked on Linux) --------- */
  /* Each declaration matches the Win32 SDK signature as closely as the
   * stub types allow.  These functions are NOT defined on Linux: they
   * exist purely so the test sources pass `-fsyntax-only`.  On Windows /
   * mingw-w64 the real implementations come from kernel32/gdi32/advapi32/
   * ole32.                                                                 */

  /* Process control (kernel32)                                            */
  void  ExitProcess(UINT uExitCode);

  /* File I/O (kernel32)                                                   */
  HANDLE CreateFileA(LPCSTR lpFileName,
                     DWORD dwDesiredAccess,
                     DWORD dwShareMode,
                     LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                     DWORD dwCreationDisposition,
                     DWORD dwFlagsAndAttributes,
                     HANDLE hTemplateFile);
  BOOL WriteFile(HANDLE hFile,
                 LPVOID lpBuffer,
                 DWORD nNumberOfBytesToWrite,
                 LPDWORD lpNumberOfBytesWritten,
                 LPOVERLAPPED lpOverlapped);
  BOOL ReadFile(HANDLE hFile,
                LPVOID lpBuffer,
                DWORD nNumberOfBytesToRead,
                LPDWORD lpNumberOfBytesRead,
                LPOVERLAPPED lpOverlapped);
  BOOL  CloseHandle(HANDLE hObject);
  DWORD GetLastError(void);

  /* Registry (advapi32)                                                   */
  /* NB: RegSetValueExA's `lpData` parameter is `const BYTE *` in the real
   * SDK — we use `LPCVOID` (same width, same const) so that `(const BYTE*)
   * data` binds cleanly without a `-Wdiscarded-qualifiers` warning.        */
  LONG RegCreateKeyExA(HKEY hKey,
                       LPCSTR lpSubKey,
                       DWORD Reserved,
                       LPSTR lpClass,
                       DWORD dwOptions,
                       REGSAM samDesired,
                       LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                       PHKEY phkResult,
                       LPDWORD lpdwDisposition);
  LONG RegSetValueExA(HKEY hKey,
                      LPCSTR lpValueName,
                      DWORD Reserved,
                      DWORD dwType,
                      LPCVOID lpData,
                      DWORD cbData);
  LONG RegOpenKeyExA(HKEY hKey,
                     LPCSTR lpSubKey,
                     DWORD ulOptions,
                     REGSAM samDesired,
                     PHKEY phkResult);
  LONG RegQueryValueExA(HKEY hKey,
                        LPCSTR lpValueName,
                        LPDWORD lpReserved,
                        LPDWORD lpType,
                        LPVOID lpData,
                        LPDWORD lpcbData);
  LONG RegCloseKey(HKEY hKey);

  /* GDI / User32                                                          */
  HDC     GetDC(HWND hWnd);
  int     ReleaseDC(HWND hWnd, HDC hDC);
  HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h);
  BOOL    Rectangle(HDC hdc, int left, int top, int right, int bottom);
  HBRUSH  CreateSolidBrush(DWORD color);
  BOOL    DeleteObject(HGDIOBJ hObject);

  /* COM (ole32)                                                           */
  HRESULT CoInitializeEx(LPVOID pvReserved, DWORD dwCoInit);
  void    CoUninitialize(void);
  HRESULT CoCreateInstance(REFCLSID rclsid,
                           LPVOID pUnkOuter,
                           DWORD dwClsContext,
                           REFIID riid,
                           LPVOID *ppv);

#endif /* !_WIN32 (Linux host stubs branch) */

#endif /* AFROS_TESTS_WINDOWS_WIN32_STUBS_H */
