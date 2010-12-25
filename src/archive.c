#include "../internal/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static fa_archive_t* openArchiveReading(const char* filename);
static fa_archive_t* openArchiveWriting(const char* filename, uint32_t alignment);

static int writeToc(fa_archive_writer_t* archive, fa_compression_t compression);

static fa_offset_t findContainer(const char* path, const fa_container_t* containers, const char* strings);

fa_archive_t* fa_open_archive(const char* filename, fa_mode_t mode, uint32_t alignment)
{
	fa_archive_t* archive = NULL;

	switch (mode)
	{
		case FA_MODE_READ:
		{
			archive = openArchiveReading(filename);
		}
		break;

		case FA_MODE_WRITE:
		{
			archive = openArchiveWriting(filename, alignment);
		}
		break;
	}

	archive->cache.data = malloc(FA_ARCHIVE_CACHE_SIZE);

	return archive;
}

int fa_close_archive(fa_archive_t* archive, fa_compression_t compression)
{
	fa_archive_writer_t* writer;

	if (archive == NULL)
	{
		return -1;
	}

	if (archive->mode == FA_MODE_READ)
	{
		fclose((FILE*)archive->fd);
		free(archive->cache.data);
		free(archive->toc);
		free(archive);
		return 0;
	}

	writer = (fa_archive_writer_t*)archive;

	if (writeToc(writer, compression))
	{
		return -1;
	}

	fclose((FILE*)archive->fd);	
	free(archive->cache.data);
	free(writer->entries.data);

	return 0;
}

