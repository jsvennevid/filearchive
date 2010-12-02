#include "../internal/api.h"

static fa_container_t* fa_find_container(const fa_archive_t* archive, const char* path);

fa_file_t* fa_open_file(fa_archive_t* archive, const char* filename, fa_compression_t compression)
{
	if (archive == NULL)
	{
		return NULL;
	}

	switch (archive->mode)
	{
		case FA_MODE_READ:
		{
			fa_container_t* container = fa_find_container(archive, filename);
			if (container == NULL)
			{
				return NULL;
			}
		}
		break;

		case FA_MODE_WRITE:
		{
		}
		break;
	}

}

