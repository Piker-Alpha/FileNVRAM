/***
 * FileNVRAM.cpp
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
 *			- Moved read_buffer/write_buffer from FileIO.c to FileNVRAM.cpp (Pike R. Alpha, August 2015).
 *			- Removed FileIO.[c/h] and Support.h (Pike R. Alpha, August 2015).
 *			- Add entitlement check (Pike R. Alpha September 2015).
 */

#include "FileNVRAM.h"

#include <IOKit/IOUserClient.h>
#include <libkern/c++/OSUnserialize.h>

/** The cpp file is included here to hide symbol names. **/
#include "Support.cpp"

/** Private Macros **/

/** Private variables **/
#define kIOPMPowerOff		0
#define POWER_STATE_OFF		0
#define POWER_STATE_ON		1

static IOPMPowerState sPowerStates[] =
{
	{1, kIOPMPowerOff, kIOPMPowerOff, kIOPMPowerOff, 0, 0, 0, 0, 0, 0, 0, 0},
	{1, kIOPMPowerOn,  kIOPMPowerOn,  kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
};

/** Private Functions **/


OSDefineMetaClassAndStructors(FileNVRAM, IODTNVRAM);

//==============================================================================

bool FileNVRAM::start(IOService *provider)
{
	bool earlyInit = false;

	LOG(NOTICE, "start() called (%d)\n", mInitComplete);

	//start is called upon wake for some reason.
	if (mInitComplete)
	{
		return true;
	}

	if (!super::start(provider))
	{
		return false;
	}
	
	LOG(NOTICE, "start() called (%d)\n", mInitComplete);

	// mFilePath		= NULL;			// no know file
	mLoggingLevel   = NOTICE;		// start with logging disabled, can be update for debug
	mInitComplete   = false;		// Don't resync anything that's already in the file system.
	mSafeToSync     = false;		// Don't sync untill later

	// We should be root right now... cache this for later.
	mCtx            = vfs_context_current();

	// Register Power modes
	PMinit();
	registerPowerDriver(this, sPowerStates, sizeof(sPowerStates) / sizeof(IOPMPowerState));
	provider->joinPMtree(this);

	IORegistryEntry* bootnvram = IORegistryEntry::fromPath(NVRAM_FILE_DT_LOCATION, gIODTPlane);
	IORegistryEntry* root = IORegistryEntry::fromPath("/", gIODTPlane);

	// Create the command gate.
	mCommandGate = IOCommandGate::commandGate( this, dispatchCommand);
	getWorkLoop()->addEventSource( mCommandGate );

	// Replace the IOService dicionary with an empty one, clean out variables we don't want.
	OSDictionary* dict = OSDictionary::withCapacity(1);

	if (!dict)
	{
		return false;
	}

	setPropertyTable(dict);

	if (bootnvram)
	{
		copyEntryProperties(NULL, bootnvram);
		bootnvram->detachFromParent(root, gIODTPlane);
	}
	else
	{
		IOTimerEventSource* mTimer = IOTimerEventSource::timerEventSource(this, timeoutOccurred);

		if (mTimer)
		{
			getWorkLoop()->addEventSource( mTimer);
			mTimer->setTimeoutMS(50); // callback isn't being setup right, causes a panic
			mSafeToSync = false;
		}
		else
		{
			earlyInit = true;
		}
	}

	// We don't have initial NVRAM data from the bootloader, or we couldn't schedule a
	// timer to read in /Extra/NVRAM/nvram.plist, so start up immediately.
	if (earlyInit == true)
	{
		mSafeToSync = true;
		registerNVRAM();
	}

	mInitComplete = true;

	return true;
}

//==============================================================================

void FileNVRAM::registerNVRAM()
{
	// Create entry in device tree -> IODeviceTree:/options
	setName("AppleEFINVRAM");
	setName("options", gIODTPlane);
	IORegistryEntry* root = IORegistryEntry::fromPath("/", gIODTPlane);
	attachToParent(root, gIODTPlane);
	registerService();

	// Register with the platform expert
	const OSSymbol* funcSym = OSSymbol::withCString("RegisterNVRAM");

	if (funcSym)
	{
		callPlatformFunction(funcSym, false, this, NULL, NULL, NULL);
		funcSym->release();
	}
}

//==============================================================================

void FileNVRAM::stop(IOService *provider)
{
	// OSSafeReleaseNULL(mFilePath);

	if (mTimer)
	{
		mTimer->cancelTimeout();
		getWorkLoop()->removeEventSource(mTimer);
		OSSafeReleaseNULL(mTimer);
	}

	if (mCommandGate)
	{
		getWorkLoop()->removeEventSource(mCommandGate);
	}

	PMstop();
	LOG(NOTICE, "Stop called, attempting to detachFromParent\n");

	IORegistryEntry* root = IORegistryEntry::fromPath("/", gIODTPlane);
	detachFromParent(root, gIODTPlane);

	LOG(NOTICE, "Stop has passed the detach point.. move along now\n");
}

//==============================================================================

void FileNVRAM::copyUnserialzedData(const char* prefix, OSDictionary* dict)
{
	const OSSymbol* key;

	if (!dict)
	{
		return;
	}
	
	OSCollectionIterator * 	iter = OSCollectionIterator::withCollection(dict);

	if (!iter)
	{
		return;
	}
	
	if (!prefix)
	{
		LOG(INFO, "Restoring nvram data from file.\n");
	}

	do
	{
		key = (const OSSymbol *)iter->getNextObject();

		if (key)
		{
			const char* name = key->getCStringNoCopy();
			OSObject* object = dict->getObject(name);

			if (prefix)
			{
				size_t size = strlen(prefix) + sizeof(NVRAM_SEPERATOR) + strlen(key->getCStringNoCopy());
				char* newKey = (char*)IOMalloc(size);

				snprintf(newKey, size, "%s%s%s", prefix, NVRAM_SEPERATOR, key->getCStringNoCopy());

				setProperty(OSSymbol::withCString(newKey), object);

				IOFree(newKey, size);
			}
			else
			{
				OSDictionary* subdict;

				if ((subdict = OSDynamicCast(OSDictionary, object)))
				{
					// Guid
					copyUnserialzedData(name, subdict);
				}
				else
				{
					setProperty(key, object);
				}
			}
		}
	} while(key);

	if (!prefix)
	{
		LOG(INFO, "nvram data restored.\n");
	}

	iter->release();
}

//==============================================================================

void FileNVRAM::copyEntryProperties(const char* prefix, IORegistryEntry* entry)
{
	IORegistryEntry* child;
	OSDictionary* properties;
	OSCollectionIterator *iter;

	if (entry)
	{
		// Parse all IORegistery Children
		OSIterator * iterator = entry->getChildIterator(gIODTPlane);

		if (iterator)
		{
			while((child = OSDynamicCast(IORegistryEntry, iterator->getNextObject())) != NULL)
			{
				const char* name = child->getName();

				if (prefix)
				{
					size_t size = strlen(prefix) + sizeof(NVRAM_SEPERATOR) + strlen(name);
					char* newPrefix = (char*)IOMalloc(size);

					snprintf(newPrefix, size, "%s%s%s", prefix, NVRAM_SEPERATOR, name);
					copyEntryProperties(newPrefix, child);

					IOFree(newPrefix, size);
				}
				else
				{
					copyEntryProperties(name, child);
				}
			}

			iterator->release();
		}

		// Parse entry properties and add them to our self
		properties = entry->dictionaryWithProperties();

		bool                 result = true;
		OSObject             *object;
		const OSSymbol       *key;

		iter = OSCollectionIterator::withCollection(properties);

		if (iter == 0)
		{
			return;
		}
	
		while (result)
		{

			key = OSDynamicCast(OSSymbol, iter->getNextObject());

			if (key == 0)
			{
				break;
			}
			
			if (key->isEqualTo("name"))
			{
				continue; // Special property in IORegistery, ignore
			}
			
			object = properties->getObject(key);

			if (object == 0)
			{
				continue;
			}
			
			if (prefix)
			{
				size_t size = strlen(prefix) + sizeof(NVRAM_SEPERATOR) + strlen(key->getCStringNoCopy());
				char* newKey = (char*)IOMalloc(size);

				snprintf(newKey, size, "%s%s%s", prefix, NVRAM_SEPERATOR, key->getCStringNoCopy());

				setProperty(OSSymbol::withCString(newKey), object);

				IOFree(newKey, size);
			}
			else
			{
				setProperty(key, object);
			}
		}

		iter->release();
	}
}

//==============================================================================

bool FileNVRAM::init(IORegistryEntry *old, const IORegistryPlane *plane)
{
	LOG(NOTICE, "init(%p, %p) called\n", old, plane);

	return IOService::init(old,plane);
}

//==============================================================================

bool FileNVRAM::passiveMatch (OSDictionary *matching, bool changesOK)
{
	OSString *str = OSDynamicCast(OSString, matching->getObject(gIOProviderClassKey));

	if (str)
	{
		LOG(NOTICE, "passiveMatch(%s) called\n", str->getCStringNoCopy());
	}
	
	if (str && str->isEqualTo("AppleEFINVRAM"))
	{
		return true;
	}

	return super::passiveMatch (matching, changesOK);
}

//==============================================================================

IOReturn FileNVRAM::syncOFVariables(void)
{
	LOG(NOTICE, "syncOFVariables() called\n");

	return kIOReturnSuccess;
}

//==============================================================================

void FileNVRAM::registerNVRAMController(IONVRAMController *nvram)
{
	LOG(NOTICE, "registerNVRAMController(%p) called\n", nvram);
}

//==============================================================================

void FileNVRAM::sync(void)
{
	LOG(NOTICE, "sync() called\n");

	mCommandGate->runCommand( ( void * ) kNVRAMSyncCommand, NULL, NULL, NULL );
}

//==============================================================================

void FileNVRAM::doSync(void)
{
	LOG(NOTICE, "doSync() called\n");

	/* if (!mFilePath)
	{
		return;
	} */

	if (!mSafeToSync)
	{
		return;
	}
	
	LOG(NOTICE, "doSync() running\n");

	//create the output Dictionary
	OSDictionary * outputDict = OSDictionary::withCapacity(1);

	//going to have to spin over ourselves..
	OSDictionary * inputDict = dictionaryWithProperties();
	OSCollectionIterator *iter = OSCollectionIterator::withCollection(inputDict);

	if (iter == 0)
	{
		LOG(ERROR, "FAILURE!. No iterator on input dictionary (myself)\n");
		return;
	}

	OSSymbol * key = NULL;
	OSObject * value = NULL;

	while ((key = OSDynamicCast(OSSymbol,iter->getNextObject())))
	{
		//just get the value now anyway
		value = inputDict->getObject(key);

		//if the key conmektains :, look to see if it's in the map already, cause we'll add a child pair to it
		//otherwise we just slam the key/val pair in

		const char * keyChar = key->getCStringNoCopy();
		const char * guidValueStr = NULL;

		if (( guidValueStr = strstr(keyChar , NVRAM_SEPERATOR)) != NULL)
		{
			//we have a GUID child to deal with
			//now substring out the GUID cause thats going to be a DICT itself on the new outputDict
			//guidValueStr points to the :
			size_t guidCutOff = guidValueStr - keyChar;

			//allocate buffer
			//we ar ereally accounting for + sizeof('\0')
			//thats always 1. so 1.
			char guidStr[guidCutOff+1];
			strlcpy(guidStr, keyChar, guidCutOff+1);

			//in theory we have a guid and a value
			//LOG("sync() -> Located GUIDStr as %s\n",guidStr);

			//check for ?OSDictionary? from the dictionary
			OSDictionary * guidDict = OSDynamicCast(OSDictionary, outputDict->getObject(guidStr));

			if (!guidDict)
			{
				guidDict = OSDictionary::withCapacity(1);
				outputDict->setObject(guidStr,guidDict);
			}

			//now we have a dict for the guid no matter what (mapping GUID | DICT)
			guidDict->setObject(OSString::withCString(guidValueStr+strlen(NVRAM_SEPERATOR)), value);
		}
		else
		{
			//we are boring.
			outputDict->setObject(key,value);
		}
	}//end while

	//serialize and write this out
	OSSerialize *s = OSSerialize::withCapacity(10000);
	s->addString(NVRAM_FILE_HEADER);
	outputDict->serialize(s);
	s->addString(NVRAM_FILE_FOOTER);

	int error = write_buffer(s->text(), mCtx);

	if (error)
	{
		LOG(ERROR, "Unable to write to %s, errno %d\n", mFilePath->getCStringNoCopy(), error);
	}

	//now free the dictionaries && iter
	iter->release();
	outputDict->release();
	s->release();

}

//==============================================================================

bool FileNVRAM::serializeProperties(OSSerialize *s) const
{
	bool result = IOService::serializeProperties(s);
	LOG(NOTICE, "serializeProperties(%p) = %s\n", s, s->text());

	return result;
}

//==============================================================================

OSObject * FileNVRAM::getProperty(const OSSymbol *aKey) const
{
	OSObject* value = IOService::getProperty(aKey);

	if (value)
	{
		OSSerialize *s = OSSerialize::withCapacity(1000);

		if (value->serialize(s))
		{
			LOG(INFO, "getProperty(%s) = %s called\n", aKey->getCStringNoCopy(), s->text());
		}
		else
		{
			LOG(INFO, "getProperty(%s) = %p called\n", aKey->getCStringNoCopy(), value);
		}

		s->release();
	}
	else
	{

		// Ignore BSD Name for now in logs, it pollutes
		if (!aKey->isEqualTo("BSD Name"))
		{
			LOG(INFO, "getProperty(%s) = %p called\n", aKey->getCStringNoCopy(), (void*)NULL);
		}
	}

	return value;
}

//==============================================================================

OSObject * FileNVRAM::getProperty(const char *aKey) const
{
	const OSSymbol *keySymbol;
	OSObject *theObject = 0;

	keySymbol = OSSymbol::withCStringNoCopy(aKey);

	if (keySymbol != 0)
	{
		theObject = getProperty(keySymbol);
		keySymbol->release();
	}

	return theObject;
}

//==============================================================================

OSObject * FileNVRAM::copyProperty(const OSSymbol *aKey) const
{
	OSObject* prop = getProperty(aKey);

	if (prop)
	{
		prop->retain();
	}

	return prop;
}

//==============================================================================

OSObject * FileNVRAM::copyProperty(const char *aKey) const
{
	OSObject* prop = getProperty(aKey);

	if (prop)
	{
		prop->retain();
	}

	return prop;
}

//==============================================================================

bool FileNVRAM::setProperty(const OSSymbol *aKey, OSObject *anObject)
{
	// Verify permissions.
	if (IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator) != kIOReturnSuccess)
	{
		return false;
	}
	
	if ((strncmp("csr-data", aKey->getCStringNoCopy(), 8) == 0) || (strncmp("csr-active-config", aKey->getCStringNoCopy(), 17) == 0))
	{
		// Verify entitlements.
		if (IOUserClient::copyClientEntitlement(current_task(), "com.apple.private.iokit.nvram-csr") == NULL)
		{
			LOG(INFO, "setProperty(%s, (%s) %p) failed (not entitled)\n", aKey->getCStringNoCopy(), anObject->getMetaClass()->getClassName(), anObject);
			return false;
		}
	}
	
	OSSerialize *s = OSSerialize::withCapacity(1000);
	
	if (anObject->serialize(s))
	{
		
		LOG(INFO, "setProperty(%s, (%s) %s) called\n", aKey->getCStringNoCopy(), anObject->getMetaClass()->getClassName(), s->text());
	}
	else
	{
		LOG(INFO, "setProperty(%s, (%s) %p) called\n", aKey->getCStringNoCopy(), anObject->getMetaClass()->getClassName(), anObject);
	}
	
	s->release();
	
	// Check for special FileNVRAM properties:
	if (strncmp(FILE_NVRAM_GUID ":", aKey->getCStringNoCopy(), MIN(aKey->getLength(), strlen(FILE_NVRAM_GUID ":"))) == 0)
	{
		unsigned long bytes = aKey->getLength() - strlen(FILE_NVRAM_GUID ":") + 1;
		// Found GUID
		char* newKey = (char*)IOMalloc(bytes);
		snprintf(newKey, bytes+1, "%s", &(aKey->getCStringNoCopy()[strlen(FILE_NVRAM_GUID ":")]));
		
		// Send d
		OSString* str = OSString::withCString(newKey);
		handleSetting(str, anObject, this);
		str->release();
		IOFree(newKey, bytes);
	}
	
	bool stat = IOService::setProperty(aKey, cast(aKey, anObject));
	
	if (mInitComplete)
	{
		sync();
	}
	
	return stat;
}

