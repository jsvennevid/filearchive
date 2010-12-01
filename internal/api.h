#ifndef FILEARCHIVE_INTERNAL_API_H
#define FILEARCHIVE_INTERNAL_API_H

typedef struct fa_archive_t fa_archive_t;
typedef struct fa_archive_writer_t fa_archive_writer_t;
typedef struct fa_file_t fa_file_t;
typedef struct fa_dir_t fa_dir_t;

#include "../api.h"

#include <stdint.h>

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
	} cache;
};

struct fa_archive_writer_t
{
	fa_archive_t archive;

	uint32_t alignment;
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
};

struct fa_dir_t
{
	fa_archive_t* archive;
	fa_container_t* container;
	uint32_t index;
};

#endif

