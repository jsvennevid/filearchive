#include <filearchive/filearchive.h>
#include <filearchive/api.h>

#if defined(_WIN32)
#pragma warning(disable: 4100 4127 4996)
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <ctype.h>

#if defined(__APPLE__)
#include <sys/syslimits.h>
#endif

#if defined(__OpenBSD__)
#include <sys/syslimits.h>
#endif

typedef enum
{
	State_Options,
	State_Archive,
	State_Files
} State;

static int addSpecFile(fa_archive_t* archive, const char* specFile, int compression, int verbose)
{
	fprintf(stderr, "create: Support for spec files not complete\n");
	return -1;
}

static int addFile(fa_archive_t* archive, const char* path, const char* internalPath, int compression, int verbose)
{
#if defined(_WIN32)
	DWORD attrs;
#else
	struct stat fs;
#endif
	fa_file_t* file = NULL;
	FILE* inp = NULL;
	int result = -1;

	if ('@' == *path)
	{
		return addSpecFile(archive, path+1, compression, verbose);
	}

#if defined(_WIN32)
	if ((attrs = GetFileAttributes(path)) == INVALID_FILE_ATTRIBUTES)
	{
		fprintf(stderr, "create: Could not get attributes for file \"%s\"\n", path);
		return -1;
	}

	if (attrs & FILE_ATTRIBUTE_DIRECTORY)
	{
		WIN32_FIND_DATA data;
		char searchPath[MAX_PATH];
		HANDLE dh;
		
		sprintf_s(searchPath, sizeof(searchPath), "%s\\*", path);
		dh = FindFirstFile(searchPath, &data);
		if (dh != INVALID_HANDLE_VALUE)
		{
			do
			{
				char newPath[MAX_PATH];
				char newInternalPath[MAX_PATH];

				if (!strcmp(".", data.cFileName) || !strcmp("..", data.cFileName))
				{
					continue;
				}

				sprintf_s(newPath, sizeof(newPath), "%s\\%s", path, data.cFileName);
				sprintf_s(newInternalPath, sizeof(newPath), "%s\\%s", internalPath, data.cFileName);

				if (addFile(archive, newPath, newInternalPath, compression, verbose) < 0)
				{
					FindClose(dh);
					return -1;
				}
			}
			while (FindNextFile(dh, &data));

			FindClose(dh);
		}

		return 0;
	}
#else
	if (stat(path, &fs) < 0)
	{
		fprintf(stderr, "create: Could not stat file \"%s\"\n", path);
		return -1;
	}

	if (fs.st_mode & S_IFDIR)
	{
		DIR* dir = opendir(path);
		struct dirent* dirEntry;

		if (dir == NULL)
		{
			fprintf(stderr, "create: Could not open directory \"%s\"\n", path);
			return -1;
		}

		while ((dirEntry = readdir(dir)) != NULL)
		{
			char newPath[PATH_MAX];
			char newInternalPath[PATH_MAX];

			if (!strcmp(".", dirEntry->d_name) || !strcmp("..", dirEntry->d_name))
			{
				continue;
			}

			snprintf(newPath, sizeof(newPath), "%s/%s", path, dirEntry->d_name);
			snprintf(newInternalPath, sizeof(newPath), "%s/%s", internalPath, dirEntry->d_name);

			if (addFile(archive, newPath, newInternalPath, compression, verbose) < 0)
			{
				closedir(dir);
				return -1;
			}
		}

		closedir(dir);
		return 0;
	}
#endif

	do
	{
		char buf[16384];
		size_t bufsize;
		size_t insize = 0;
		size_t outsize = 0;

		file = fa_open(archive, internalPath, compression, NULL);
		if (file == NULL)
		{
			fprintf(stderr, "create: Failed opening archive file \"%s\" for writing\n", internalPath);
			break;
		}

		inp = fopen(path, "rb");
		if (inp == NULL)
		{
			fprintf(stderr, "create: Failed opening file \"%s\" for reading\n", path);
			break;
		}

		while ((bufsize = fread(buf, 1, sizeof(buf), inp)) != 0)
		{
			insize += bufsize;

			if (fa_write(file, buf, bufsize) != bufsize)
			{
				fprintf(stderr, "create: Failed writing %lu bytes to archive file \"%s\"\n", bufsize, internalPath);
				break;
			}

			outsize += bufsize;
		}

		if (outsize != insize)
		{
			break;
		}

		result = 0;
	}
	while (0);

	if (inp != NULL)
	{
		fclose(inp);
	}

	if (file != NULL)
	{
		fa_dirinfo_t info;

		if ((fa_close(file, &info) == 0) && (result == 0) && (verbose > 0))
		{
			char hash[sizeof(fa_hash_t) * 2 + 1];
			int i;
			for (i = 0; i < sizeof(fa_hash_t); ++i)
			{
				sprintf(hash + i * 2, "%02x", info.hash.data[i]); 
			}

			fprintf(stderr, "File: \"%s\", %u bytes (%u bytes compressed, ratio %.2f%%), hash: %s\n", info.name, info.size.original, info.size.compressed, info.size.original > 0 ? (info.size.compressed * 100.0f) / info.size.original : 0.0f, hash);
		}
	}

	return result;
}