//==============================================================================

void FileNVRAM::removeProperty(const OSSymbol *aKey)
{
	// Verify permissions.
	IOReturn result = IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator);

	if (result != kIOReturnSuccess)
	{
		return;
	}
	
	LOG(NOTICE, "removeProperty() called\n");

	IOService::removeProperty(aKey);

	if (mInitComplete)
	{
		sync();
	}
}

//==============================================================================

IOReturn FileNVRAM::setProperties(OSObject *properties)
{
	bool                 result = true;
	OSObject             *object;
	const OSSymbol       *key;
	const OSString       *tmpStr;
	OSDictionary         *dict;
	OSCollectionIterator *iter;

	dict = OSDynamicCast(OSDictionary, properties);

	if (!dict)
	{
		return kIOReturnBadArgument;
	}

	iter = OSCollectionIterator::withCollection(dict);

	if (!iter)
	{
		return kIOReturnBadArgument;
	}

	while (result)
	{
		key = OSDynamicCast(OSSymbol, iter->getNextObject());

		if (!key)
		{
			break;
		}

		object = dict->getObject(key);

		if (!object)
		{
			continue;
		}
		
		if (key->isEqualTo(kIONVRAMDeletePropertyKey))
		{
			tmpStr = OSDynamicCast(OSString, object);

			if (tmpStr)
			{
				key = OSSymbol::withString(tmpStr);
				removeProperty(key);
				key->release();
				result = true;
			}
			else
			{
				result = false;
			}
		}
		else if(key->isEqualTo(kIONVRAMSyncNowPropertyKey))
		{
			tmpStr = OSDynamicCast(OSString, object);

			if (tmpStr)
			{
				result = true; // We are not going to gaurantee sync, this is best effort

				if (safeToSync())
				{
					sync();
				}
			}
			else
			{
				result = false;
			}
		}
		else
		{
			result = setProperty(key, object);
		}

	}

	iter->release();

	if (result)
	{
		return kIOReturnSuccess;
	}

	return kIOReturnError;
}

