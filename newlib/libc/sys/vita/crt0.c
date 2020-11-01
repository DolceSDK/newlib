/*
Copyright (C) 2015 xyzz
Copyright (C) 2015 Glenn Anderson
Copyright (C) 2016 xerpi
Copyright (C) 2016 Yifan Lu
Copyright (C) 2016 frangarcj
Copyright (C) 2018 yne
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
#include <stdlib.h>
#include <string.h>

/* The maximum number of arguments that can be passed to main(). */
#define ARGC_MAX 19

int main(int argc, const char* argv[]);
void _init_vita_heap(void);
void _free_vita_heap(void);
void _init_vita_reent(void);
void _free_vita_reent(void);
void _init_vita_malloc(void);
void _free_vita_malloc(void);
void _init_vita_io(void);
void _free_vita_io(void);
void __libc_init_array(void);

void _init_vita_newlib(void) {
	_init_vita_heap();
	_init_vita_reent();
	_init_vita_malloc();
	_init_vita_io();
}

void _free_vita_newlib(void) {
	_free_vita_io();
	_free_vita_malloc();
	_free_vita_reent();
	_free_vita_heap();
}

/*
 * Code below is based on the PSPSDK implementation
 * Copyright (c) 2005 Marcus R. Brown <mrbrown@ocgnet.org>
 */

void _start(unsigned int args, const void *argp)
{
	const char *argv[ARGC_MAX + 1] = {""}; // Program name
	int argc = 1;
	int loc = 0;
	const char *ptr = argp;

	/* Turn our thread arguments into main()'s argc and argv[]. */
	while(loc < args)
	{
		argv[argc] = &ptr[loc];
		loc += strnlen(&ptr[loc], args - loc) + 1;
		argc++;
		if(argc == ARGC_MAX)
		{
			break;
		}
	}
	argv[argc] = NULL;

	_init_vita_newlib();
	__libc_init_array();
	setenv("HOME", "app0:", 1);

	exit(main(argc, argv));
}
