/***
 * FileIO.h
 * FileNVRAM
 *
 * Created by Chris Morton on 1/14/13.
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

#ifndef FileNVRAM_FileIO_h
#define FileNVRAM_FileIO_h

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <libkern/libkern.h>

#include <IOKit/IOLib.h>

#define	FILE_NVRAM_PATH			"/Extra/NVRAM/nvram.plist"

extern vfs_context_t mCtx;

int write_buffer(char* buffer);
int read_buffer(char** buffer, uint64_t* length);

#ifdef __cplusplus
}
#endif

#endif /* FileNVRAM_FileIO_h */
