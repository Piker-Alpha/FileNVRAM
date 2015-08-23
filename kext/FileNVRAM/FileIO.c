//
//  FileIO.c
//  FileNVRAM
//
//  Created by Evan Lojewski on 1/13/13.
//  Copyright (c) 2013-2014 xZenue LLC. All rights reserved.
//
// This work is licensed under the
//  Creative Commons Attribution-NonCommercial 3.0 Unported License.
//  To view a copy of this license, visit http://creativecommons.org/licenses/by-nc/3.0/.
//

#include "FileIO.h"

int write_buffer(const char* path, char* buffer, int length, vfs_context_t ctx)
{
    
    int error;
    struct vnode * vp;
    
    if ((error = vnode_open(path, (O_TRUNC | O_CREAT | FWRITE | O_NOFOLLOW), S_IRUSR | S_IWUSR, VNODE_LOOKUP_NOFOLLOW, &vp, ctx)))
    {
        printf("failed opening vnode at path %s, errno %d\n",path, error);
        return error;
    }
    
    if ((error = vn_rdwr(UIO_WRITE, vp, buffer, length, 0, UIO_SYSSPACE, IO_NOCACHE|IO_NODELOCKED|IO_UNIT, vfs_context_ucred(ctx), (int *) 0, vfs_context_proc(ctx))))
    {
        printf("Error writing to vnode at path %s, errno %d\n",path,error);
    }
	
    if ((error = vnode_close(vp, FWASWRITTEN, ctx)))
    {
        printf("Error closing vnode errno %d\n",error);
    }
    
    
    return 0;
}

int read_buffer(const char* path, char** buffer, uint64_t* length, vfs_context_t ctx)
{
    int error;
    struct vnode * vp;
    struct vnode_attr ap;

    
    if ((error = vnode_open(path, (O_RDONLY | FREAD | O_NOFOLLOW), S_IRUSR, VNODE_LOOKUP_NOFOLLOW, &vp, ctx)))
    {
        printf("failed opening vnode at path %s, errno %d\n",path, error);
        return error;
    }
    
    
    VATTR_INIT(&ap);
    VATTR_WANTED(&ap, va_data_size);
    
    // Determine size of vnode
    if((error = vnode_getattr(vp, &ap, ctx)))
    {
        printf("failed to determine file size of %s, errno %d.\n", path, error);
    }
    else
    {
        if(length) *length = ap.va_data_size;
        *buffer = IOMalloc((size_t)ap.va_data_size);
        int len = (int)ap.va_data_size;
        
        if ((error = vn_rdwr(UIO_READ, vp, *buffer, len, 0, UIO_SYSSPACE, IO_NOCACHE|IO_NODELOCKED|IO_UNIT, vfs_context_ucred(ctx), (int *) 0, vfs_context_proc(ctx))))
        {
            printf("Error writing to vnode at path %s, errno %d\n",path,error);
        }
    }
    
    if ((error = vnode_close(vp, 0, ctx)))
    {
        printf("Error closing vnode errno %d\n",error);
    }
    
    return 0;
}
