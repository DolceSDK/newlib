/*
Copyright (C) 2015 xyzz
Copyright (C) 2016 Sergi Granell
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

#include <stddef.h>
#include <psp2/kernel/threadmgr.h>

static SceKernelLwMutexWork _newlib_malloc_mutex;

void __malloc_lock(struct _reent *r) {
	sceKernelLockLwMutex(&_newlib_malloc_mutex, 1, NULL);
}

void __malloc_unlock(struct _reent *r) {
	sceKernelUnlockLwMutex(&_newlib_malloc_mutex, 1);
}

void _init_vita_malloc(void) {
	sceKernelCreateLwMutex(
		&_newlib_malloc_mutex,
		"malloc mutex",
		SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE,
		0,
		NULL);
}

void _free_vita_malloc(void) {
	sceKernelDeleteLwMutex(&_newlib_malloc_mutex);
}
