/*
Copyright (C) 2015 xyzz
Copyright (C) 2015 frangarcj
Copyright (C) 2015 Glenn Anderson
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

#include <errno.h>
#include <reent.h>

#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

extern unsigned int _newlib_heap_size_user __attribute__((weak));

static int _newlib_heap_memblock;
static unsigned _newlib_heap_size;
static char *_newlib_heap_base, *_newlib_heap_end, *_newlib_heap_cur;
static SceKernelLwMutexWork _newlib_sbrk_mutex;

void * _sbrk_r(struct _reent *reent, ptrdiff_t incr) {
	if (sceKernelLockLwMutex(&_newlib_sbrk_mutex, 1, NULL) < 0)
		goto fail;
	if (!_newlib_heap_base || _newlib_heap_cur + incr >= _newlib_heap_end) {
		sceKernelUnlockLwMutex(&_newlib_sbrk_mutex, 1);
fail:
		reent->_errno = ENOMEM;
		return (void*)-1;
	}

	char *prev_heap_end = _newlib_heap_cur;
	_newlib_heap_cur += incr;

	sceKernelUnlockLwMutex(&_newlib_sbrk_mutex, 1);
	return (void*) prev_heap_end;
}

void _init_vita_heap(void) {
	// Create a mutex to use inside _sbrk_r
	if (sceKernelCreateLwMutex(
			&_newlib_sbrk_mutex,
			"sbrk mutex",
			SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO,
			0,
			NULL) < 0) {
		goto failure;
	}
	if (&_newlib_heap_size_user != NULL) {
		_newlib_heap_size = _newlib_heap_size_user;
	} else {
		// Create a memblock for the heap memory, 32MiB
		_newlib_heap_size = SCE_KERNEL_32MiB;
	}
	_newlib_heap_memblock = sceKernelAllocMemBlock(
		"Newlib heap",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
		_newlib_heap_size,
		NULL);
	if (_newlib_heap_memblock < 0) {
		goto failure;
	}
	if (sceKernelGetMemBlockBase(_newlib_heap_memblock, (void**)&_newlib_heap_base) < 0) {
		goto failure;
	}
	_newlib_heap_end = _newlib_heap_base + _newlib_heap_size;
	_newlib_heap_cur = _newlib_heap_base;

	return;
failure:
	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}

void _free_vita_heap(void) {
	// Destroy the sbrk mutex
	sceKernelDeleteLwMutex(&_newlib_sbrk_mutex);

	// Free the heap memblock to avoid memory leakage.
	sceKernelFreeMemBlock(_newlib_heap_memblock);

	_newlib_heap_memblock = 0;
	_newlib_heap_base = 0;
	_newlib_heap_cur = 0;
}
