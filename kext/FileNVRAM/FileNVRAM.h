/***
 * FileNVRAM.h
 * FileNVRAM
 *
 * Created by Chris Morton on 1/11/13.
 * Copyright (c) 2013-2014 xZenue LLC. All rights reserved.
 *
 * This work is licensed under the
 * Creative Commons Attribution-NonCommercial 3.0 Unported License.
 * To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
 *
 * Updates:
 *
 *			- Reformatted source code (Pike R. Alpha, August 2015).
 *			- Xcode 7 compiler warnings fixed (Pike R. Alpha, August 2015).
 *			- Secured read/write_buffer routines (Pike R. Alpha, August 2015).
 *			- Compiler warnings fixed (Pike R. Alpha, August 2015).
 */

#ifndef FileNVRAM_FileNVRAM_h
#define FileNVRAM_FileNVRAM_h

#include <sys/proc.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/* This is needed for Info.plist to work properly */
#define FileNVRAM AppleNVRAM

#define HEADER	__FILE__ "[" TOSTRING(__LINE__) "]: "

#define DISABLED	0
#define ERROR		1
#define INFO		2
#define NOTICE		3

#define LOG(__level__, x...)																\
do {																						\
	if (mLoggingLevel >= __level__)															\
	{																						\
		char pname[256];																	\
		proc_name(proc_pid(vfs_context_proc(vfs_context_current())), pname, sizeof(pname));	\
		printf("Process: %s, ", pname);														\
		printf( HEADER x);																	\
	}																						\
} while(0)

#ifndef kIONVRAMDeletePropertyKey
#define kIONVRAMDeletePropertyKey	"IONVRAM-DELETE-PROPERTY"
#endif
#ifndef kIONVRAMSyncNowPropertyKey
#define kIONVRAMSyncNowPropertyKey	"IONVRAM-SYNCNOW-PROPERTY"
#endif
#ifndef kIODTNVRAMPanicInfoKey
#define kIODTNVRAMPanicInfoKey		"aapl,panic-info"
#endif

#define private public
#include <IOKit/IONVRAM.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOService.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOTimerEventSource.h>

#include "FileIO.h"

#define FILE_NVRAM_GUID			"D8F0CCF5-580E-4334-87B6-9FBBB831271D"

#define NVRAM_ENABLE_LOG		"EnableLogging"

#define NVRAM_SEPERATOR			":"
#define NVRAM_FILE_DT_LOCATION	"/chosen/nvram"
#define NVRAM_FILE_HEADER		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" \
								"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"\
								"<plist version=\"1.0\">\n"
#define NVRAM_FILE_FOOTER		"</plist>\n"

#define NVRAM_MISS_KEY			"NVRAM_MISS"
#define NVRAM_MISS_HEADER		"\n<key>NVRAM_MISS</key>\n"


#define kNVRAMSyncCommand		1
#define kNVRAMSetProperty		2
#define kNVRAMGetProperty		4

#define super IODTNVRAM

class FileNVRAM : public IODTNVRAM
{
	OSDeclareDefaultStructors(FileNVRAM);
	
public:
	virtual bool		start(IOService *provider) override;
	virtual void		stop(IOService *provider) override;

	virtual bool		passiveMatch(OSDictionary *matching, bool changesOK);

	virtual void		copyEntryProperties(const char* prefix, IORegistryEntry* entry);
	virtual void		copyUnserialzedData(const char* prefix, OSDictionary* dict);

	virtual IOReturn	syncOFVariables(void) override;
	virtual bool		init(IORegistryEntry *old, const IORegistryPlane *plane) override;

	virtual void		registerNVRAMController(IONVRAMController *nvram) override;
	virtual void		sync(void) override;
	virtual void		doSync(void);

	virtual bool		serializeProperties(OSSerialize *s) const override;

	virtual OSObject	*getProperty(const OSSymbol *aKey) const override;
	virtual OSObject	*copyProperty(const OSSymbol *aKey) const override;
	virtual OSObject	*getProperty(const char *aKey) const override;
	virtual OSObject	*copyProperty(const char *aKey) const override;

	virtual bool		setProperty(const OSSymbol *aKey, OSObject *anObject) override;
	virtual void		removeProperty(const OSSymbol *aKey) override;
	virtual IOReturn	setProperties(OSObject *properties) override;

	virtual IOReturn	readXPRAM(IOByteCount offset, UInt8 *buffer, IOByteCount length) override;
	virtual IOReturn	writeXPRAM(IOByteCount offset, UInt8 *buffer, IOByteCount length) override;

	virtual IOReturn	readNVRAMProperty(IORegistryEntry *entry, const OSSymbol **name, OSData **value) override;
	virtual IOReturn	writeNVRAMProperty(IORegistryEntry *entry, const OSSymbol *name, OSData *value) override;

	virtual OSDictionary *getNVRAMPartitions(void) override;
	
	virtual IOReturn	readNVRAMPartition(const OSSymbol *partitionID, IOByteCount offset, UInt8 *buffer, IOByteCount length) override;

	virtual IOReturn	writeNVRAMPartition(const OSSymbol *partitionID, IOByteCount offset, UInt8 *buffer, IOByteCount length) override;

	virtual IOByteCount	savePanicInfo(UInt8 *buffer, IOByteCount length) override;
	virtual bool		safeToSync(void) override;

	IOReturn			setPowerState (unsigned long whichState, IOService * whatDevice) override;

private:
	static void			timeoutOccurred(OSObject *target, IOTimerEventSource* timer);

	virtual void		registerNVRAM();
	virtual void		setPath(OSString* path);

	virtual OSObject	*cast(const OSSymbol* key, OSObject* obj);

	static IOReturn		dispatchCommand(OSObject* owner, void* arg0, void* arg1, void* arg2, void* arg3);

	bool				mInitComplete;
	bool				mSafeToSync;

	UInt8				mLoggingLevel;

	vfs_context_t		mCtx;

	OSDictionary		*mNvramMissDict;
	IOCommandGate		*mCommandGate;
	OSString			*mFilePath;

	IOTimerEventSource	*mTimer;
};

#endif /* FileNVRAM_FileNVRAM_h */
