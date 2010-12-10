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

			SHA1Reset(&(file->hash));

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

	if (file->archive.cache.owner == file)
	{
		file->archive->cache.offset = 0;
		file->archive->cache.fill = 0;
		file->archive->cache.owner = NULL;
	}

	switch (file->archive.mode)
	{
		case FA_MODE_READ:
		{
		}
		break;

		case FA_MODE_WRITE:
		{
			fa_file_writer_t* writer = (fa_file_writer_t*)file;

			SHA1Result(&(writer->hash));

			if (dirinfo != NULL)
			{
				fa_entry_t* entry = writer->entry;

				dirinfo->name = entry->name;
				dirinfo->type = FA_ENTRY_FILE;
				dirinfo->compression = entry->compression;

				dirinfo->size.compressed = entry->size.compressed;
				dirinfo->size.original = entry->size.original;

				// TODO: fix hash
			}

			// TODO: flush buffer

			free(writer->file.buffer);
			free(writer);

			return 0;
		}
		break;
	}

	return -1;
}

size_t fa_write(fa_file_t* file, const void* buffer, size_t length)
{
	size_t written = 0;

	do
	{
		fa_file_writer_t* writer;
		size_t maxWrite;

		if ((file == NULL) || (file->archive.mode != FA_MODE_WRITE))
		{
			break;
		}

		writer = (fa_file_writer_t*)file;

		SHA1Input(&(writer->hash), buffer, length);

		if (writer->entry.compression == FA_COMPRESSION_NONE)
		{
			size_t result = fwrite(buffer, 1, length, (FILE*)file->archive.fd);

			writer->entry.size.original += result;
			writer->entry.size.compressed += result;

			written += result;
			break;
		}

		while (length > 0)
		{
			size_t bufferMax = FA_COMPRESSION_MAX_BLOCK - writer->buffer.fill;
			size_t maxWrite = length > bufferMax ? bufferMax : length;

			memcpy(writer->buffer.data + writer->buffer.fill, 

			buffer = ((uint8_t*)buffer) + maxWrite;
			writer->buffer.fill += maxWrite;
			length -= maxWrite;

			if (writer->buffer.fill == FA_COMPRESSION_MAX_BLOCK)
			{
				size_t compressedSize = fa_compress_block(writer->entry.compression);
				fa_block_t block;
				uint8_t* data;

				if (compressedSize >= FA_COMPRESSION_MAX_BLOCK)
				{
					block.original = FA_COMPRESSION_MAX_BLOCK;
					block.compressed = FA_COMPRESSION_SIZE_IGNORE | FA_COMPRESSION_MAX_BLOCK;
				}
				else
				{
					block.original = FA_COMPRESSION_MAX_BLOCK;
					block.compressed = compressedSize;
				}

				fwrite(...);
			}
		}

		
	}
	while (0);

	return written;
}

static fa_container_t* fa_find_container(const fa_archive_t* archive, const char* path)
{
}

