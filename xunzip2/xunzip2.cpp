//-------------------------------------------------------------------------------------------------
// File: xunzip2.cpp
//
// Public functions of xUnzip2
//-------------------------------------------------------------------------------------------------

#include <xtl.h>
#include "xunzip2.h"
#include "ziparchive.h"

// Extract from a zip file on the filesystem
bool xunzipFromZipFile(const char * pszSource, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback, void* progressUserData) {
	CZipArchive cZip;
	return cZip.ExtractFromZipFile(pszSource, pszDestinationFolder, bUseFolderNames, bOverwrite, bStripSingleRootFolder, progressCallback, progressUserData);
}

// Extract from address in memory
bool xunzipFromMemory(void *pData, int iDataSize, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback, void* progressUserData) {
	CZipArchive cZip;
	return cZip.ExtractFromMemory((uint8_t *)pData, iDataSize, pszDestinationFolder, bUseFolderNames, bOverwrite, bStripSingleRootFolder, progressCallback, progressUserData);
}

// Extract from XBE section by name
bool xunzipFromXBESection(const char * pszSectionName, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback, void* progressUserData) {
    HANDLE sectionHandle = XGetSectionHandle(pszSectionName);
    if (sectionHandle == INVALID_HANDLE_VALUE) {
        return false;
	}
	DWORD sectionSize = XGetSectionSize(sectionHandle);
	PVOID mem = XLoadSectionByHandle(sectionHandle);

	return xunzipFromMemory(mem, sectionSize, pszDestinationFolder, bUseFolderNames, bOverwrite, bStripSingleRootFolder, progressCallback, progressUserData);
}
