#include <streamer/backend/filearchive.h>
#include <fastlz/fastlz.h>

#include <sha1/sha1.h>

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

typedef enum
{
	State_Options,
	State_Archive,
	State_Files
} State;

struct FileEntry
{
	int compression;
	int offset;

	struct
	{
		int original;
		int compressed;
	} size;

	char* path;
	char* internalPath;
	const char* filePart;

	uint32_t container;

	uint32_t blockSize;

	FileArchiveHash hash;
};

struct FileList
{
	unsigned int count;
	unsigned int capacity;
	struct FileEntry* files;
};

typedef struct CommandContext
{
	uint32_t compression;
	uint32_t blockAlignment;
	uint32_t verbose;

	uint32_t containerCount;
	uint32_t fileCount;

	struct
	{
		uint32_t offset;
		struct
		{
			uint32_t original;
			uint32_t compressed;
		} size;
	} data, header, footer;
} CommandContext;

static int addSpecFile(struct FileList* files, const char* specFile, int compression)
{
	fprintf(stderr, "create: Support for spec files not complete\n");
	return -1;
}

static int addFile(struct FileList* files, const char* path, const char* internalPath, int compression)
{
	struct FileEntry* entry;
#if defined(_WIN32)
	DWORD attrs;
#else
	struct stat fs;
#endif
	char* curr;

	if ('@' == *path)
	{
		return addSpecFile(files, path+1, compression);
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

				if (addFile(files, newPath, newInternalPath, compression) < 0)
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

			if (addFile(files, newPath, newInternalPath, compression) < 0)
			{
				closedir(dir);
				return -1;
			}
		}

		closedir(dir);
		return 0;
	}
#endif

	if (files->count == files->capacity)
	{
		int newCapacity = files->capacity < 128 ? 128 : files->capacity * 2;
		files->files = realloc(files->files, newCapacity * sizeof(struct FileEntry));
		memset(files->files + sizeof(struct FileEntry) * files->capacity, 0, sizeof(struct FileEntry) * (newCapacity - files->capacity));
		files->capacity = newCapacity;
	}

	entry = &(files->files[files->count++]);

	entry->compression = compression;
	entry->path = strdup(path);
	entry->internalPath = strdup(internalPath);
	entry->container = FILEARCHIVE_INVALID_OFFSET;

	for (curr = entry->internalPath; *curr; ++curr)
	{
#if defined(_WIN32)
		if (*curr == '\\')
		{
			*curr = '/';
		}
#endif
		*curr = (char)tolower(*curr);
	}

	entry->filePart = (strrchr(entry->internalPath, '/') != NULL) ? strrchr(entry->internalPath, '/') + 1 : entry->internalPath;

	return 0;
}

static void freeFiles(struct FileList* files)
{
	unsigned int i;
	for (i = 0; i < files->count; ++i)
	{
		free(files->files[i].path);
		free(files->files[i].internalPath);
	}

	free(files->files);
}

#define COMPRESS_RESULT_ERROR (0xFFFFFFFF)
typedef uint32_t (*CompressCallback)(void* buffer, uint32_t maxLength, void* userData);
static uint32_t compressStream(FILE* outp, uint32_t compression, void* inbuf, uint32_t inlen, void* outbuf, uint32_t outlen, CompressCallback callback, void* userData)
{
	uint32_t total = 0;
	for(;;)
	{
		uint32_t size = callback(inbuf, inlen, userData);
		if (size == 0)
		{
			break;
		}

		if (compression == FILEARCHIVE_COMPRESSION_NONE)
		{
			if (fwrite(inbuf, 1, size, outp) != size)
			{
				return COMPRESS_RESULT_ERROR;
			}
			total += size;
		}
		else
		{
			int result = -1;
			FileArchiveCompressedBlock block;

			switch (compression)
			{
				default: case FILEARCHIVE_COMPRESSION_NONE: break;
				case FILEARCHIVE_COMPRESSION_FASTLZ:
				{
					if (size < 16)
					{
						break;
					}

					result = fastlz_compress_level(2, inbuf, size, outbuf);
					if (result >= (int)size)
					{
						result = -1;
					}	
				}
			}

			if (result >= 0)
			{
				block.original = (uint16_t)size;
				block.compressed = (uint16_t)result;

				if (fwrite(&block, 1, sizeof(block), outp) != sizeof(block))
				{
					return COMPRESS_RESULT_ERROR;
				}

				if (fwrite(outbuf, 1, result, outp) != ((size_t)result))
				{
					return COMPRESS_RESULT_ERROR;
				}

				total += sizeof(block) + result;
			}
			else
			{
				block.original = (uint16_t)size;
				block.compressed = (uint16_t)(size | FILEARCHIVE_COMPRESSION_SIZE_IGNORE);

				if (fwrite(&block, 1, sizeof(block), outp) != sizeof(block))
				{
					return COMPRESS_RESULT_ERROR;
				}

				if (fwrite(inbuf, 1, size, outp) != (size_t)size)
				{
					return COMPRESS_RESULT_ERROR;
				}

				total += sizeof(block) + size;
			}
		}
	}

	return total;
}

