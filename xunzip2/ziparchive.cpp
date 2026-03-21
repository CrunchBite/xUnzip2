#include <xtl.h>
#include <stdio.h>

#include <malloc.h>
#include <sstream>
#include <fstream>
#include <conio.h>
#include <limits.h>
#include <locale>
#include <iostream>

using namespace std;

#include "ziparchive.h"

// Callbacks
void *  zipFile_Open(const char *filename, __int64 *size); // [LARGE FILE CHANGE] size was int32_t*
void    zipFile_Close(void *p);
int32_t zipFile_Read(void *p, uint8_t *buffer, int32_t length);
__int64 zipFile_Seek(void *p, __int64 position, int iType); // [LARGE FILE CHANGE] was int32_t position/return

// [LARGE FILE CHANGE] XboxFileHandle replaces bare FILE* in the callbacks.
// The XDK CRT does not provide _get_osfhandle, _fseeki64 or _ftelli64, so we
// bypass the CRT file layer entirely and use a Win32 HANDLE directly.
// SetFilePointer with a LARGE_INTEGER gives us full 64-bit seek support.
struct XboxFileHandle
{
    HANDLE  hFile;
    __int64 iSize;
};

// 64-bit seek helper using Win32 SetFilePointer + LARGE_INTEGER
static __int64 handle_seek64(HANDLE hFile, __int64 offset, DWORD dwMoveMethod)
{
    LARGE_INTEGER li;
    li.QuadPart = offset;
    li.LowPart  = SetFilePointer(hFile, li.LowPart, &li.HighPart, dwMoveMethod);
    if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        li.QuadPart = -1;
    return li.QuadPart;
}


char *strrepl(char *Str, size_t BufSiz, char *OldStr, char *NewStr) {
    int OldLen, NewLen;
    char *p, *q;

	if (NULL == (p = strstr(Str, OldStr))) {
        return Str;
	}

    OldLen = strlen(OldStr);
    NewLen = strlen(NewStr);

	if ((strlen(Str) + NewLen - OldLen + 1) > BufSiz) {
        return NULL;
	}

    memmove(q = p + NewLen, p + OldLen, strlen(p + OldLen)+1);
    memcpy(p, NewStr, NewLen);
    return q;
}

char *strreplall(char *Str, size_t BufSiz, char *OldStr, char *NewStr) {
	char *ret;
	size_t i;

	for (i = 0; i < BufSiz; i++) {
		ret = strrepl(Str, BufSiz, OldStr, NewStr);
	}

	return ret;
}

CZipArchive::CZipArchive(void) {
	m_pUnZipBuffer		= (void*)NULL;
	m_uiUnZipBufferSize	= 131072; // amount to malloc
}

CZipArchive::~CZipArchive(void) {
	if (m_pUnZipBuffer != (void*)NULL) {
		free(m_pUnZipBuffer);
		m_pUnZipBuffer = (void*)NULL;
	}
}

// Public
bool CZipArchive::ExtractFromFile(const char * pszSource, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback, void* progressUserData) {
	UNZIP zip;
    int rc;
	
	rc = zip.openZIP(pszSource, zipFile_Open, zipFile_Close, zipFile_Read, zipFile_Seek);
	if (rc != UNZ_OK) {
		zip.closeZIP();
		return false;
	}

	rc = ExtractZip(&zip, pszDestinationFolder, bUseFolderNames, bOverwrite, bStripSingleRootFolder, progressCallback, progressUserData);

	zip.closeZIP();
	return (rc == UNZ_OK || rc == UNZ_END_OF_LIST_OF_FILE);
}

// Public
// [LARGE FILE CHANGE] iDataSize was int, now __int64 to support buffers > 2GB
bool CZipArchive::ExtractFromMemory(uint8_t *pData, __int64 iDataSize, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback, void* progressUserData) {
	UNZIP zip;
	int rc;

	rc = zip.openZIP(pData, (uint64_t)iDataSize);
	if (rc != UNZ_OK) {
		zip.closeZIP();
		return false;
	}

	rc = ExtractZip(&zip, pszDestinationFolder, bUseFolderNames, bOverwrite, bStripSingleRootFolder, progressCallback, progressUserData);

	zip.closeZIP();
	return (rc == UNZ_OK || rc == UNZ_END_OF_LIST_OF_FILE);
}