//==============================================================================

IOReturn FileNVRAM::readXPRAM(IOByteCount offset, UInt8 *buffer, IOByteCount length)
{
	LOG(NOTICE, "readXPRAM(%zu, %p, %zu) called\n", (size_t)offset, buffer, (size_t)length);

	return kIOReturnUnsupported;
}

//==============================================================================

IOReturn FileNVRAM::writeXPRAM(IOByteCount offset, UInt8 *buffer, IOByteCount length)
{
	LOG(NOTICE, "writeXPRAM(%zu, %p, %zu) called\n", (size_t)offset, buffer, (size_t)length);

	return kIOReturnUnsupported;
}

//==============================================================================

IOReturn FileNVRAM::readNVRAMProperty(IORegistryEntry *entry, const OSSymbol **name, OSData **value)
{
	LOG(NOTICE, "readNVRAMProperty(%s, %p, %p) called\n", entry->getName(), name, value);

	return kIOReturnUnsupported;
}

//==============================================================================

IOReturn FileNVRAM::writeNVRAMProperty(IORegistryEntry *entry, const OSSymbol *name, OSData *value)
{
	LOG(NOTICE, "writeNVRAMProperty(%s, %s, %p) called\n", entry->getName(), name->getCStringNoCopy(), value);

	return kIOReturnUnsupported;
}

