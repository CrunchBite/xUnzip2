//-------------------------------------------------------------------------------------------------
// File: xunzip-testapp.cpp
//
// Example usage of xUnzip2
//-------------------------------------------------------------------------------------------------

// Where should the zip contents be extracted? (Z:\ is the cache partition)
#define OUTPUT_DIR "Z:\\temp\\"

// Where should the temporary zip file be stored?
#define TEMPZIP_FILENAME "Z:\\temp.zip"


#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include "xunzip2.h"
#include <sys/stat.h>

// Function prototypes as we don't have a header
void unpackPayload();
void mountAllDrives();

VOID __cdecl main() {
	bool ret = false;

	// Uncomment this if you want to have a clean cache drive (Z:\) to use
	//!XMountUtilityDrive(true);
	
	// Mount all drives
	// The way this is done is a simple 'jailbreak' out of the XDK's usual constraints on disk access.
	// The hard drive partitions will all be mounted using the usual scene drive conventions.
	// C: is where the dashboard lives (usually)
	// E: has gamesaves and DLC
	// F: and G: are extra partitions
	// Z:/Y:/Z: are the cache partitions.
	// If you want to be sure you have enough space available for your payload to unpack uncomment the code block above to have a clean Z:\ drive and don't call mountAllDrives()
	mountAllDrives();

	// START xunzipFromFile TEST

	// Unpack the embedded zip file to TEMPZIP_FILENAME
	unpackPayload();

	// Extract the zip file
	ret = xunzipFromFile(TEMPZIP_FILENAME, OUTPUT_DIR, true);
	__debugbreak(); // Check the ret value and the cache drive contents

	// END xunzipFromFile TEST


	// START xunzipFromMemory TEST

	HANDLE sectionHandle = XGetSectionHandle("testzip");
	if (sectionHandle == INVALID_HANDLE_VALUE) {
		return;
	}
	DWORD sectionSize = XGetSectionSize(sectionHandle);
	PVOID mem = XLoadSectionByHandle(sectionHandle);

	ret = xunzipFromMemory(mem, sectionSize, OUTPUT_DIR, true);
	__debugbreak(); // Check the ret value and the cache drive contents

	// END xunzipFromMemory TEST


	// START xunzipFromXBESection TEST

	ret = xunzipFromXBESection("testzip", OUTPUT_DIR, true);
	__debugbreak(); // Check the ret value

	// END xunzipFromXBESection TEST

	__debugbreak(); // Did you check the ret value and the cache drive contents?
	return;
}