int CZipArchive::ExtractZip(UNZIP* zip, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback, void* progressUserData) {
	char szComment[256], szName[256];
	char firstRoot[260] = {0};
	char pathCopy[1024];
	unz_file_info fi;
	int rc;
	int currentFileIndex = 0;
	int totalFileCount = 0;
	const char* pszStripPrefix = NULL;

	// Use global comment as a zip sanity check
	rc = zip->getGlobalComment(szComment, sizeof(szComment));
	if (rc != UNZ_OK) {
		return rc;
	}

	// If strip single root requested, first pass to detect single root folder name (and count when progress callback set)
	if (bStripSingleRootFolder) {
		zip->gotoFirstFile();
		rc = UNZ_OK;
		while (rc == UNZ_OK) {
			rc = zip->getFileInfo(&fi, szName, sizeof(szName), NULL, 0, szComment, sizeof(szComment));
			if (rc != UNZ_OK) break;
			if (progressCallback != NULL) totalFileCount++;
			strncpy(pathCopy, szName, sizeof(pathCopy) - 1);
			pathCopy[sizeof(pathCopy) - 1] = '\0';
			strreplall(pathCopy, sizeof(pathCopy), (char*)"/", (char*)"\\");
			char* p = strchr(pathCopy, '\\');
			size_t len = p ? (size_t)(p - pathCopy) : strlen(pathCopy);
			if (len > 0 && len < sizeof(firstRoot)) {
				if (firstRoot[0] == '\0') {
					strncpy(firstRoot, pathCopy, len);
					firstRoot[len] = '\0';
				} else if (len != strlen(firstRoot) || strncmp(firstRoot, pathCopy, len) != 0) {
					firstRoot[0] = '\0';
				}
			}
			rc = zip->gotoNextFile();
		}
		pszStripPrefix = (firstRoot[0] != '\0') ? firstRoot : NULL;
		zip->gotoFirstFile();
		rc = UNZ_OK;
	}
	// If progress callback set but we didn't count yet, do a count-only pass
	else if (progressCallback != NULL) {
		zip->gotoFirstFile();
		rc = UNZ_OK;
		while (rc == UNZ_OK) {
			rc = zip->getFileInfo(&fi, szName, sizeof(szName), NULL, 0, szComment, sizeof(szComment));
			if (rc != UNZ_OK) break;
			totalFileCount++;
			rc = zip->gotoNextFile();
		}
		zip->gotoFirstFile();
		rc = UNZ_OK;
	}

	// Ensure that the destination root folder exists
	CreateDirectory(pszDestinationFolder, NULL);

	// Loop through all files
	while (rc == UNZ_OK) {
		// File record ok?
		rc = zip->getFileInfo(&fi, szName, sizeof(szName), NULL, 0, szComment, sizeof(szComment));
		if (rc == UNZ_OK) {
			currentFileIndex++;
			if (progressCallback != NULL) {
				if (!progressCallback(currentFileIndex, totalFileCount, szName, progressUserData)) {
					rc = UNZ_PARAMERROR;  /* cancelled by user */
					break;
				}
			}
			// Extract the current file
			if ((rc = ExtractCurrentFile(zip, pszDestinationFolder, bUseFolderNames, bOverwrite, pszStripPrefix)) != UNZ_OK) {
				// Error during extraction of file. Break out of loop
				break;
			}
		}
		rc = zip->gotoNextFile();
	}

	// Return status
	return rc;
}

