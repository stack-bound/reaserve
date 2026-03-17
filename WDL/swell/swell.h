// Minimal SWELL stub for REAPER extension plugins on non-Windows platforms.
// SWELL (Simple Windows Emulation Layer) types are provided by REAPER at runtime.
// We only need the type definitions to compile.

#ifndef _SWELL_H_STUB_
#define _SWELL_H_STUB_

#include <cstdint>
#include <cstring>

// Basic Windows types
typedef int BOOL;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef uintptr_t UINT_PTR;
typedef intptr_t INT_PTR;
typedef long LONG;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef intptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef void* LPVOID;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef uint64_t WDL_UINT64;

// Handle types
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HDC;
typedef void* HFONT;
typedef void* HPEN;
typedef void* HBRUSH;
typedef void* HICON;
typedef void* HCURSOR;
typedef void* HMENU;
typedef void* HGDIOBJ;
typedef void* HBITMAP;
typedef void* HANDLE;
typedef void* HMODULE;
typedef void* HKEY;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif

// RECT
typedef struct {
    LONG left, top, right, bottom;
} RECT;

typedef struct {
    LONG x, y;
} POINT;

// MSG
typedef struct {
    HWND hwnd;
    UINT message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD time;
    POINT pt;
} MSG;

// ACCEL
typedef struct {
    BYTE fVirt;
    WORD key;
    WORD cmd;
} ACCEL;

// LOGFONT
typedef struct {
    LONG lfHeight;
    LONG lfWidth;
    LONG lfEscapement;
    LONG lfOrientation;
    LONG lfWeight;
    BYTE lfItalic;
    BYTE lfUnderline;
    BYTE lfStrikeOut;
    BYTE lfCharSet;
    BYTE lfOutPrecision;
    BYTE lfClipPrecision;
    BYTE lfQuality;
    BYTE lfPitchAndFamily;
    char lfFaceName[32];
} LOGFONT;

typedef DWORD COLORREF;

// GUID
#ifndef _GUID_DEFINED
#define _GUID_DEFINED
typedef struct {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID;
#endif

#endif // _SWELL_H_STUB_