//==============================================================================

OSDictionary * FileNVRAM::getNVRAMPartitions(void)
{
	LOG(NOTICE, "getNVRAMPartitions() called\n");

	return 0;
}

//==============================================================================

IOReturn FileNVRAM::readNVRAMPartition(const OSSymbol *partitionID, IOByteCount offset, UInt8 *buffer, IOByteCount length)
{
	LOG(NOTICE, "readNVRAMPartition(%s, %zu, %p, %zu) called\n", partitionID->getCStringNoCopy(), (size_t)offset, buffer, (size_t)length);

	return kIOReturnNotFound;
}

//==============================================================================

IOReturn FileNVRAM::writeNVRAMPartition(const OSSymbol *partitionID, IOByteCount offset, UInt8 *buffer, IOByteCount length)
{
	LOG(NOTICE, "writeNVRAMPartition(%s, %zu, %p, %zu) called\n", partitionID->getCStringNoCopy(), (size_t)offset, buffer, (size_t)length);

	return kIOReturnSuccess;
}

//==============================================================================

IOByteCount FileNVRAM::savePanicInfo(UInt8 *buffer, IOByteCount length)
{
	// NOTE: In the event of a panic, we *cannot* use printf's.
	// Also, we double panic when we call the next line, investigate.
	//setProperty(OSSymbol::withCString("PANIC"), OSString::withCString("test"));

	return length;
}