int CZipArchive::ExtractCurrentFile(UNZIP* zip, const char * pszDestinationFolder, const bool bUseFolderNames, bool bOverwrite, const char* pszStripRootPrefix) {
	char szComment[256] = {0};
	char szFileName_InZip[1024] = {0};
	char szBuffer[1024];
	unz_file_info fi;
	char szPathSep[2];
	char * pszFileName_WithOutPath;
	char * pszPos;
	int rc;
	char * pszWriteFileName;
	char chHold;
	bool bSkip = false;
	HANDLE hFile;
	DWORD dwBytesWritten = 0;

	// Check if the destination folder ends with an '\\'
	if (*(pszDestinationFolder + strlen(pszDestinationFolder) - 1) == '\\') {
		// Use no separator
		*szPathSep = '\0';
	}
	else {
		// Use path separator
		strcpy(szPathSep, "\\");
	}

	// Get information about the current file
	rc = zip->getFileInfo(&fi, szBuffer, sizeof(szBuffer), NULL, 0, szComment, sizeof(szComment));
	if (rc != UNZ_OK) {
		return rc;
	}

	// Substitute '/' with '\'
	strreplall(szBuffer, 1024, "/", "\\");

	// Don't include the drive letter (if present) and the leading '\' (if present)
	if (szBuffer[1] == ':' && szBuffer[2] == '\\') {
		// Copy file name
		strcpy(szFileName_InZip, (szBuffer + 3));
	}
	else if (szBuffer[1] == ':') {
		strcpy(szFileName_InZip, (szBuffer + 2));
	}
	else if (szBuffer[0] == '\\') {
		strcpy(szFileName_InZip, (szBuffer + 1));
	}
	else {
		strcpy(szFileName_InZip, szBuffer);
	}

	// Strip single root prefix if requested (e.g. "MyApp\sub\file.txt" -> "sub\file.txt")
	if (pszStripRootPrefix && pszStripRootPrefix[0] != '\0') {
		size_t prefixLen = strlen(pszStripRootPrefix);
		if (strncmp(szFileName_InZip, pszStripRootPrefix, prefixLen) == 0) {
			char c = szFileName_InZip[prefixLen];
			if (c == '\\' || c == '/' || c == '\0') {
				size_t skip = prefixLen + (c ? 1 : 0);
				memmove(szFileName_InZip, szFileName_InZip + skip, strlen(szFileName_InZip + skip) + 1);
			}
		}
	}

	// Set reference
	pszPos = (char*)pszFileName_WithOutPath = (char*)szFileName_InZip;

	// Find filename part (without the path)
	while ((*pszPos) != '\0') {
		if (((*pszPos) == '/') || ((*pszPos) == '\\')) {
			// Set reference
			pszFileName_WithOutPath = (char*)(pszPos + 1);
		}

		// Increment position
		pszPos++;
	}

	// Is this a folder?
	if ((*pszFileName_WithOutPath) == '\0') {
		// Use folder names?
		if (bUseFolderNames) {
			// Compose file name
			sprintf(szBuffer, "%s%s%s", pszDestinationFolder, szPathSep, szFileName_InZip);

			// Substitute '/' with '\'
			strreplall(szBuffer, 1024, "/", "\\");

			// Create folder
			CreateDirectory(szBuffer, NULL);
		}

		// Return OK
		return UNZ_OK;
	}

	// Do we have a buffer?
	if (m_pUnZipBuffer == (void*)NULL) {
		// Allocate buffer
		if ((m_pUnZipBuffer = (void*)malloc(m_uiUnZipBufferSize)) == (void*)NULL) {
			// Return not OK
			return UNZ_INTERNALERROR;
		}
	}

	// Use folder names?
	if (bUseFolderNames) {
		// Use total file name
		pszWriteFileName = szFileName_InZip;
	}
	else {
		// Use file name only
		pszWriteFileName = pszFileName_WithOutPath;
	}

	// Open the current file
	if ((rc = zip->openCurrentFile()) != UNZ_OK) {
		return rc;
	}

	// Compose file name
	sprintf(szBuffer, "%s%s%s", pszDestinationFolder, szPathSep, pszWriteFileName);

	// Check if file exists?
	if (!bOverwrite && rc == UNZ_OK) {
		// Open the local file
		hFile = CreateFile(szBuffer, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		// Check handle
		if (hFile != (HANDLE)INVALID_HANDLE_VALUE) {
			// File exists but don't overwrite. Close file
			CloseHandle(hFile);

			// Skip this file
			bSkip = true;
		}
	}

	// Skip this file?
	if (!bSkip && rc == UNZ_OK) {
		// Create the file
		hFile = CreateFile(szBuffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		// Check handle
		if (hFile == (HANDLE)INVALID_HANDLE_VALUE) {
			// File not created. Some zipfiles doesn't contain
			// folder alone before file
			if (bUseFolderNames && pszFileName_WithOutPath != (char*)szFileName_InZip) {
				// Store character
				chHold = *(pszFileName_WithOutPath - 1);

				// Terminate string
				*(pszFileName_WithOutPath - 1) = '\0';

				// Compose folder name
				sprintf(szBuffer, "%s%s%s", pszDestinationFolder, szPathSep, pszWriteFileName);

				// Create folder
				CreateDirectory(szBuffer, NULL);

				// Restore file name
				*(pszFileName_WithOutPath - 1) = chHold;

				// Compose folder name
				sprintf(szBuffer, "%s%s%s", pszDestinationFolder, szPathSep, pszWriteFileName);

				// Try to create the file
				hFile = CreateFile(szBuffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			}
		}   

		// Check handle
		if (hFile == (HANDLE)INVALID_HANDLE_VALUE) {
			// Return not OK
			return UNZ_ERRNO;
		}
	}

	// Check handle
	if (hFile != (HANDLE)INVALID_HANDLE_VALUE) {
		do {
			// Read the current file
			if ((rc = zip->readCurrentFile((uint8_t*)m_pUnZipBuffer, m_uiUnZipBufferSize)) < 0) {
				// Error reading zip file
				// Break out of loop
				break;
			}

			// Check return code
			if (rc > 0) {
				// Write to file
				if (WriteFile(hFile, m_pUnZipBuffer, (DWORD)rc, &dwBytesWritten, NULL) == false) {
					// Error during write of file

					// Set return status
					rc = UNZ_ERRNO;

					// Break out of loop
					break;
				}
			}
		}
		while (rc > 0);

		// Close file
		CloseHandle(hFile);
	}

	if (rc == UNZ_OK) {
		// Close current file
		rc = zip->closeCurrentFile();
	}
	else {
		// Close current file (don't lose the error)
		zip->closeCurrentFile();
	}

	// Return status
	return rc;
}

// Callback functions needed by the unzipLIB to access the filesystem
// [LARGE FILE CHANGE] All callbacks now use Win32 HANDLE via XboxFileHandle instead of
// a CRT FILE*. This is required because the XDK CRT does not provide _get_osfhandle,
// _fseeki64 or _ftelli64. Using HANDLE + SetFilePointer gives us full 64-bit seek support.

void * zipFile_Open(const char *filename, __int64 *size) {
	HANDLE hFile = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
	                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		*size = 0;
		return NULL;
	}

	// GetFileSize fills the high DWORD so we get the full 64-bit size
	DWORD dwHigh = 0;
	DWORD dwLow  = GetFileSize(hFile, &dwHigh);
	if (dwLow == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
		CloseHandle(hFile);
		*size = 0;
		return NULL;
	}

	XboxFileHandle *pxfh = (XboxFileHandle*)malloc(sizeof(XboxFileHandle));
	if (!pxfh) {
		CloseHandle(hFile);
		*size = 0;
		return NULL;
	}

	pxfh->hFile = hFile;
	pxfh->iSize = ((__int64)dwHigh << 32) | (__int64)dwLow;
	*size       = pxfh->iSize;
	return (void*)pxfh;
}

void zipFile_Close(void *p) {
	ZIPFILE *pzf = (ZIPFILE *)p;
	XboxFileHandle *pxfh = (XboxFileHandle *)pzf->fHandle;
	if (pxfh) {
		if (pxfh->hFile != INVALID_HANDLE_VALUE)
			CloseHandle(pxfh->hFile);
		free(pxfh);
		pzf->fHandle = NULL;
	}
}

int32_t zipFile_Read(void *p, uint8_t *buffer, int32_t length) {
	ZIPFILE *pzf = (ZIPFILE *)p;
	XboxFileHandle *pxfh = (XboxFileHandle *)pzf->fHandle;
	DWORD dwRead = 0;
	if (!ReadFile(pxfh->hFile, buffer, (DWORD)length, &dwRead, NULL))
		return -1;
	return (int32_t)dwRead;
}

__int64 zipFile_Seek(void *p, __int64 position, int iType) {
	ZIPFILE *pzf = (ZIPFILE *)p;
	XboxFileHandle *pxfh = (XboxFileHandle *)pzf->fHandle;

	DWORD dwMoveMethod;
	if      (iType == SEEK_SET) dwMoveMethod = FILE_BEGIN;
	else if (iType == SEEK_END) dwMoveMethod = FILE_END;
	else                        dwMoveMethod = FILE_CURRENT;

	return handle_seek64(pxfh->hFile, position, dwMoveMethod);
}