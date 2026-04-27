#ifndef __ZIPARCHIVE_H__
#define __ZIPARCHIVE_H__

#include <string>
#include <unzipLIB.h>
#include "xunzip2.h"

class CZipArchive {
    public:
					CZipArchive(void);
					~CZipArchive(void);
	
	bool			ExtractFromFile(const char * pszSource, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback = NULL, void* progressUserData = NULL);
	bool			ExtractFromMemory(uint8_t *pData, int iDataSize, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback = NULL, void* progressUserData = NULL);

private:

	int				ExtractZip(UNZIP* zip, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite, const bool bStripSingleRootFolder, xunzip_progress_fn progressCallback = NULL, void* progressUserData = NULL);
	int				ExtractCurrentFile(UNZIP* zip, const char * pszDestinationFolder, const bool bUseFolderNames, bool bOverwrite, const char* pszStripRootPrefix);

	void		  * m_pUnZipBuffer;
	unsigned int	m_uiUnZipBufferSize;
};
#endif // __ZIPARCHIVE_H__
