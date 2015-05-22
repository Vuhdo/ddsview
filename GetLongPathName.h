/*****************************************************************************
 *
 * GetLongPathName
 *
 *****************************************************************************/

//the getLongPathName() function I wrote only converted the
//file name, not every directory in the path...so instead of
//wasting time completing it, use microsofts official
//GetLongPathName() for win95 for all systems :)

#include <shlobj.h>

#undef GetLongPathName
#define GetLongPathName Emulate_GetLongPathName 
//_GetLongPathName

//extern DWORD (WINAPI *GetLongPathName)(LPCTSTR, LPTSTR, DWORD);

/*
* Exactly one file should define this symbol.
*/
#ifdef COMPILE_NEWAPIS_STUBS

/*
* The version to use if we are forced to emulate.
*/
DWORD WINAPI
Emulate_GetLongPathName(LPCTSTR ptszShort, LPTSTR ptszLong, DWORD ctchBuf)
{
LPSHELLFOLDER psfDesk;
HRESULT hr;
LPITEMIDLIST pidl;
TCHAR tsz[MAX_PATH]; /* Scratch TCHAR buffer */
DWORD dwRc;
LPMALLOC pMalloc;

/*
* The file had better exist. GetFileAttributes() will
* not only tell us, but it'll even call SetLastError()
* for us.
*/
if (GetFileAttributes(ptszShort) == 0xFFFFFFFF) {
return 0;
}

/*
* First convert from relative path to absolute path.
* This uses the scratch TCHAR buffer.
*/
dwRc = GetFullPathName(ptszShort, MAX_PATH, tsz, NULL);
if (dwRc == 0) {
/*
* Failed; GFPN already did SetLastError().
*/
} else if (dwRc >= MAX_PATH) {
/*
* Resulting path would be too long.
*/
SetLastError(ERROR_BUFFER_OVERFLOW);
dwRc = 0;
} else {
/*
* Just right.
*/
hr = SHGetDesktopFolder(&psfDesk);
if (SUCCEEDED(hr)) {
ULONG cwchEaten;

#ifdef UNICODE
#ifdef __cplusplus
hr = psfDesk->ParseDisplayName(NULL, NULL, tsz,
&cwchEaten, &pidl, NULL);
#else
hr = psfDesk->lpVtbl->ParseDisplayName(psfDesk, NULL, NULL, tsz,
&cwchEaten, &pidl, NULL);
#endif
#else
WCHAR wsz[MAX_PATH]; /* Scratch WCHAR buffer */

/*
* ParseDisplayName requires UNICODE, so we use
* the scratch WCHAR buffer during the conversion.
*/
dwRc = MultiByteToWideChar(
AreFileApisANSI() ? CP_ACP : CP_OEMCP,
0, tsz, -1, wsz, MAX_PATH);
if (dwRc == 0) {
/*
* Couldn't convert to UNICODE. MB2WC uses
* ERROR_INSUFFICIENT_BUFFER, which we convert
* to ERROR_BUFFER_OVERFLOW. Any other error
* we leave alone.
*/
if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
SetLastError(ERROR_BUFFER_OVERFLOW);
}
dwRc = 0;
} else {
#ifdef __cplusplus
hr = psfDesk->ParseDisplayName(NULL, NULL, wsz,
&cwchEaten, &pidl, NULL);
#else
hr = psfDesk->lpVtbl->ParseDisplayName(psfDesk, NULL, NULL,
wsz, &cwchEaten, &pidl, NULL);
#endif
#endif

if (FAILED(hr)) {
/*
* Weird. Convert the result back to a Win32
* error code if we can. Otherwise, use the
* generic "duh" error code ERROR_INVALID_DATA.
*/
if (HRESULT_FACILITY(hr) == FACILITY_WIN32) {
SetLastError(HRESULT_CODE(hr));
} else {
SetLastError(ERROR_INVALID_DATA);
}
dwRc = 0;
} else {
/*
* Convert the pidl back to a filename in the
* TCHAR scratch buffer.
*/
dwRc = SHGetPathFromIDList(pidl, tsz);
if (dwRc == 0 && tsz[0]) {
/*
* Bizarre failure.
*/
SetLastError(ERROR_INVALID_DATA);
} else {
/*
* Copy the result back to the user's buffer.
*/
dwRc = lstrlen(tsz);
if (dwRc + 1 > ctchBuf) {
/*
* On buffer overflow, return necessary
* size including terminating null (+1).
*/
SetLastError(ERROR_INSUFFICIENT_BUFFER);
dwRc = dwRc + 1;
} else {
/*
* On buffer okay, return actual size not
* including terminating null.
*/
lstrcpyn(ptszLong, tsz, ctchBuf);
}
}

/*
* Free the pidl.
*/
if (SUCCEEDED(SHGetMalloc(&pMalloc))) {
#ifdef __cplusplus
pMalloc->Free(pidl);
pMalloc->Release();
#else
pMalloc->lpVtbl->Free(pMalloc, pidl);
pMalloc->lpVtbl->Release(pMalloc);
#endif
}
}
#ifndef UNICODE
}
#endif
/*
* Release the desktop folder now that we no longer
* need it.
*/
#ifdef __cplusplus
psfDesk->Release();
#else
psfDesk->lpVtbl->Release(psfDesk);
#endif
}
}
return dwRc;
}
#endif