//==============================================================================

bool FileNVRAM::safeToSync(void)
{
	static int count;
	LOG(NOTICE, "safeToSync() called\n");

	// Don't sync every time... we already do it so it shouldn't be needed, but just in case.
	count++;

	if (count % 4)
	{
		return true;
	}

	return false;
}

//==============================================================================

IOReturn FileNVRAM::dispatchCommand(OSObject* owner, void* arg0, void* arg1, void* arg2, void* arg3)
{
	FileNVRAM* self = OSDynamicCast(FileNVRAM, owner);

	if (!self)
	{
		return kIOReturnBadArgument;
	}
	
	size_t command = (size_t) arg0;

	switch (command)
	{
		case kNVRAMSyncCommand:
			self->doSync();
			break;

		default:
			break;
	}

	return kIOReturnSuccess;
}

//==============================================================================

void FileNVRAM::timeoutOccurred(OSObject *target, IOTimerEventSource* timer)
{
	static int retryCount;

	if (target)
	{
		FileNVRAM* self = OSDynamicCast(FileNVRAM, target);

		if (self)
		{
			uint64_t timeout = 20000; // 20ms
			// Check to see if BSD has been published, if so sync();

			OSDictionary *  dict = 0;
			IOService *     match = 0;
			boolean_t		found = false;

			do
			{
				dict = IOService::resourceMatching("IOBSD");

				if (dict)
				{
					if (IOService::waitForMatchingService(dict, timeout))
					{
						found = true;
					}
				}
			} while (false);

			OSSafeReleaseNULL(dict);
			OSSafeReleaseNULL(match);

			if (found)
			{
				UInt8 mLoggingLevel = self->mLoggingLevel;
				LOG(NOTICE, "BSD found, syncing\n");

				// TODO: Read /Extra/NVRAM/nvram.plist and populate the device tree.
				char* buffer;
				uint64_t len;

				if (self->read_buffer(&buffer, &len, self->mCtx))
				{
					retryCount++;
					LOG(ERROR, "Unable to read in nvram data at %s\n", self->mFilePath->getCStringNoCopy());
					// TODO: Check if / is mounted, and if not, try again until it is.
					if (retryCount < 100)
					{
						timer->setTimeoutMS(100);
					}
					else
					{
						self->mSafeToSync = true;
						self->registerNVRAM();
					}
				}
				else
				{
					self->mSafeToSync = false;

					timer->cancelTimeout();
					self->getWorkLoop()->removeEventSource(timer);
					timer->release();
					self->mTimer = NULL;

					if (len > strlen(NVRAM_FILE_HEADER) + strlen(NVRAM_FILE_FOOTER) + 1)
					{
						char* xml = buffer + strlen(NVRAM_FILE_HEADER);
						size_t xmllen = (size_t)len - strlen(NVRAM_FILE_HEADER) - strlen(NVRAM_FILE_FOOTER);
						xml[xmllen-1] = 0;
						OSString *errmsg = 0;
						OSObject* nvram = OSUnserializeXML(xml, &errmsg);

						if (nvram)
						{
							OSDictionary* data = OSDynamicCast(OSDictionary, nvram);
							//if(data) self->setPropertyTable(data);

							if (data)
							{
								self->copyUnserialzedData(NULL, data);
							}

							nvram->release();
						}
					}

					IOFree(buffer, (size_t)len);


					self->mSafeToSync = true;
					self->registerNVRAM();
					//self->sync();
				}
			}
			else
			{
				timer->setTimeoutMS(50);
			}
		}
		else
		{
			//printf("Self is not of type FileNVRAM.\n");
		}
	}
}

