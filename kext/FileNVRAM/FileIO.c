/***
 * FileIO.c
 * FileNVRAM
 *
 * Created by Evan Lojewski on 1/13/13.
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

#include "FileIO.h"

//==============================================================================

int write_buffer(char* buffer)
{
	int error = 0;
	int length = (int)strlen(buffer);

	struct vnode * vp;

	if (mCtx)
	{
		if ((error = vnode_open(FILE_NVRAM_PATH, (O_TRUNC | O_CREAT | FWRITE | O_NOFOLLOW), S_IRUSR | S_IWUSR, VNODE_LOOKUP_NOFOLLOW, &vp, mCtx)))
		{
			printf("FileNVRAM.kext: Error, vnode_open(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);

			return error;
		}

		if ((error = vn_rdwr(UIO_WRITE, vp, buffer, length, 0, UIO_SYSSPACE, IO_NOCACHE|IO_NODELOCKED|IO_UNIT, vfs_context_ucred(mCtx), (int *) 0, vfs_context_proc(mCtx))))
		{
			printf("FileNVRAM.kext: Error, vn_rdwr(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
		}
	
		if ((error = vnode_close(vp, FWASWRITTEN, mCtx)))
		{
			printf("FileNVRAM.kext: Error, vnode_close(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
		}
	}
	else
	{
		printf("AppleIntelInfo.kext: mCtx == NULL!\n");
		error = 0xFFFF;
	}
	
	return error;
}

//==============================================================================

int read_buffer(char** buffer, uint64_t* length)
{
	int error = 0;

	struct vnode * vp;
	struct vnode_attr ap;

	if (mCtx)
	{
		if ((error = vnode_open(FILE_NVRAM_PATH, (O_RDONLY | FREAD | O_NOFOLLOW), S_IRUSR, VNODE_LOOKUP_NOFOLLOW, &vp, mCtx)))
		{
			printf("failed opening vnode at path %s, errno %d\n", FILE_NVRAM_PATH, error);

			return error;
		}
		else
		{
			if ((error = vnode_isreg(vp)) == VREG)
			{
				VATTR_INIT(&ap);
				VATTR_WANTED(&ap, va_data_size);

				// Determine size of vnode
				if ((error = vnode_getattr(vp, &ap, mCtx)))
				{
					printf("FileNVRAM.kext: Error, failed to determine file size of %s, errno %d.\n", FILE_NVRAM_PATH, error);
				}
				else
				{
					if (length)
					{
						*length = ap.va_data_size;
					}

					*buffer = IOMalloc((size_t)ap.va_data_size);
					int len = (int)ap.va_data_size;

					if ((error = vn_rdwr(UIO_READ, vp, *buffer, len, 0, UIO_SYSSPACE, IO_NOCACHE|IO_NODELOCKED|IO_UNIT, vfs_context_ucred(mCtx), (int *) 0, vfs_context_proc(mCtx))))
					{
						printf("FileNVRAM.kext: Error, writing to vnode(%s) failed with error %d!\n", FILE_NVRAM_PATH, error);
					}
				}

				if ((error = vnode_close(vp, 0, mCtx)))
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
		printf("FileNVRAM.kext: mCtx == NULL!\n");
		error = 0xFFFF;
	}

	return error;
}
