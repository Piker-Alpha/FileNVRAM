//
//  FileIO.h
//  FileNVRAM
//
//  Created by Chris Morton on 1/14/13.
//  Copyright (c) 2013-2014 xZenue LLC. All rights reserved.
//
// This work is licensed under the
//  Creative Commons Attribution-NonCommercial 3.0 Unported License.
//  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
//

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

int write_buffer(const char* path, char* buffer, int length, vfs_context_t ctx);
int read_buffer(const char* path, char** buffer, uint64_t* length, vfs_context_t ctx);


#ifdef __cplusplus
}
#endif

    
#endif /* FileNVRAM_FileIO_h */