static uint32_t compressData(FILE* outp, uint32_t compression, CompressCallback callback, void* data, uint32_t blockSize)
{
	char* inbuf = malloc(blockSize);
	char* outbuf = malloc(blockSize * 2);

	uint32_t result = compressStream(outp, compression, inbuf, blockSize, outbuf, blockSize * 2, callback, data);

	free(inbuf);
	free(outbuf);

	return result;
}

static int writePadding(FILE* outp, uint32_t size)
{
	char buf[1024];
	uint32_t curr = 0;

	memset(buf, 0, sizeof(buf));

	while (curr != size)
	{
		uint32_t bytesMax = (size - curr) > sizeof(buf) ? sizeof(buf) : size-curr;
		if (fwrite(buf, 1, bytesMax, outp) != bytesMax)
		{
			return -1;
		}
		curr += bytesMax;
	}

	return 0;
}

struct FileCompressData
{
	FILE* inp;
	uint32_t totalRead;

	SHA1Context state;
};

static uint32_t fileCompressCallback(void* buffer, uint32_t maxLength, void* userData)
{
	struct FileCompressData* data = (struct FileCompressData*)userData;
	int result = fread(buffer, 1, maxLength, data->inp);
	SHA1Input(&(data->state), buffer, result);
	data->totalRead += result;
	return result;
}

static uint32_t writeFiles(FILE* outp, struct FileList* files, uint32_t offset, CommandContext* context)
{
	unsigned int i;
	FILE* inp = NULL;

	size_t dataRead = 0;
	size_t dataWrite = 0;

	context->data.offset = offset;

	for (i = 0; i < files->count; ++i)
	{
		struct FileEntry* entry = &(files->files[i]);
		struct FileCompressData data;
		size_t totalRead = 0, totalWritten = 0;
		unsigned int j,k;

		inp = fopen(entry->path, "rb");
		if (inp == NULL)
		{
			fprintf(stderr, "create: Failed opening \"%s\" for reading\n", entry->path);
			break;
		}

		if (context->blockAlignment != 0)
		{
			uint32_t bytes = offset % context->blockAlignment;
			if (writePadding(outp, bytes) < 0)
			{
				fprintf(stderr, "create: Failed writing %u bytes to archive.\n", (unsigned int)bytes);
				break;
			}
			offset += bytes;
			dataWrite += bytes;
		}

		data.inp = inp;
		data.totalRead = 0;
		SHA1Reset(&(data.state));

		totalWritten = compressData(outp, entry->compression, fileCompressCallback, &data, FILEARCHIVE_COMPRESSION_BLOCK_SIZE);
		if (totalWritten == COMPRESS_RESULT_ERROR)
		{
			fprintf(stderr, "create: Failed to add \"%s\" to archive.\n", entry->path);
			break;
		} 
		totalRead = data.totalRead;

		entry->offset = offset;
		entry->size.original = totalRead;
		entry->size.compressed = totalWritten;
		entry->blockSize = FILEARCHIVE_COMPRESSION_BLOCK_SIZE;
		SHA1Result(&(data.state));
		for (j = 0; j < 5; ++j)
		{
			for (k = 0; k < 4; ++k)
			{
				uint8_t value = (uint8_t)((data.state.Message_Digest[j] >> ((3-k) * 8)) & 0xff);
				entry->hash.data[j * 4 + k] = value;
			}
		}
		offset += totalWritten;

		dataRead += totalRead;
		dataWrite += totalWritten;

		if (inp)
		{
			if (context->verbose > 0)
			{
				fprintf(stderr, "Added file \"%s\" (%u bytes) (as \"%s\", %u bytes), %s\n", entry->path, (unsigned int)totalRead, entry->internalPath, (unsigned int)totalWritten, entry->compression != FILEARCHIVE_COMPRESSION_NONE ? "(compressed)" : "(raw)");
			}

			fclose(inp);
			inp = NULL;
		}
	}

	context->data.size.original = dataRead;
	context->data.size.compressed = dataWrite;

	if (inp != NULL)
	{
		fclose(inp);
	}

	return (i == files->count) ? offset : 0;
}