static fa_archive_t* openArchiveReading(const char* filename)
{
	fa_archive_t* archive = malloc(sizeof(fa_archive_t));
	memset(archive, 0, sizeof(fa_archive_t));

	archive->mode = FA_MODE_READ;

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

static fa_archive_t* openArchiveWriting(const char* filename, uint32_t alignment)
{
	fa_archive_writer_t* writer = malloc(sizeof(fa_archive_writer_t));
	memset(writer, 0, sizeof(fa_archive_writer_t));

	writer->archive.mode = FA_MODE_WRITE;

	do
	{
		writer->archive.fd = fopen(filename, "wb");
		if (writer->archive.fd == NULL)
		{
			break;
		}

		writer->alignment = alignment;

		return &(writer->archive);
	}
	while (0);

	free(writer);
	return NULL;
}

static fa_offset_t relocateOffset(fa_offset_t offset, fa_offset_t delta)
{
	return offset != FA_INVALID_OFFSET ? offset + delta : FA_INVALID_OFFSET;
}

static int writeToc(fa_archive_writer_t* writer, fa_compression_t compression)
{
	struct
	{
		size_t count;
		size_t capacity;
		char* data;
	} strings = { 0, 32658, malloc(32768) };

	struct
	{
		size_t count;
		size_t capacity;
		fa_container_t* data;
	} containers = { 0, 1024, malloc(1024 * sizeof(fa_container_t)) };

	struct
	{
		size_t count;
		size_t capacity;
		fa_entry_t* data;
		fa_hash_t* hashes;
	} entries = { 0, 1024, malloc(1024 * sizeof(fa_entry_t)), malloc(1024 * sizeof(fa_hash_t)) };

	int result = -1;

	do
	{
		int i, count;
		fa_header_t header;
		fa_footer_t footer;

		struct
		{
			size_t original;
			size_t compressed;
		} written = { 0, 0 };

		struct
		{
			void* data;
			size_t size;
		} blocks[5]; // header, containers, files, hashes, strings

		// construct containers

		containers.data->parent = FA_INVALID_OFFSET;
		containers.data->children = FA_INVALID_OFFSET;
		containers.data->next = FA_INVALID_OFFSET;

		containers.data->name = FA_INVALID_OFFSET;
		containers.data->files.offset = FA_INVALID_OFFSET;
		containers.data->files.count = 0;
		++ containers.count;

		for (i = 0, count = writer->entries.count; i < count; ++i)
		{
			fa_writer_entry_t* entry = &(writer->entries.data[i]);
			char* path = entry->path;
			char* curr = path;
			char* offset = NULL;
			fa_offset_t parent = 0;

			while ((offset = strchr(curr, '/')) != NULL)
			{
				fa_container_t* parentContainer;
				fa_offset_t actual;
				char temp = *(offset + 1);

				*(offset + 1) = '\0';

				parentContainer = (fa_container_t*)(((uint8_t*)containers.data) + parent);
				actual = findContainer(path, containers.data, strings.data);

				if (actual == FA_INVALID_OFFSET)
				{
					fa_container_t* container;
					size_t nlen = strlen(curr);
					char term = *(offset);
					*offset = '\0';

					fprintf(stderr, "%s\n", curr);

					if (containers.count == containers.capacity)
					{
						containers.capacity *= 2;
						containers.data = realloc(containers.data, containers.capacity * sizeof(fa_container_t));
					}

					container = &(containers.data[containers.count++]);

					container->parent = parent;
					container->children = FA_INVALID_OFFSET;
					container->next = parentContainer->children;
					container->name = strings.count;

					if ((strings.count + nlen) > strings.capacity)
					{
						strings.capacity *= 2;
						strings.data = realloc(strings.data, strings.capacity);
					}

					memcpy(strings.data + strings.count, curr, nlen);
					strings.count += nlen;

					container->files.offset = FA_INVALID_OFFSET;
					container->files.count = 0;

					parentContainer->children = parent = (container - containers.data) * sizeof(fa_container_t);

					*offset = term;
				}
				else
				{
					parent = actual;
				}

				*(offset + 1) = temp;	
				curr = offset + 1;
			}
		}

		// construct entries

		for (i = 0, count = writer->entries.count; i < count; ++i)
		{
			fa_writer_entry_t* entry = &(writer->entries.data[i]);
			fa_offset_t offset = findContainer(entry->path, containers.data, strings.data);

			if (offset == FA_INVALID_OFFSET)
			{
				break;
			}

			entry->container = offset;
		}

		if (i != count)
		{
			break;
		}

		for (i = 0, count = containers.count; i < count; ++i)
		{
			fa_container_t* container = &(containers.data[i]);
			fa_offset_t containerOffset = (container - containers.data) * sizeof(fa_container_t);
			size_t j;

			for (j = 0; j < writer->entries.count; ++j)
			{
				const fa_writer_entry_t* writerEntry = &(writer->entries.data[j]);
				const char* name;
				size_t nlen;
				fa_entry_t* entry;

				if (writerEntry->container != containerOffset)
				{
					continue;
				}

				name = strrchr(writerEntry->path, '/') != NULL ? strrchr(writerEntry->path, '/') + 1 : writerEntry->path;
				nlen = strlen(name) + 1;

				if (container->files.offset == FA_INVALID_OFFSET)
				{
					container->files.offset = entries.count * sizeof(fa_entry_t);
				}

				if (entries.count == entries.capacity)
				{
					entries.capacity *= 2;
					entries.data = realloc(entries.data, entries.capacity * sizeof(fa_entry_t));
					entries.hashes = realloc(entries.hashes, entries.capacity * sizeof(fa_hash_t));
				}

				entry = &(entries.data[entries.count++]);

				entry->data = writerEntry->offset;
				entry->name = strings.count;

				if (strings.count + nlen > strings.capacity)
				{
					strings.capacity *= 2;
					strings.data = realloc(strings.data, strings.capacity);
				}

				memcpy(strings.data + strings.count, name, nlen);
				strings.count += nlen;

				entry->compression = writerEntry->compression;
				entry->blockSize = FA_COMPRESSION_MAX_BLOCK;

				entry->size.original = writerEntry->size.original;
				entry->size.compressed = writerEntry->size.compressed;

				++ container->files.count;
			}
		}

		// relocate offsets

		for (i = 0, count = containers.count; i < count; ++i)
		{
			fa_container_t* container = &(containers.data[i]);

			container->parent = relocateOffset(container->parent, sizeof(fa_header_t));
			container->children = relocateOffset(container->children, sizeof(fa_header_t));
			container->next = relocateOffset(container->next, sizeof(fa_header_t));

			container->name = relocateOffset(container->name, sizeof(fa_header_t) + containers.count * sizeof(fa_container_t) + entries.count * (sizeof(fa_entry_t) + sizeof(fa_hash_t)));
			container->files.offset = relocateOffset(container->files.offset, sizeof(fa_header_t) + containers.count * sizeof(fa_container_t));			
		}

		for (i = 0, count = entries.count; i < count; ++i)
		{
			fa_entry_t* entry = &(entries.data[i]);

			entry->name = relocateOffset(entry->name, sizeof(fa_header_t) + containers.count * sizeof(fa_container_t) + entries.count * (sizeof(fa_entry_t) + sizeof(fa_hash_t)));
		}

		// create header

		header.cookie = FA_MAGIC_COOKIE;
		header.version = FA_VERSION_CURRENT;
		header.size = sizeof(fa_header_t) + containers.count + sizeof(fa_container_t) + entries.count * (sizeof(fa_entry_t) + sizeof(fa_hash_t)) + strings.count;
		header.flags = 0;

		header.containers.offset = sizeof(fa_header_t);
		header.containers.count = containers.count;

		header.files.offset = sizeof(fa_header_t) + containers.count * sizeof(fa_container_t);
		header.files.count = entries.count;

		header.hashes = sizeof(fa_header_t) + containers.count * sizeof(fa_container_t) + entries.count * sizeof(fa_entry_t);

		// write toc to archive

		blocks[0].data = &header;
		blocks[0].size = sizeof(header);

		blocks[1].data = containers.data;
		blocks[1].size = containers.count * sizeof(fa_container_t);

		blocks[2].data = entries.data;
		blocks[2].size = entries.count * sizeof(fa_entry_t);

		blocks[3].data = entries.hashes;
		blocks[3].size = entries.count * sizeof(fa_hash_t);

		blocks[4].data = strings.data;
		blocks[4].size = strings.count;

		result = 0;
		for (;;)
		{
			size_t blockSize = 0, compressedSize;
			uint8_t* blockData = writer->archive.cache.data;
			uint8_t* compressedBlock = blockData + FA_COMPRESSION_MAX_BLOCK;
			fa_block_t block;

			for (i = 0, count = 5; (i < count) && (blockSize < FA_COMPRESSION_MAX_BLOCK); ++i)
			{
				size_t maxWrite = blocks[i].size > (FA_COMPRESSION_MAX_BLOCK - blockSize) ? (FA_COMPRESSION_MAX_BLOCK - blockSize) : blocks[i].size;

				memcpy(blockData + blockSize, blocks[i].data, maxWrite);
				blocks[i].data = ((uint8_t*)blocks[i].data) + maxWrite;
				blocks[i].size -= maxWrite;

				blockSize += maxWrite;
			}

			if (blockSize == 0)
			{
				break;
			}

			if (compression == FA_COMPRESSION_NONE)
			{
				fprintf(stderr, "toc (%lu) uncompressed\n", blockSize);

				if (fwrite(blockData, 1, blockSize, (FILE*)writer->archive.fd) != blockSize)
				{
					result = -1;
					break;
				}

				written.original += blockSize;
				written.compressed += blockSize;
				continue;
			}

			compressedSize = fa_compress_block(compression, compressedBlock, FA_COMPRESSION_MAX_BLOCK * 3, blockData, blockSize);
			if (compressedSize >= blockSize)
			{
				block.original = blockSize;
				block.compressed = FA_COMPRESSION_SIZE_IGNORE | blockSize;
				compressedBlock = blockData;
			}
			else
			{
				block.original = blockSize;
				block.compressed = compressedSize;
			}

			fprintf(stderr, "toc (%lu %lu) compressed\n", blockSize, compressedSize);

			if (fwrite(compressedBlock, 1, block.compressed & ~FA_COMPRESSION_SIZE_IGNORE, (FILE*)writer->archive.fd) != (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE))
			{
				result = -1;
				break;
			}

			written.original += block.original;
			written.compressed += block.compressed & ~FA_COMPRESSION_SIZE_IGNORE;
		}

		footer.cookie = FA_MAGIC_COOKIE;
		footer.compression = compression;	

		footer.size.original = written.original;
		footer.size.compressed = written.compressed;

		footer.toc = written.compressed;
		footer.data = writer->offset + written.compressed;

		if (fwrite(&footer, 1, sizeof(footer), (FILE*)writer->archive.fd) != sizeof(footer))
		{
			result = -1;
			break;
		}	
	}
	while (0);

	fprintf(stderr, "%lu string bytes\n", strings.count);
	fprintf(stderr, "%lu containers\n", containers.count);
	fprintf(stderr, "%lu files\n", entries.count);
	fprintf(stderr, "result: %d\n", result);

	free(strings.data);
	free(containers.data);
	free(entries.data);

	return result;
}

static fa_offset_t findContainer(const char* path, const fa_container_t* containers, const char* strings)
{
	const char* curr = path;
	const char* term;
	fa_offset_t offset = 0;

	while ((term = strchr(curr, '/')) != NULL)
	{
		size_t nlen = (term - curr);
		const fa_container_t* parent;
		fa_offset_t child;

		parent = (const fa_container_t*)(((const uint8_t*)containers) + offset);

		for (child = parent->children; child != FA_INVALID_OFFSET;)
		{
			const fa_container_t* childContainer = (const fa_container_t*)(((const uint8_t*)containers) + child);
			const char* name = childContainer->name != FA_INVALID_OFFSET ? strings + childContainer->name : NULL;

			if (name && (strlen(name) == nlen) && !memcmp(name, curr, nlen))
			{
				break;
			}

			child = childContainer->next;
		}

		if (child == FA_INVALID_OFFSET)
		{
			return FA_INVALID_OFFSET;
		}

		offset = child;
		curr = term + 1;
	}
	
	return offset;
}
