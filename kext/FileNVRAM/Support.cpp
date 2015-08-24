/***
 * Support.cpp
 * FileNVRAM
 *
 * Created by Evan Lojewski on 1/29/13.
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
 */

#include "FileNVRAM.h"

//==============================================================================

static inline const char * strstr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0)
	{
		len = strlen(find);

		do
		{
			do
			{
				if ((sc = *s++) == 0)
				{
					return (NULL);
				}
			} while (sc != c);
		} while (strncmp(s, find, len) != 0);

		s--;
	}

	return s;
}

//==============================================================================

static inline void handleSetting(const OSObject* object, const OSObject* value, FileNVRAM* entry)
{
	UInt8 mLoggingLevel = entry->mLoggingLevel;

	OSString* key = OSDynamicCast( OSString, object);

	if(key->isEqualTo(NVRAM_ENABLE_LOG))
	{
		OSData* shouldlog = OSDynamicCast(OSData, value);

		if (shouldlog)
		{
			const void* data = shouldlog->getBytesNoCopy();
			mLoggingLevel = entry->mLoggingLevel = ((UInt8*)data)[0];

			LOG(INFO, "Setting logging to level %d.\n", mLoggingLevel);
		}
	}
	else
	{
		LOG(NOTICE, "Unknown key\n");
	}
}