//==============================================================================

IOReturn FileNVRAM::setPowerState ( unsigned long whichState, IOService * whatDevice )
{
	LOG(NOTICE, "setPowerState() state %lu\n",whichState);

	switch (whichState)
	{
		case POWER_STATE_OFF:
			LOG(NOTICE, "Entering sleep\n");
			mSafeToSync = false;
			// Going to sleep. Perform state-saving tasks here.
			break;

		default:
		case POWER_STATE_ON:
			LOG(NOTICE, "Wakeing\n");
			// Waking up. Perform device initialization here.
			mSafeToSync = true;
			break;
	}

	return kIOPMAckImplied;
}

//==============================================================================

OSObject* FileNVRAM::cast(const OSSymbol* key, OSObject* obj)
{
	const char* legacy[] = {
		"boot-args",
		"boot-script",
	};

	OSString* str = OSDynamicCast(OSString, key);

	if (str)
	{
		for (int i = 0; i < sizeof(legacy)/sizeof(char*); i++)
		{
			if (str->isEqualTo(legacy[i]))
			{
				LOG(NOTICE, "Found legacy key %s\n", str->getCStringNoCopy());

				// add null char, convert to OSString
				OSData* data = OSDynamicCast(OSData, obj);

				if (data)
				{
					data->appendByte(0x00, 1);

					return OSString::withCString((const char*)data->getBytesNoCopy());
				}
			}
		}
	}

	return obj;
}