static uint32_t findContainer(const char* path, const FileArchiveContainer* root, const char* stringBuffer)
{
	const char* curr = path;
	const char* term;
	uint32_t offset = 0;

	while ((term = strchr(curr, '/')) != NULL)
	{
		size_t nlen = (term-curr);
		const FileArchiveContainer* parent = (const FileArchiveContainer*)(((const char*)root) + offset);
		uint32_t child = parent->children;

		for (child = parent->children; child != FILEARCHIVE_INVALID_OFFSET;)
		{
			const FileArchiveContainer* childContainer = (const FileArchiveContainer*)(((const char*)root) + child);
			const char* name = childContainer->name != FILEARCHIVE_INVALID_OFFSET ? stringBuffer + childContainer->name : NULL;

			if (name && (strlen(name) == nlen) && !memcmp(name, curr, nlen))
			{
				break;
			}

			child = childContainer->next;
		}

		if (child == FILEARCHIVE_INVALID_OFFSET)
		{
			return FILEARCHIVE_INVALID_OFFSET;
		}

		offset = child;
		curr = term+1;
	}

	return offset;
}

static uint32_t relocateOffset(uint32_t offset, uint32_t delta)
{
	return offset != FILEARCHIVE_INVALID_OFFSET ? offset + delta : FILEARCHIVE_INVALID_OFFSET;
}

struct HeaderCompressData
{
	struct
	{
		const char* begin;
		const char* end;
	} blocks[5]; // header, containers, files, hashes, strings

	uint32_t totalRead;
};

uint32_t headerCompressCallback(void* buffer, uint32_t maxLength, void* userData)
{
	struct HeaderCompressData* data = (struct HeaderCompressData*)userData; 
	unsigned int i;
	size_t written = 0;

	for (i = 0; i < 5; ++i)
	{
		size_t size = data->blocks[i].end - data->blocks[i].begin;
		size_t blockSize = maxLength < size ? maxLength : size;

		memcpy(buffer, data->blocks[i].begin, blockSize);

		data->blocks[i].begin += blockSize;
		buffer = (((char*)buffer) + blockSize);
		maxLength -= blockSize;
		written += blockSize;
	}

	data->totalRead += written;
	return written;
}