// Unpack the embedded zip to the location specified by TEMPZIP_FILENAME
void unpackPayload() {
	HANDLE sectionHandle = XGetSectionHandle("testzip");
	if (sectionHandle == INVALID_HANDLE_VALUE) {
		return;
	}
	DWORD sectionSize = XGetSectionSize(sectionHandle);

	// Check if file already exists
	HANDLE existingFile = CreateFile(TEMPZIP_FILENAME, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (existingFile != INVALID_HANDLE_VALUE) {
		// Is the file on disk the same size as what we would write?
		DWORD dwSize = GetFileSize(existingFile, NULL);
		if (dwSize == sectionSize) {
			// Size matches, assume we're good to leave it alone
			// This would be a good place to do a file checksum if you wanted to really be sure
			CloseHandle(existingFile);
			return;
		}
	}
	CloseHandle(existingFile);

	// Write the file to disk
	DWORD bytesWritten;
	PVOID mem = XLoadSectionByHandle(sectionHandle);
	HANDLE file = CreateFile(TEMPZIP_FILENAME, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(file, mem, sectionSize, &bytesWritten, NULL);
	CloseHandle(file);
	XFreeSectionByHandle(sectionHandle);
}

// Helper functions
// Everything below here is used for hard drive access

typedef struct _STRING {
	USHORT	Length;
	USHORT	MaximumLength;
	PSTR	Buffer;
} UNICODE_STRING, *PUNICODE_STRING, ANSI_STRING, *PANSI_STRING;
extern "C" { 
	XBOXAPI LONG WINAPI IoCreateSymbolicLink(IN PUNICODE_STRING SymbolicLinkName,IN PUNICODE_STRING DeviceName);
	XBOXAPI LONG WINAPI IoDeleteSymbolicLink(IN PUNICODE_STRING SymbolicLinkName);
}
struct pathconv_s {
	char * DriveLetter;
	char * FullPath;
} pathconv_table[] = {
	{ "DVD:", "\\Device\\Cdrom0" },// Can't use D:
	{ "C:", "\\Device\\Harddisk0\\Partition2" },
	{ "E:", "\\Device\\Harddisk0\\Partition1" },
	{ "F:", "\\Device\\Harddisk0\\Partition6" },
	{ "G:", "\\Device\\Harddisk0\\Partition7" },
	{ "X:", "\\Device\\Harddisk0\\Partition3" },
	{ "Y:", "\\Device\\Harddisk0\\Partition4" },
	{ "Z:", "\\Device\\Harddisk0\\Partition5" },
	{ NULL, NULL }
};
#define DeviceC "\\Device\\Harddisk0\\Partition2"
#define DeviceE "\\Device\\Harddisk0\\Partition1"
#define CdRom "\\Device\\Cdrom0"
#define DeviceX "\\Device\\Harddisk0\\Partition3"
#define DeviceY "\\Device\\Harddisk0\\Partition4"
#define DeviceZ "\\Device\\Harddisk0\\Partition5"
#define DeviceF "\\Device\\Harddisk0\\Partition6"
#define DeviceG "\\Device\\Harddisk0\\Partition7"
#define DriveC "\\??\\C:"
#define DriveD "\\??\\D:"
#define DriveE "\\??\\E:"
#define DriveF "\\??\\F:"
#define DriveG "\\??\\G:"
#define DriveX "\\??\\X:"
#define DriveY "\\??\\Y:"
#define DriveZ "\\??\\Z:"
LONG MountDevice(LPSTR sSymbolicLinkName, char *sDeviceName) {
	UNICODE_STRING 	deviceName;
	deviceName.Buffer  = (PSTR)sDeviceName;
	deviceName.Length = (USHORT)strlen(sDeviceName);
	deviceName.MaximumLength = (USHORT)strlen(sDeviceName) + 1;
	UNICODE_STRING 	symbolicLinkName;
	symbolicLinkName.Buffer  = sSymbolicLinkName;
	symbolicLinkName.Length = (USHORT)strlen(sSymbolicLinkName);
	symbolicLinkName.MaximumLength = (USHORT)strlen(sSymbolicLinkName) + 1;
	return IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
}
LONG UnMountDevice(LPSTR sSymbolicLinkName) {
	UNICODE_STRING 	symbolicLinkName;
	symbolicLinkName.Buffer  = sSymbolicLinkName;
	symbolicLinkName.Length = (USHORT)strlen(sSymbolicLinkName);
	symbolicLinkName.MaximumLength = (USHORT)strlen(sSymbolicLinkName) + 1;
	return IoDeleteSymbolicLink(&symbolicLinkName);
}
void mountAllDrives() {
	UnMountDevice(DriveX);
	UnMountDevice(DriveY);
	UnMountDevice(DriveZ);
	UnMountDevice(DriveC);
	UnMountDevice(DriveE);
	UnMountDevice(DriveF);
	UnMountDevice(DriveG);
	MountDevice(DriveX, DeviceX);
	MountDevice(DriveY, DeviceY);
	MountDevice(DriveZ, DeviceZ);
	MountDevice(DriveC, DeviceC);
	MountDevice(DriveE, DeviceE);
	MountDevice(DriveF, DeviceF);
	MountDevice(DriveG, DeviceG);
}
