/*

Copyright (c) 2010 Jesper Svennevid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

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

#include <filearchive/internal/api.h>

static fa_io_handle_t fa_io_open(const char* filename, fa_mode_t mode);
static int fa_io_close(fa_io_handle_t handle);

static size_t fa_io_read(fa_io_handle_t handle, void* buffer, size_t length);
static size_t fa_io_write(fa_io_handle_t handle, const void* buffer, size_t length);

static int fa_io_lseek(fa_io_handle_t handle, int64_t offset, fa_seek_t whence);
static size_t fa_io_tell(fa_io_handle_t handle);

static fa_io_ops_t fa_io_default_ops =
{
	fa_io_open,
	fa_io_close,
	fa_io_read,
	fa_io_write,
	fa_io_lseek,
	fa_io_tell
};

const fa_io_ops_t* fa_get_default_ops()
{
	return &fa_io_default_ops;
}

#if defined(__unix__) || defined(__APPLE__) 

#include <fcntl.h>
#include <unistd.h>

typedef union
{
	fa_io_handle_t handle;
	int fd;
} fa_io_internal_handle_t;

fa_io_handle_t fa_io_open(const char* filename, fa_mode_t mode)
{
	int oflags[2] = { O_RDONLY, O_WRONLY|O_CREAT|O_TRUNC }; 	
	fa_io_internal_handle_t internal = { 0 };

	internal.fd = open(filename, oflags[mode], S_IRWXU|S_IRGRP|S_IROTH); 
	return internal.handle;
}

int fa_io_close(fa_io_handle_t handle)
{
	fa_io_internal_handle_t internal = { .handle = handle };
	return close(internal.fd);
}

size_t fa_io_read(fa_io_handle_t handle, void* buffer, size_t length)
{
	fa_io_internal_handle_t internal = { .handle = handle };
	ssize_t result = read(internal.fd, buffer, length);
	return result < 0 ? 0 : result;
}

size_t fa_io_write(fa_io_handle_t handle, const void* buffer, size_t length)
{
	fa_io_internal_handle_t internal = { .handle = handle };
	ssize_t result = write(internal.fd, buffer, length);
	return result < 0 ? 0 : result;
}

int fa_io_lseek(fa_io_handle_t handle, int64_t offset, fa_seek_t whence)
{
	fa_io_internal_handle_t internal = { .handle = handle };
	int whdata[3] = { SEEK_SET, SEEK_CUR, SEEK_END };
	return lseek(internal.fd, offset, whdata[whence]) < 0 ? -1 : 0;
}

size_t fa_io_tell(fa_io_handle_t handle)
{
	fa_io_internal_handle_t internal = { .handle = handle };
	off_t result = lseek(internal.fd, 0, SEEK_CUR);
	return result < 0 ? 0 : result; 
}
#elif defined(_WIN32)
#error WIN32
#else
#error OTHER
#endif
