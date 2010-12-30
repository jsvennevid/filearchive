#include <filearchive/api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int listDirectory(fa_archive_t* archive, const char* path)
{
	fa_dir_t* dir;
	int result = -1;

	do
	{
		fa_dirinfo_t info;
		int files = 0;

		dir = fa_open_dir(archive, path);
		if (dir == NULL)
		{
			fprintf(stderr, "list: Failed to open archive directory \"%s\"\n", path);
			break;
		}

		while (fa_read_dir(dir, &info) == 0)
		{
			if (info.type == FA_ENTRY_DIR)
			{
				char* buf = malloc(strlen(path) + strlen(info.name) + 2);
				sprintf(buf, "%s%s/", path, info.name);

				listDirectory(archive, buf);

				free(buf);
			}
			else if (info.type == FA_ENTRY_FILE)
			{
				char hash[sizeof(fa_hash_t) * 2 + 1];
				int i;

				if (files == 0)
				{
					fprintf(stdout, "Dir: \"%s\"\n", path);
					files = 1;
				}

				for (i = 0; i < sizeof(fa_hash_t); ++i)
				{
					sprintf(hash + i * 2, "%02x", info.hash.data[i]);
				}

				fprintf(stdout, "File: \"%s\", %u bytes (%u bytes compressed, ratio %.2f%%), hash: %s\n", info.name, info.size.original, info.size.compressed, info.size.original > 0 ? (info.size.compressed * 100.0f) / info.size.original : 0, hash);
			}
		} 

		result = 0;
	}
	while (0);

	fa_close_dir(dir);

	return result;
}

static int listArchive(const char* path)
{
	fa_archive_t* archive;
	int result = -1;

	do
	{
		fa_archiveinfo_t info;
		char hash[sizeof(fa_hash_t) + 1];
		int i;

		archive = fa_open_archive(path, FA_MODE_READ, 0, &info);
		if (archive == NULL)
		{
			fprintf(stderr, "list: Failed to open archive \"%s\"\n", path);
			break;
		}

		fprintf(stdout, "Archive: \"%s\"\n", path);

		result = listDirectory(archive, "");

		for (i = 0; i < sizeof(fa_hash_t); ++i)
		{
			sprintf(hash + i * 2, "%02x", info.footer.toc.hash.data[i]);
		}

		fprintf(stdout, "Data: %u bytes (%u bytes compressed, ratio %.2f%%)\n", info.footer.data.original, info.footer.data.compressed, info.footer.data.original > 0 ? (info.footer.data.compressed * 100.0f) / info.footer.data.original : 0);
		fprintf(stdout, "TOC: %u bytes (%u bytes compressed, ratio %.2f%%), hash: %s\n", info.footer.toc.original, info.footer.toc.compressed, info.footer.toc.original > 0 ? (info.footer.toc.compressed * 100.0f) / info.footer.toc.original : 0, hash); 
	}
	while (0);

	if (archive != NULL)
	{
		fa_close_archive(archive, FA_COMPRESSION_NONE, NULL);
	}

	return result;
}

int commandList(int argc, const char* argv[])
{
	int i;

	if (argc < 3)
	{
		return -1;
	}

	for (i = 2; i < argc; ++i)
	{
		if (listArchive(argv[i]) < 0)
		{
			return -1;
		}
	}

	return 0;
}
