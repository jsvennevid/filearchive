#ifndef FILEARCHIVE_INTERNAL_API_H
#define FILEARCHIVE_INTERNAL_API_H

typedef struct fa_archive_t fa_archive_t;
typedef struct fa_file_t fa_file_t;
typedef struct fa_dir_t fa_dir_t;

#include "../api.h"

struct fa_archive_t
{
};

struct fa_file_t
{
	fa_archive_t* archive;
};

struct fa_dir_t
{
	fa_archive_t* archive;
};

#endif

