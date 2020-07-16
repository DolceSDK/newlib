/*
Copyright (C) 2016 Yifan Lu
Copyright (C) 2016, 2018 Davee
Copyright (C) 2020 Asakura Reiko

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <fcntl.h>
#include <sys/reent.h>
#include <sys/unistd.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>

#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>

#include "vitadescriptor.h"
#include "vitaglue.h"

#define CHECK_STD_INIT(ptr) do { \
	struct _reent *_check_init_ptr = (ptr); \
	if (_check_init_ptr && !_check_init_ptr->__sdidinit) \
		__sinit(_check_init_ptr); \
} while (0)

#define SCE_ERRNO_MASK 0xFF

DescriptorTranslation *__vita_fdmap[MAX_OPEN_FILES];
DescriptorTranslation __vita_fdmap_pool[MAX_OPEN_FILES];

SceKernelLwMutexWork _newlib_fd_mutex;

void _init_vita_io(void) {
	int ret;
	sceKernelCreateLwMutex(
		&_newlib_fd_mutex,
		"fd conversion table mutex",
		SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
		1,
		NULL);

	memset(__vita_fdmap, 0, sizeof(__vita_fdmap));
	memset(__vita_fdmap_pool, 0, sizeof(__vita_fdmap_pool));

	// TODO: do we prefer sceKernelStdin and friends?
	ret = sceIoOpen("tty0:", SCE_O_RDONLY, 0);

	if (ret >= 0) {
		__vita_fdmap[STDIN_FILENO] = &__vita_fdmap_pool[STDIN_FILENO];
		__vita_fdmap[STDIN_FILENO]->sce_uid = ret;
		__vita_fdmap[STDIN_FILENO]->type = VITA_DESCRIPTOR_TTY;
		__vita_fdmap[STDIN_FILENO]->ref_count = 1;
	}

	ret = sceIoOpen("tty0:", SCE_O_WRONLY, 0);

	if (ret >= 0) {
		__vita_fdmap[STDOUT_FILENO] = &__vita_fdmap_pool[STDOUT_FILENO];
		__vita_fdmap[STDOUT_FILENO]->sce_uid = ret;
		__vita_fdmap[STDOUT_FILENO]->type = VITA_DESCRIPTOR_TTY;
		__vita_fdmap[STDOUT_FILENO]->ref_count = 1;
	}

	ret = sceIoOpen("tty0:", SCE_O_WRONLY, 0);

	if (ret >= 0) {
		__vita_fdmap[STDERR_FILENO] = &__vita_fdmap_pool[STDERR_FILENO];
		__vita_fdmap[STDERR_FILENO]->sce_uid = ret;
		__vita_fdmap[STDERR_FILENO]->type = VITA_DESCRIPTOR_TTY;
		__vita_fdmap[STDERR_FILENO]->ref_count = 1;
	}

	sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);

	CHECK_STD_INIT(_GLOBAL_REENT);
}

void _free_vita_io(void) {
	sceKernelLockLwMutex(&_newlib_fd_mutex, 1, NULL);

	if (__vita_fdmap[STDIN_FILENO]) {
		sceIoClose(__vita_fdmap[STDIN_FILENO]->sce_uid);
		memset(__vita_fdmap[STDIN_FILENO], 0, sizeof(DescriptorTranslation));
		__vita_fdmap[STDIN_FILENO] = NULL;
	}
	if (__vita_fdmap[STDOUT_FILENO]) {
		sceIoClose(__vita_fdmap[STDOUT_FILENO]->sce_uid);
		memset(__vita_fdmap[STDOUT_FILENO], 0, sizeof(DescriptorTranslation));
		__vita_fdmap[STDOUT_FILENO] = NULL;
	}
	if (__vita_fdmap[STDERR_FILENO]) {
		sceIoClose(__vita_fdmap[STDERR_FILENO]->sce_uid);
		memset(__vita_fdmap[STDERR_FILENO], 0, sizeof(DescriptorTranslation));
		__vita_fdmap[STDERR_FILENO] = NULL;
	}

	sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
	sceKernelDeleteLwMutex(&_newlib_fd_mutex);
}

int __vita_acquire_descriptor(void)
{
	int fd = -1;
	int i = 0;
	sceKernelLockLwMutex(&_newlib_fd_mutex, 1, NULL);

	// get free descriptor
	// only allocate descriptors after stdin/stdout/stderr -> aka 0/1/2
	for (fd = 3; fd < MAX_OPEN_FILES; ++fd)
	{
		if (__vita_fdmap[fd] == NULL)
		{
			// get free pool
			for (i = 0; i < MAX_OPEN_FILES; ++i)
			{
				if (__vita_fdmap_pool[i].ref_count == 0)
				{
					__vita_fdmap[fd] = &__vita_fdmap_pool[i];
					__vita_fdmap[fd]->ref_count = 1;
					sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
					return fd;
				}
			}
		}
	}

	// no mores descriptors available...
	sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
	return -1;
}

int __vita_release_descriptor(int fd)
{
	DescriptorTranslation *map = NULL;
	int res = -1;

	sceKernelLockLwMutex(&_newlib_fd_mutex, 1, NULL);

	if (is_fd_valid(fd) && __vita_fd_drop(__vita_fdmap[fd]) >= 0)
	{
		__vita_fdmap[fd] = NULL;
		res = 0;
	}

	sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
	return res;
}

int __vita_duplicate_descriptor(int fd)
{
	int fd2 = -1;

	sceKernelLockLwMutex(&_newlib_fd_mutex, 1, NULL);

	if (is_fd_valid(fd))
	{
		// get free descriptor
		// only allocate descriptors after stdin/stdout/stderr -> aka 0/1/2
		for (fd2 = 3; fd2 < MAX_OPEN_FILES; ++fd2)
		{
			if (__vita_fdmap[fd2] == NULL)
			{
				__vita_fdmap[fd2] = __vita_fdmap[fd];
				__vita_fdmap[fd2]->ref_count++;
				sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
				return fd2;
			}
		}
	}

	sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
	return -1;
}

int __vita_descriptor_ref_count(int fd)
{
	int res = 0;
	sceKernelLockLwMutex(&_newlib_fd_mutex, 1, NULL);
	res = __vita_fdmap[fd]->ref_count;
	sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
	return res;
}

DescriptorTranslation *__vita_fd_grab(int fd)
{
	DescriptorTranslation *map = NULL;

	sceKernelLockLwMutex(&_newlib_fd_mutex, 1, NULL);

	if (is_fd_valid(fd))
	{
		map = __vita_fdmap[fd];

		if (map)
			map->ref_count++;
	}

	sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
	return map;
}

int __vita_fd_drop(DescriptorTranslation *map)
{
	sceKernelLockLwMutex(&_newlib_fd_mutex, 1, NULL);

	if (map->ref_count == 1)
	{
		int ret = 0;

		switch (map->type)
		{
		case VITA_DESCRIPTOR_FILE:
		case VITA_DESCRIPTOR_TTY:
		{
			ret = sceIoClose(map->sce_uid);
			break;
		}
		case VITA_DESCRIPTOR_SOCKET:
			if (__vita_glue_socket_close)
				ret = __vita_glue_socket_close(map->sce_uid);
			break;
		}

		if (ret < 0) {
			sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
			return -(ret & SCE_ERRNO_MASK);
		}

		map->ref_count--;
		memset(map, 0, sizeof(DescriptorTranslation));
	}
	else
	{
		map->ref_count--;
	}

	sceKernelUnlockLwMutex(&_newlib_fd_mutex, 1);
	return 0;
}
