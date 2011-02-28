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

#ifndef FILEARCHIVE_INTERNAL_API_H
#define FILEARCHIVE_INTERNAL_API_H

typedef struct fa_archive_t fa_archive_t;
typedef struct fa_archive_writer_t fa_archive_writer_t;
typedef struct fa_writer_entry_t fa_writer_entry_t;
typedef struct fa_file_t fa_file_t;
typedef struct fa_file_writer_t fa_file_writer_t;
typedef struct fa_dir_t fa_dir_t;

typedef struct fa_io_ops_t fa_io_ops_t;
typedef void* fa_io_handle_t;

#include "../api.h"

#include <sha1/sha1.h>

#include <stdint.h>

#define FA_COMPRESSION_MAX_BLOCK (16384)
#define FA_ARCHIVE_CACHE_SIZE (FA_COMPRESSION_MAX_BLOCK * 4)

#if defined(_WIN32)
#define FA_IO_INVALID_HANDLE ((fa_io_handle_t)-1)
#else
#define FA_IO_INVALID_HANDLE (NULL)
#endif

struct fa_archive_t
{
	fa_header_t* toc;
	fa_mode_t mode;

	const fa_io_ops_t* ops;
	fa_io_handle_t handle;
	uint64_t base;

	struct
	{
		uint32_t offset;
		uint32_t fill;
		uint8_t* data;
		fa_file_t* owner;
	} cache;
};

struct fa_archive_writer_t
{
	fa_archive_t archive;

	uint32_t alignment;

	struct
	{
		uint32_t original;
		uint32_t compressed;
	} offset;

	struct
	{
		fa_writer_entry_t* data;
		uint32_t count;
		uint32_t capacity;
	} entries;
};

struct fa_writer_entry_t
{
	char* path;

	fa_offset_t container;
	fa_offset_t offset;

	fa_compression_t compression;

	struct
	{
		uint32_t original;
		uint32_t compressed;
	} size;

	SHA1Context hash;
};

struct fa_file_t
{
	fa_archive_t* archive;
	const fa_entry_t* entry;

	uint64_t base;

	struct
	{
		uint32_t offset;
		uint32_t fill;
		uint8_t* data;
	} buffer;

	struct
	{
		uint32_t original;
		uint32_t compressed;
	} offset;
};

struct fa_file_writer_t
{
	fa_file_t file;
	fa_writer_entry_t* entry;
};

struct fa_dir_t
{
	const fa_archive_t* archive;
	const fa_container_t* parent;

	const fa_container_t* container;
	uint32_t index;
};

size_t fa_compress_block(fa_compression_t compression, void* out, size_t outSize, const void* in, size_t inSize);
size_t fa_decompress_block(fa_compression_t compression, void* out, size_t outSize, const void* in, size_t inSize); 
const fa_container_t* fa_find_container(const fa_archive_t* archive, const fa_container_t* container, const char* path);

struct fa_io_ops_t
{
	fa_io_handle_t (*open)(const char* filename, fa_mode_t mode);
	int (*close)(fa_io_handle_t handle);

	size_t (*read)(fa_io_handle_t handle, void* buffer, size_t length);
	size_t (*write)(fa_io_handle_t handle, const void* buffer, size_t length); 
	int (*lseek)(fa_io_handle_t handle, int64_t offset, fa_seek_t whence);
	size_t (*tell)(fa_io_handle_t handle);	
};

const fa_io_ops_t* fa_get_default_ops(); 

#endif