static uint32_t writeHeader(FILE* outp, struct FileList* files, uint32_t offset, CommandContext* context)
{
	size_t containerCapacity = 128, containerCount = 0;
	FileArchiveContainer* containerEntries = malloc(containerCapacity * sizeof(FileArchiveContainer));
	unsigned int i;

	size_t stringCapacity = 1024, stringSize = 0;
	char* stringBuffer = malloc(stringCapacity);

	FileArchiveEntry* fileEntries = NULL; 
	uint32_t fileCount = 0;

	FileArchiveHeader header;
	struct HeaderCompressData data;

	FileArchiveHash* hashes = NULL;

	uint32_t result = 0;

	memset(&data, 0, sizeof(data));

	do
	{
		if (context->blockAlignment != 0)
		{
			unsigned int bytes = offset % context->blockAlignment;
			if (writePadding(outp, bytes) < 0)
			{
				fprintf(stderr, "Failed writing %u bytes to archive.\n", bytes);
				offset = 0;
				break;
			}

			offset += bytes;
		}

		/* construct containers */

		containerEntries->parent = FILEARCHIVE_INVALID_OFFSET;
		containerEntries->children = FILEARCHIVE_INVALID_OFFSET;
		containerEntries->next = FILEARCHIVE_INVALID_OFFSET;
		containerEntries->files = FILEARCHIVE_INVALID_OFFSET;
		containerEntries->count = 0;
		containerEntries->name = FILEARCHIVE_INVALID_OFFSET;
		++ containerCount;

		for (i = 0; i < files->count; ++i)
		{
			struct FileEntry* entry = &(files->files[i]);
			char* path = entry->internalPath;
			char* curr = path;
			char* offset = NULL;
			uint32_t parent = 0;

			while ((offset = strchr(curr, '/')) != NULL)
			{
				uint32_t actual;
				char temp = *(offset+1);
				*(offset+1) = 0;

				if ((actual = findContainer(path, containerEntries, stringBuffer)) == FILEARCHIVE_INVALID_OFFSET)
				{
					size_t nlen = strlen(curr);
					FileArchiveContainer* container;
					FileArchiveContainer* parentContainer;

					if (containerCapacity == containerCount)
					{
						containerCapacity *= 2;
						containerEntries = realloc(containerEntries, containerCapacity * sizeof(FileArchiveContainer));
					}

					container = &containerEntries[containerCount++];
					parentContainer = (FileArchiveContainer*)(((uint8_t*)containerEntries) + parent);

					/* TODO: attach new container to end of sibling list instead */

					container->parent = parent;
					container->children = FILEARCHIVE_INVALID_OFFSET;
					container->next = parentContainer->children;
					container->files = FILEARCHIVE_INVALID_OFFSET;
					container->count = 0;
					container->name = stringSize;
					if (stringSize + nlen > stringCapacity)
					{
						stringCapacity = stringCapacity * 2 < stringSize + nlen ? stringSize + nlen : stringCapacity * 2;
						stringBuffer = realloc(stringBuffer, stringCapacity);
					}
					memcpy(stringBuffer + stringSize, curr, nlen);
					*(stringBuffer + stringSize + nlen -1) = '\0';
					stringSize += nlen;

					parentContainer->children = parent = (container - containerEntries) * sizeof(FileArchiveContainer);
				}
				else
				{
					parent = actual;
				}

				*(offset+1) = temp;
				curr = offset+1;
			}
		}

		/* construct files */

		fileEntries = malloc(sizeof(FileArchiveEntry) * files->count);
		hashes = malloc(sizeof(FileArchiveHash) * files->count);
		memset(fileEntries, 0, sizeof(FileArchiveEntry) * files->count);

		for (i = 0; i < files->count; ++i)
		{
			struct FileEntry* entry = &(files->files[i]);
			uint32_t offset = findContainer(entry->internalPath, containerEntries, stringBuffer);
			if (offset == FILEARCHIVE_INVALID_OFFSET)
			{
				fprintf(stderr, "create: Failed to resolve container for file \"%s\" while constructing index\n", entry->internalPath);
				break;
			}

			entry->container = offset;
		}

		if (i != files->count)
		{
			offset = 0;
			break;
		}

		for (i = 0; i < containerCount; ++i)
		{
			unsigned int j;
			FileArchiveContainer* container = &(containerEntries[i]);
			uint32_t containerOffset = ((char*)container - (char*)containerEntries);

			for (j = 0; j < files->count; ++j)
			{
				struct FileEntry* source = &(files->files[j]);
				FileArchiveEntry* file = &(fileEntries[fileCount]);
				uint32_t fileOffset = ((char*)file - (char*)fileEntries);
				size_t nlen = strlen(source->filePart) + 1;
				FileArchiveHash* hash = hashes + fileCount;

				if (source->container != containerOffset)
				{
					continue;
				}

				if (container->files == FILEARCHIVE_INVALID_OFFSET)
				{
					container->files = fileOffset;
				}

				++ container->count;

				file->data = source->offset;
				file->name = stringSize;
				file->compression = source->compression;
				file->size.original = source->size.original;
				file->size.compressed = source->size.compressed;
				file->blockSize = (uint16_t)source->blockSize;

				if (stringCapacity < (stringSize + nlen))
				{
					stringCapacity = stringCapacity * 2 < (stringSize + nlen) ? stringSize + nlen : stringCapacity * 2;
					stringBuffer = realloc(stringBuffer, stringCapacity);
				}
				memcpy(stringBuffer + stringSize, source->filePart, nlen);
				stringSize += nlen;

				*hash = source->hash;

				++ fileCount;
			}
		}

		if ((i != containerCount) || (fileCount != files->count))
		{
			fprintf(stderr, "create: Failed to assign all file entries.\n");
			offset = 0;
			break;
		}

		/* relocate and write blocks */

		for (i = 0; i < containerCount; ++i)
		{
			FileArchiveContainer* container = &(containerEntries[i]);

			container->parent = relocateOffset(container->parent, sizeof(FileArchiveHeader));
			container->children = relocateOffset(container->children, sizeof(FileArchiveHeader));
			container->next = relocateOffset(container->children, sizeof(FileArchiveHeader));

			container->files = relocateOffset(container->files, sizeof(FileArchiveHeader) + containerCount * sizeof(FileArchiveContainer));
			container->name = relocateOffset(container->name, sizeof(FileArchiveHeader) + containerCount * sizeof(FileArchiveContainer) + fileCount * sizeof(FileArchiveEntry) + fileCount * sizeof(FileArchiveHash));
		}

		for (i = 0; i < fileCount; ++i)
		{
			FileArchiveEntry* file = &(fileEntries[i]);

			file->name = relocateOffset(file->name, sizeof(FileArchiveHeader) + containerCount * sizeof(FileArchiveContainer) + fileCount * sizeof(FileArchiveEntry) + fileCount * sizeof(FileArchiveHash));
		}

		header.cookie = FILEARCHIVE_MAGIC_COOKIE;
		header.version = FILEARCHIVE_VERSION_CURRENT;
		header.size = sizeof(FileArchiveHeader) + containerCount * sizeof(FileArchiveContainer) + fileCount * sizeof(FileArchiveEntry) + fileCount * sizeof(FileArchiveHash) + stringSize;
		header.flags = 0;

		header.containers = sizeof(FileArchiveHeader);
		header.containerCount = containerCount;

		header.files = sizeof(FileArchiveHeader) + containerCount * sizeof(FileArchiveContainer);
		header.fileCount = fileCount;
		header.hashes = header.files + fileCount * sizeof(FileArchiveEntry);

		data.blocks[0].begin = (const void*)&header;
		data.blocks[0].end = (const void*)(&header + 1);

		data.blocks[1].begin = (const void*)containerEntries;
		data.blocks[1].end = (const void*)(containerEntries + containerCount);

		data.blocks[2].begin = (const void*)fileEntries;
		data.blocks[2].end = (const void*)(fileEntries + fileCount);

		data.blocks[3].begin = (const void*)hashes;
		data.blocks[3].end = (const void*)(hashes + fileCount);

		data.blocks[4].begin = (const void*)stringBuffer;
		data.blocks[4].end = (const void*)(stringBuffer + stringSize);

		result = compressData(outp, context->compression, headerCompressCallback, &data, FILEARCHIVE_COMPRESSION_BLOCK_SIZE); 
		if (result == COMPRESS_RESULT_ERROR)
		{
			fprintf(stderr, "create: Failed writing header to archive.\n");
			offset = 0;
			break;
		}

		context->containerCount = containerCount;
		context->fileCount = fileCount; 
		context->header.offset = offset;
		context->header.size.original = data.totalRead;
		context->header.size.compressed = result;
		offset += result;
	}
	while (0);

	free(fileEntries);
	free(containerEntries);
	free(hashes);
	free(stringBuffer);

	return offset;
}

