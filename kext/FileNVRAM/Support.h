//
//  Support.h
//  FileNVRAM
//
//  Created by Evan Lojewski on 1/29/13.
//  Copyright (c) 2013 xZenue LLC. All rights reserved.
//
// This work is licensed under the
//  Creative Commons Attribution-NonCommercial 3.0 Unported License.
//  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
//

#ifndef __FileNVRAM__Support__
#define __FileNVRAM__Support__

#include "FileNVRAM.h"

static inline const char * strstr(const char *s, const char *find);
static inline void gen_random(char *s, const int len);
static inline void handleSetting(const OSObject* object, const OSObject* value, FileNVRAM* entry);

#endif /* defined(__FileNVRAM__Support__) */
