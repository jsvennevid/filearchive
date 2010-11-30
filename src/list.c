#include <streamer/backend/filearchive.h>
#include <fastlz/fastlz.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#pragma warning(disable: 4127)
#endif

static char* buildContainerName(const FileArchiveContainer* container, const void* toc, unsigned int extraSize)
{
	const char* name = container->name != FILEARCHIVE_INVALID_OFFSET ? ((const char*)toc) + container->name : "";
	int len = strlen(name), blen;
	char* buf;

	if (container->parent != FILEARCHIVE_INVALID_OFFSET)
	{
		buf = buildContainerName((const FileArchiveContainer*)(((const char*)toc) + container->parent), toc, extraSize + len + 1);
		blen = strlen(buf);

		memcpy(buf + blen, name, len);
		buf[blen + len] = '/';
		buf[blen + len + 1] = '\0';
		return buf;
	}

	buf = malloc(len + 1 + extraSize);
	memcpy(buf, name, len + 1);

	return buf;
}

static int decompressStream(FILE* inp, void* out, size_t compressedSize, size_t originalSize, uint32_t compression, uint32_t blockSize)
{
	char* buffer = malloc(blockSize + sizeof(FileArchiveCompressedBlock));
	size_t bufferUse = 0;
	size_t curr, used;

	if (compression == FILEARCHIVE_COMPRESSION_NONE)
	{
		if (fread(out, 1, originalSize, inp) != originalSize)
		{
			return -1;
		}

		return 0;
	}

	for (curr = 0; curr != compressedSize;)
	{
		size_t maxSize = (compressedSize - curr) < ((blockSize - bufferUse) + sizeof(FileArchiveCompressedBlock)) ? compressedSize - curr : (blockSize - bufferUse) + sizeof(FileArchiveCompressedBlock);
		FileArchiveCompressedBlock* block = (FileArchiveCompressedBlock*)buffer;

		if (fread(buffer + bufferUse, 1, maxSize, inp) != maxSize)
		{
			return -1;
		}
		bufferUse += maxSize;

		if (block->compressed & FILEARCHIVE_COMPRESSION_SIZE_IGNORE)
		{
			memcpy(out, buffer + sizeof(FileArchiveCompressedBlock), block->original);
			used = block->original + sizeof(FileArchiveCompressedBlock);
		}
		else
		{
			switch (compression)
			{
				case FILEARCHIVE_COMPRESSION_FASTLZ: 
				{
					if (fastlz_decompress(buffer + sizeof(FileArchiveCompressedBlock), block->compressed, out, block->original) != block->original)
					{
						return -1;
					}
				}
				break;

				default:
				{
					return -1;
				}
				break;
			}
			used = block->compressed + sizeof(FileArchiveCompressedBlock);
		}

		memmove(buffer, buffer + used, bufferUse - used);
		bufferUse -= used;

		out = ((char*)out) + used;
		curr += used;
	}

	return 0;
} 