//==============================================================================

IOReturn FileNVRAM::write_buffer(char* aBuffer, vfs_context_t aCtx)
{
	IOReturn error = 0;

	int length = (int)strlen(aBuffer);
	
	struct vnode * vp;
	
	if (aCtx)
	{
		if ((error = vnode_open(FILE_NVRAM_PATH, (O_TRUNC | O_CREAT | FWRITE | O_NOFOLLOW), S_IRUSR | S_IWUSR, VNODE_LOOKUP_NOFOLLOW, &vp, aCtx)))
		{
			printf("FileNVRAM.kext: Error, vnode_open(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
			
			return error;
		}
		else
		{
			if ((error = vnode_isreg(vp)) == VREG)
			{
				if ((error = vn_rdwr(UIO_WRITE, vp, aBuffer, length, 0, UIO_SYSSPACE, IO_NOCACHE|IO_NODELOCKED|IO_UNIT, vfs_context_ucred(aCtx), (int *) 0, vfs_context_proc(aCtx))))
				{
					printf("FileNVRAM.kext: Error, vn_rdwr(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
				}
				
				if ((error = vnode_close(vp, FWASWRITTEN, aCtx)))
				{
					printf("FileNVRAM.kext: Error, vnode_close(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
				}
			}
			else
			{
				printf("FileNVRAM.kext: Error, vnode_isreg(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
			}
		}
	}
	else
	{
		printf("FileNVRAM.kext: aCtx == NULL!\n");
		error = 0xFFFF; // EINVAL;
	}
	
	return error;
}

//==============================================================================

IOReturn FileNVRAM::read_buffer(char** aBuffer, uint64_t* aLength, vfs_context_t aCtx)
{
	IOReturn error = 0;

	struct vnode * vp;
	struct vnode_attr va;

	if (aCtx)
	{
		if ((error = vnode_open(FILE_NVRAM_PATH, (O_RDONLY | FREAD | O_NOFOLLOW), S_IRUSR, VNODE_LOOKUP_NOFOLLOW, &vp, aCtx)))
		{
			printf("failed opening vnode at path %s, errno %d\n", FILE_NVRAM_PATH, error);
			
			return error;
		}
		else
		{
			if ((error = vnode_isreg(vp)) == VREG)
			{
				VATTR_INIT(&va);
				VATTR_WANTED(&va, va_data_size);	/* size in bytes of the fork managed by current vnode */

				// Determine size of vnode
				if ((error = vnode_getattr(vp, &va, aCtx)))
				{
					printf("FileNVRAM.kext: Error, failed to determine file size of %s, errno %d.\n", FILE_NVRAM_PATH, error);
				}
				else
				{
					if (aLength)
					{
						*aLength = va.va_data_size;
					}
					
					*aBuffer = (char *)IOMalloc((size_t)va.va_data_size);
					int len = (int)va.va_data_size;
					
					if ((error = vn_rdwr(UIO_READ, vp, *aBuffer, len, 0, UIO_SYSSPACE, IO_NOCACHE|IO_NODELOCKED|IO_UNIT, vfs_context_ucred(aCtx), (int *) 0, vfs_context_proc(aCtx))))
					{
						printf("FileNVRAM.kext: Error, writing to vnode(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
					}
				}
				
				if ((error = vnode_close(vp, 0, aCtx)))
				{
					printf("FileNVRAM.kext: Error, vnode_close(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
				}
			}
			else
			{
				printf("FileNVRAM.kext: Error, vnode_isreg(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
			}
		}
	}
	else
	{
		printf("FileNVRAM.kext: aCtx == NULL!\n");
		error = 0xFFFF; // EINVAL;
	}
	
	return error;
}