uint32_t writeFooter(FILE* outp, uint32_t offset, CommandContext* context)
{
	FileArchiveFooter footer;

	footer.cookie = FILEARCHIVE_MAGIC_COOKIE;
	footer.toc = offset - context->header.offset;
	footer.data = offset - context->data.offset;
	footer.compression = context->compression;
	footer.size.original = context->header.size.original;
	footer.size.compressed = context->header.size.compressed;

	if (fwrite(&footer, 1, sizeof(footer), outp) != sizeof(footer))
	{
		fprintf(stderr, "create: Failed to write %u bytes to archive.\n", (unsigned int)sizeof(footer));
		return 0;
	}

	context->footer.offset = offset;
	context->footer.size.original = context->footer.size.compressed = sizeof(footer);

	return offset + sizeof(footer);
} 

int commandCreate(int argc, char* argv[])
{
	int i, result;
	State state = State_Options;
	struct FileList files = { 0 };
	const char* archive = NULL;
	FILE* archp = NULL;

	uint32_t offset = 0;
	CommandContext context = { 0 };
	context.compression = FILEARCHIVE_COMPRESSION_NONE;
	context.blockAlignment = 0;

	for (i = 2; i < argc; ++i)
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
						return -1;
					}
					++i;

					if (!strcmp("fastlz", argv[i]))
					{
						context.compression = FILEARCHIVE_COMPRESSION_FASTLZ;
					}
					else if (!strcmp("none", argv[i]))
					{
						context.compression = FILEARCHIVE_COMPRESSION_NONE;
					}
					else
					{
						fprintf(stderr, "create: Unknown compression method \"%s\"\n", argv[i]);
						return -1;
					}
				}
				else if (!strcmp("-v", argv[i]))
				{
					context.verbose = 1;
				}
				else if (!strcmp("-s", argv[i]))
				{
					context.blockAlignment = 2048;
				}
				else
				{
					fprintf(stderr, "create: Unknown option \"%s\"\n", argv[i]);
					return -1;
				}

			}
			break;

			case State_Archive:
			{
				archive = argv[i];
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
				if (addFile(&files, argv[i], internalPath, context.compression) < 0)
				{
					fprintf(stderr, "create: Failed adding file \"%s\"\n", argv[i]);
					freeFiles(&files);
					return -1;
				}
			}
			break;
		}
	}

	result = -1;
	do
	{
		if (state != State_Files)
		{
			fprintf(stderr, "create: Too few arguments\n");
			break;
		}

		if (files.count == 0)
		{
			fprintf(stderr, "create: No files available for use\n");
			break;
		}

		archp = fopen(archive, "wb");
		if (archp == NULL)
		{
			fprintf(stderr, "create: Could not open archive \"%s\" for writing\n", archive);
			break;
		}

		if ((offset = writeFiles(archp, &files, offset, &context)) == 0)
		{
			fprintf(stderr, "create: Failed writing files to archive\n");
			break;
		}

		if ((offset = writeHeader(archp, &files, offset, &context)) == 0)
		{
			fprintf(stderr, "create: Failed writing header to archive\n");
			break;
		}

		if ((offset = writeFooter(archp, offset, &context)) == 0)
		{
			fprintf(stderr, "create: Failed writing footer to archive\n");
			break;
		}

		fprintf(stderr, "Archive \"%s\" created successfully. Statistics: \n", archive);
		fprintf(stderr, "   Containers: %u Files: %u\n", context.containerCount, context.fileCount);
		fprintf(stderr, "   Data: %u bytes, %u bytes uncompressed (ratio: %.2f%%)\n", context.data.size.compressed, context.data.size.original, (1.0f-((float)context.data.size.compressed / (float)context.data.size.original)) * 100.0f);
		fprintf(stderr, "   TOC: %u bytes, %u bytes uncompressed (ratio: %.2f%%)\n", context.header.size.compressed, context.header.size.original, (1.0f-((float)context.header.size.compressed / (float)context.header.size.original)) * 100.0f);
		fprintf(stderr, "   Footer: %u bytes\n", context.footer.size.original);

		result = 0;
	}
	while (0);

	if (archp != NULL)
	{
		fclose(archp);
	}
	freeFiles(&files);

	return result;
}