static int listArchive(const char* path)
{
	FILE* inp = NULL;
	int result = -1;
	FileArchiveFooter footer;
	unsigned int i;

	FileArchiveHeader* header = NULL;
	char* toc = NULL;

	size_t compressedTotal = 0;
	size_t uncompressedTotal = 0;

	do
	{
#if defined(_WIN32)
		if (fopen_s(&inp, path, "rb") != 0)
#else
		if ((inp = fopen(path, "rb")) == NULL)
#endif
		{
			fprintf(stderr, "list: Failed to open archive \"%s\"\n", path);
			break;
		}

		if (fseek(inp, -((int)sizeof(FileArchiveFooter)), SEEK_END) < 0)
		{
			fprintf(stderr, "list: Could not seek to end of file\n");
			break;
		}

		if (fread(&footer, 1, sizeof(footer), inp) != sizeof(footer))
		{
			fprintf(stderr, "list: Failed to read footer\n");
			break;
		}

		if (footer.cookie != FILEARCHIVE_MAGIC_COOKIE)
		{
			fprintf(stderr, "list: Mismatching cookie\n");
			break;
		}

		toc = malloc(footer.size.original);
		if (toc == NULL)
		{
			fprintf(stderr, "list: Failed allocating TOC (%u bytes)\n", footer.size.original);
			break;
		}

		if (fseek(inp, -((int)(sizeof(FileArchiveFooter) + footer.toc)), SEEK_END) < 0)
		{
			fprintf(stderr, "list: Could not seek to TOC\n");
			break;
		}

		if (decompressStream(inp, toc, footer.size.compressed, footer.size.original, footer.compression, FILEARCHIVE_COMPRESSION_BLOCK_SIZE) < 0)
		{
			fprintf(stderr, "list: Failed to load TOC\n");
			break;
		}

		header = (FileArchiveHeader*)toc;
		if (header->cookie != FILEARCHIVE_MAGIC_COOKIE)
		{
			fprintf(stderr, "list: Header cookie mismatch\n");
			break;
		}

		if (header->version > FILEARCHIVE_VERSION_CURRENT)
		{
			fprintf(stderr, "list: Unsupported version %u (maximum supported: %u)\n", (unsigned int)header->version, (unsigned int)FILEARCHIVE_VERSION_CURRENT);
			break;
		}

		if (header->size != footer.size.original)
		{
			fprintf(stderr, "list: Header size mismatch from footer block (%u != %u)\n", (unsigned int)header->size, (unsigned int)footer.size.original);
			break;
		}

		fprintf(stdout, "File archive version %u:\n", header->version);
		for (i = 0; i < header->fileCount; ++i)
		{
			const FileArchiveEntry* file = ((const FileArchiveEntry*)(toc + header->files)) + i;

			compressedTotal += file->size.compressed;
			uncompressedTotal += file->size.original;
		}

		fprintf(stdout, "   Containers: %u Files: %u\n", header->containerCount, header->fileCount);
		fprintf(stdout, "   Data: %u bytes, %u uncompressed (%.2f%% ratio)\n", (unsigned int)compressedTotal, (unsigned int)uncompressedTotal, (1.0-(((float)compressedTotal) / ((float)uncompressedTotal))) * 100.0f);
		fprintf(stdout, "   TOC: %u bytes, %u uncompressed (%.2f%% ratio)\n", (unsigned int)footer.size.compressed, (unsigned int)footer.size.original, (1.0f-(((float)footer.size.compressed) / ((float)footer.size.original))) * 100.0f);
		fprintf(stdout, "   Footer: %u bytes\n", (unsigned int)sizeof(footer));

		for (i = 0; i < header->containerCount; ++i)
		{
			const FileArchiveContainer* container = ((const FileArchiveContainer*)(toc + header->containers)) + i; 
			char* name;
			unsigned int j;

			name = buildContainerName(container, toc, 0);

			fprintf(stdout, "%s\n", name);

			for (j = 0; j < container->count; ++j)
			{
				const FileArchiveEntry* file = ((const FileArchiveEntry*)(toc + container->files)) + j;
				const FileArchiveHash* hash = ((const FileArchiveHash*)(toc + header->hashes)) + (file - ((const FileArchiveEntry*)(toc + header->files)));
				char hashbuf[sizeof(hash->data) * 2 + 1];
				char* name = toc + file->name;
				unsigned int k;

				hashbuf[0] = '\0';
				for (k = 0; k < sizeof(hash->data); ++k)
				{
#if defined(_WIN32)
					sprintf_s(hashbuf, sizeof(hashbuf), "%s%02x", hashbuf, hash->data[k]);
#else
					sprintf(hashbuf, "%s%02x", hashbuf, hash->data[k]);
#endif
				}

				fprintf(stdout, "   %s (%u bytes, %.2f%% ratio) - SHA-1: %s\n", name, file->size.original, (1.0f-(((float)file->size.compressed) / ((float)file->size.original))) * 100.0f, hashbuf);
			}

			free(name);
		}

		result = 0;
	}
	while (0);

	fclose(inp);
	free(toc);
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
