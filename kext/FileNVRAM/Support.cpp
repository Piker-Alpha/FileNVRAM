//
//  Support.cpp
//  FileNVRAM
//
//  Created by Evan Lojewski on 1/29/13.
//  Copyright (c) 2013-2014 xZenue LLC. All rights reserved.
//
// This work is licensed under the
//  Creative Commons Attribution-NonCommercial 3.0 Unported License.
//  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
//

#include "Support.h"

static inline const char * strstr(const char *s, const char *find)
{
    char c, sc;
    size_t len;
	
    if ((c = *find++) != 0) {
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == 0)
                    return (NULL);
            } while (sc != c);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return s;
}

static inline void handleSetting(const OSObject* object, const OSObject* value, FileNVRAM* entry)
{
    UInt8 mLoggingLevel = entry->mLoggingLevel;

    OSString* key = OSDynamicCast( OSString, object);
    
    if(!key)
    {
        LOG(NOTICE, "Unknown key\n");
        return;
    }
    else
    {
        LOG(NOTICE, "Handling key %s\n", key->getCStringNoCopy());
    }
    
    if(key->isEqualTo(NVRAM_SET_FILE_PATH))
    {
        OSString* str = OSDynamicCast(OSString, value);
        if(str)
        {
            entry->setPath(str);
        }
        else
        {
            OSData* dat = OSDynamicCast(OSData, value);
            if(dat)
            {
                OSString* str = OSString::withCString((const char*)dat->getBytesNoCopy());
                entry->setPath(str);
                str->release();
            }
        }
        // Where to get path from?
    }
    else if(key->isEqualTo(NVRAM_ENABLE_LOG))
    {
        OSData* shouldlog = OSDynamicCast(OSData, value);
        if(shouldlog)
        {
            const void* data = shouldlog->getBytesNoCopy();
            mLoggingLevel = entry->mLoggingLevel = ((UInt8*)data)[0];
            LOG(INFO, "Setting logging to level %d.\n", mLoggingLevel);
        }

    }
}
