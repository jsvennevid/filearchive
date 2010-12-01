#include "../internal/api.h"

static fa_archive_t* fa_open_archive_reading(const char* filename);
static fa_archive_t* fa_open_archive_writing(const char* filename, uint32_t alignment);

fa_archive_t* fa_open_archive(const char* filename, fa_mode_t mode, uint32_t alignment)
{
	fa_archive_t* archive = NULL;

	switch (mode)
	{
		case FA_MODE_READ:
		{
			archive = fa_open_archive_reading(filename);
		}
		break;

		case FA_MODE_WRITE:
		{
			archive = fa_open_archive_writing(filename, alignment);
		}
		break;
	}

	return archive;
}

int fa_close_archive(fa_archive_t* archive)
{
	return -1;
}

static fa_archive_t* fa_open_archive_reading(const char* filename)
{
	fa_archive_t* archive = malloc(sizeof(fa_archive_t));
	memset(archive, 0, sizeof(fa_archive_t));

	do
	{
		archive->fd = fopen(filename, "rb");
		if (archive->fd == NULL)
		{
			break;
		}

		// return archive;
	}
	while (0);

	if (archive->fd)
	{
		fclose(archive->fd);
	}

	free(archive);
	return NULL;
}

static fa_archive_t* fa_open_archive_writing(const char* filename, uint32_t alignment)
{
	fa_archive_writer_t* archive = malloc(sizeof(fa_archive_writer_t));
	memset(archive, 0, sizeof(fa_archive_writer_t));

	do
	{
		// return &(archive->archive);
	}
	while (0);

	free(archive);
	return NULL;
}