int commandCreate(int argc, char* argv[])
{
	int i, result;
	State state = State_Options;
	fa_archive_t* archive = NULL;

	fa_compression_t compression = FA_COMPRESSION_NONE;
	uint32_t blockAlignment = 0;
	int verbose = 0;

	result = 0;
	for (i = 2; (i < argc) && (result == 0); ++i)
	{
		switch (state)
		{
			case State_Options:
			{
				if ('-' != argv[i][0])
				{
					state = State_Archive;
					--i;
					break;
				}

				if (!strcmp("-z", argv[i]))
				{
					if (argc == (i+1))
					{
						fprintf(stderr, "create: Missing argument for -z compression argument\n");
						result = -1;
						break;
					}
					++i;

					if (!strcmp("fastlz", argv[i]))
					{
						compression = FA_COMPRESSION_FASTLZ;
					}
					else if (!strcmp("none", argv[i]))
					{
						compression = FA_COMPRESSION_NONE;
					}
#if defined(FA_ZLIB_ENABLE)
					else if (!strcmp("deflate", argv[i]))
					{
						compression = FA_COMPRESSION_DEFLATE;
					}
#endif
#if defined(FA_LZMA_ENABLE)
					else if (!strcmp("lzma2", argv[i]))
					{
						compression = FA_COMPRESSION_LZMA2;
					}
#endif
					else
					{
						fprintf(stderr, "create: Unknown compression method \"%s\"\n", argv[i]);
						result = -1;
						break;
					}
				}
				else if (!strcmp("-v", argv[i]))
				{
					verbose = 1;
				}
				else if (!strcmp("-s", argv[i]))
				{
					blockAlignment = 2048;
				}
				else
				{
					fprintf(stderr, "create: Unknown option \"%s\"\n", argv[i]);
					result = -1;
					break;
				}

			}
			break;

			case State_Archive:
			{
				archive = fa_open_archive(argv[i], FA_MODE_WRITE, blockAlignment, NULL);
				if (archive == NULL)
				{
					fprintf(stderr, "create: Failed to open archive \"%s\" for writing\n", argv[i]);
					result = -1;
					break;
				}
				state = State_Files;
			}
			break;

			case State_Files:
			{
				const char* internalPath = argv[i];
				const char* sep = strrchr(internalPath, '/');
#if defined(_WIN32)
				const char* sep2 = strrchr(internalPath, '\\');
#endif
				if (sep)
				{
					internalPath = sep+1;
				}
#if defined(_WIN32)
				if ((!sep && sep2) || (sep2 > sep))
				{
					internalPath = sep2+1;
				}
#endif
				if (addFile(archive, argv[i], internalPath, compression, verbose) < 0)
				{
					fprintf(stderr, "create: Failed adding file \"%s\"\n", argv[i]);
					result = -1;
					break;
				}
			}
			break;
		}
	}

	if (archive != NULL)
	{
		fa_archiveinfo_t info;
		if (fa_close_archive(archive, compression, &info) < 0)
		{
			fprintf(stderr, "create: Failed to finalize archive\n");
			result = -1;
		}
		else
		{
			char hash[sizeof(fa_hash_t) * 2 + 1];
			int i;

			for (i = 0; i < sizeof(fa_hash_t); ++i)
			{
				sprintf(hash + i * 2, "%02x", info.footer.toc.hash.data[i]);
			}

			fprintf(stderr, "create: Archive constructed successfully\n");
			fprintf(stderr, "Data: %u bytes (%u bytes compressed, ratio %.2f%%)\n", info.footer.data.original, info.footer.data.compressed, info.footer.data.original > 0 ? (info.footer.data.compressed * 100.0f) / info.footer.data.original : 0.0f);
			fprintf(stderr, "TOC: %u bytes (%u bytes compressed, ratio %.2f%%; %u containers, %u entries), hash: %s\n", info.footer.toc.original, info.footer.toc.compressed, info.footer.toc.original > 0 ? (info.footer.toc.compressed * 100.0f) / info.footer.toc.original : 0.0f, info.header.containers.count, info.header.entries.count, hash); 
		}
	}

	return result;
}
