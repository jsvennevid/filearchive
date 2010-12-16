#ifndef FILEARCHIVE_INTERNAL_API_H
#define FILEARCHIVE_INTERNAL_API_H

typedef struct fa_archive_t fa_archive_t;
typedef struct fa_archive_writer_t fa_archive_writer_t;
typedef struct fa_writer_entry_t fa_writer_entry_t;
typedef struct fa_file_t fa_file_t;
typedef struct fa_file_writer_t fa_file_writer_t;
typedef struct fa_dir_t fa_dir_t;

#include "../api.h"

#include <sha1/sha1.h>

#include <stdint.h>

#define FA_COMPRESSION_MAX_BLOCK (16384)
#define FA_ARCHIVE_CACHE_SIZE (FA_COMPRESSION_MAX_BLOCK * 4)

struct fa_archive_t
{
	fa_header_t* toc;
	fa_mode_t mode;

	uint32_t base;
	intptr_t fd;

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
	uint32_t offset;

	struct
	{
		fa_writer_entry_t* data;
		uint32_t count;
		uint32_t capacity;
	} entries;
};

struct fa_writer_entry_t
{
	const char* path;

	fa_offset_t offset;
	fa_compression_t compression;

	struct
	{
		uint32_t original;
		uint32_t compressed;
	} size;
};

struct fa_file_t
{
	fa_archive_t* archive;

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

	SHA1Context hash;
};

struct fa_file_writer_t
{
	fa_file_t file;
	fa_writer_entry_t* entry;
};

struct fa_dir_t
{
	fa_archive_t* archive;
	fa_container_t* container;
	uint32_t index;
};

size_t fa_compress_block(fa_compression_t compression, void* out, size_t outSize, const void* in, size_t inSize);

#endif

