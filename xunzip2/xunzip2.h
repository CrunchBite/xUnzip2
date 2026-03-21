//-------------------------------------------------------------------------------------------------
// File: xunzip2.h
//
// xUnzip2 public header - extract files from ZIP archives (file, memory, or XBE section).
//-------------------------------------------------------------------------------------------------

#ifndef __XUNZIP2_H__
#define __XUNZIP2_H__

/**
 * Progress callback type. Invoked once per archive entry (file or directory) before it is processed.
 * @param currentFile  1-based index of the current entry.
 * @param totalFiles   Total entry count (when callback is used; 0 if no callback).
 * @param currentFileName  Path/name of the current entry inside the zip.
 * @param userData     Value passed through from the extract call; use for context (e.g. UI handle).
 * @return true to continue extraction, false to cancel (extract returns false).
 */
typedef bool (*xunzip_progress_fn)(int currentFile, int totalFiles, const char* currentFileName, void* userData);

/**
 * Extract a ZIP from a file on disk.
 * @param pszSource             Path to the .zip file.
 * @param pszDestinationFolder   Folder to extract into (created if needed).
 * @param bUseFolderNames        true = keep paths from zip; false = flatten all files into destination.
 * @param bOverwrite             true = overwrite existing files (default).
 * @param bStripSingleRootFolder true = if zip has one top-level folder, extract its contents only (no root folder).
 * @param progressCallback       Optional; called per entry. Pass NULL to omit.
 * @param progressUserData       Passed to progressCallback; use NULL if not needed.
 * @return true on success, false on failure.
 */
bool xunzipFromFile(const char * pszSource, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite = true, const bool bStripSingleRootFolder = false, xunzip_progress_fn progressCallback = NULL, void* progressUserData = NULL);

/**
 * Extract a ZIP from a buffer in memory.
 * @param pData                  Pointer to the raw zip data.
 * @param iDataSize              Size in bytes of the zip data. [LARGE FILE CHANGE] was int, now __int64 to support buffers > 2GB.
 * @param pszDestinationFolder   Folder to extract into (created if needed).
 * @param bUseFolderNames        true = keep paths from zip; false = flatten all files into destination.
 * @param bOverwrite             true = overwrite existing files (default).
 * @param bStripSingleRootFolder true = if zip has one top-level folder, extract its contents only (no root folder).
 * @param progressCallback       Optional; called per entry. Pass NULL to omit.
 * @param progressUserData       Passed to progressCallback; use NULL if not needed.
 * @return true on success, false on failure.
 */
bool xunzipFromMemory(void *pData, __int64 iDataSize, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite = true, const bool bStripSingleRootFolder = false, xunzip_progress_fn progressCallback = NULL, void* progressUserData = NULL); // [LARGE FILE CHANGE] iDataSize was int, now __int64

/**
 * Extract a ZIP from an XBE section by name (e.g. embedded zip in the executable).
 * @param pszSectionName         Name of the XBE section containing the zip data.
 * @param pszDestinationFolder   Folder to extract into (created if needed).
 * @param bUseFolderNames        true = keep paths from zip; false = flatten all files into destination.
 * @param bOverwrite             true = overwrite existing files (default).
 * @param bStripSingleRootFolder true = if zip has one top-level folder, extract its contents only (no root folder).
 * @param progressCallback       Optional; called per entry. Pass NULL to omit.
 * @param progressUserData       Passed to progressCallback; use NULL if not needed.
 * @return true on success, false on failure (e.g. section not found).
 */
bool xunzipFromXBESection(const char * pszSectionName, const char * pszDestinationFolder, const bool bUseFolderNames, const bool bOverwrite = true, const bool bStripSingleRootFolder = false, xunzip_progress_fn progressCallback = NULL, void* progressUserData = NULL);

#endif // __XUNZIP2_H__