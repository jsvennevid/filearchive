#include "../internal/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static fa_container_t* fa_find_container(const fa_archive_t* archive, const char* path);

fa_file_t* fa_open_file(fa_archive_t* archive, const char* filename, fa_compression_t compression, fa_dirinfo_t* dirinfo)
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
			fa_archive_writer_t* writer = (fa_archive_writer_t*)archive;
			fa_writer_entry_t* entry;
			fa_file_writer_t* file;

			if (archive->cache.owner != NULL)
			{
				return NULL;
			}

			file = malloc(sizeof(fa_file_writer_t));
			memset(file, 0, sizeof(fa_file_writer_t));

			if (writer->entries.count == writer->entries.capacity)
			{
				size_t newCapacity = (writer->entries.capacity * 2) < 32 ? 32 : writer->entries.capacity * 2;
				writer->entries.data = realloc(writer->entries.data, newCapacity * sizeof(fa_writer_entry_t));
				writer->entries.capacity = newCapacity;
			}

			// TODO: align on block size

			entry = &(writer->entries.data[writer->entries.count++]);
			entry->path = strdup(filename);
			entry->offset = writer->offset;
			entry->compression = compression;

			file->file.archive = archive;
			file->file.buffer.data = malloc(FA_COMPRESSION_MAX_BLOCK);
			file->entry = entry;

			SHA1Reset(&(file->file.hash));

			archive->cache.owner = &(file->file);
			return &(file->file);
		}
		break;
	}

	return NULL;
}

int fa_close_file(fa_file_t* file, fa_dirinfo_t* dirinfo)
{
	if (file == NULL)
	{
		return -1;
	}

	if (file->archive->cache.owner == file)
	{
		file->archive->cache.offset = 0;
		file->archive->cache.fill = 0;
		file->archive->cache.owner = NULL;
	}

	switch (file->archive->mode)
	{
		case FA_MODE_READ:
		{
		}
		break;

		case FA_MODE_WRITE:
		{
			fa_file_writer_t* writer = (fa_file_writer_t*)file;
			int result = 0;

			SHA1Result(&(writer->file.hash));

			if (dirinfo != NULL)
			{
				fa_writer_entry_t* entry = writer->entry;

				dirinfo->name = strrchr(erntry->path, '/') ? strrchr(entry->path, '/') + 1 : entry->path;
				dirinfo->type = FA_ENTRY_FILE;
				dirinfo->compression = entry->compression;

				dirinfo->size.compressed = entry->size.compressed;
				dirinfo->size.original = entry->size.original;

				// TODO: fix hash
			}

			do
			{
				size_t compressedSize;
				fa_block_t block;
				uint8_t* data;

				if (writer->file.buffer.fill == 0)
				{
					break;
				}

				compressedSize = fa_compress_block(writer->entry->compression, file->archive->cache.data, FA_ARCHIVE_CACHE_SIZE, writer->file.buffer.data, writer->file.buffer.fill);

				if (compressedSize >= writer->file.buffer.fill)
				{
					block.original = writer->file.buffer.fill;
					block.compressed = FA_COMPRESSION_SIZE_IGNORE | writer->file.buffer.fill;
					data = writer->file.buffer.data;
				}
				else
				{
					block.original = writer->file.buffer.fill;
					block.compressed = compressedSize;
					data = file->archive->cache.data;
				}

				if (fwrite(&block, 1, sizeof(block), (FILE*)(file->archive->fd)) != sizeof(block))
				{
					result = -1;
					break;
				}

				if (fwrite(data, 1, block.compressed & ~FA_COMPRESSION_SIZE_IGNORE, (FILE*)file->archive->fd) != (size_t)((block.compressed & ~FA_COMPRESSION_SIZE_IGNORE)))
				{
					result = -1;
					break;
				}
			}
			while (0);

			// TODO: flush buffer

			free(writer->file.buffer.data);
			free(writer);

			return result;
		}
		break;
	}

	return -1;
}

size_t fa_read(fa_file_t* file, void* buffer, size_t length)
{
	return 0;
}

size_t fa_write(fa_file_t* file, const void* buffer, size_t length)
{
	size_t written = 0;

	do
	{
		fa_file_writer_t* writer;

		if ((file == NULL) || (file->archive->mode != FA_MODE_WRITE))
		{
			break;
		}

		writer = (fa_file_writer_t*)file;

		SHA1Input(&(writer->file.hash), buffer, length);

		if (writer->entry->compression == FA_COMPRESSION_NONE)
		{
			size_t result = fwrite(buffer, 1, length, (FILE*)file->archive->fd);

			writer->entry->size.original += result;
			writer->entry->size.compressed += result;

			written += result;
			break;
		}

		while (length > 0)
		{
			size_t bufferMax = FA_COMPRESSION_MAX_BLOCK - writer->file.buffer.fill;
			size_t maxWrite = length > bufferMax ? bufferMax : length;

			memcpy(writer->file.buffer.data + writer->file.buffer.fill, buffer, maxWrite);

			buffer = ((uint8_t*)buffer) + maxWrite;
			writer->file.buffer.fill += maxWrite;

			if (writer->file.buffer.fill == FA_COMPRESSION_MAX_BLOCK)
			{
				size_t compressedSize = fa_compress_block(writer->entry->compression, file->archive->cache.data, FA_ARCHIVE_CACHE_SIZE, writer->file.buffer.data, FA_COMPRESSION_MAX_BLOCK);
				fa_block_t block;
				uint8_t* data;

				if (compressedSize >= FA_COMPRESSION_MAX_BLOCK)
				{
					block.original = FA_COMPRESSION_MAX_BLOCK;
					block.compressed = FA_COMPRESSION_SIZE_IGNORE | FA_COMPRESSION_MAX_BLOCK;
					data = writer->file.buffer.data;
				}
				else
				{
					block.original = FA_COMPRESSION_MAX_BLOCK;
					block.compressed = (uint16_t)compressedSize;
					data = file->archive->cache.data;
				}

				if (fwrite(&block, 1, sizeof(block), (FILE*)(file->archive->fd)) != sizeof(block))
				{
					break;
				}

				if (fwrite(data, 1, block.compressed & ~FA_COMPRESSION_SIZE_IGNORE, (FILE*)file->archive->fd) != (size_t)((block.compressed & ~FA_COMPRESSION_SIZE_IGNORE)))
				{
					break;
				}

				writer->file.buffer.fill = 0;
			}

			length -= maxWrite;
			written += maxWrite;
		}
	}
	while (0);

	return written;
}

static fa_container_t* fa_find_container(const fa_archive_t* archive, const char* path)
{
	return NULL;
}

