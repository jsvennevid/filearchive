#include "../internal/api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
			const fa_container_t* container = fa_find_container(archive, NULL, filename);
			const fa_entry_t* begin;
			const fa_entry_t* end;
			const char* local = strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename;
			fa_file_t* file;

			if (container == NULL)
			{
				return NULL;
			}

			for (begin = (fa_entry_t*)(((uint8_t*)archive->toc) + container->files.offset), end = begin + container->files.count; begin != end; ++begin)
			{
				const char* name = begin->name != FA_INVALID_OFFSET ? ((const char*)archive->toc) + begin->name : NULL;
				if ((name != NULL) && !strcmp(name, local))
				{
					break;
				}
 
			}

			if (begin == end)
			{
				return NULL;
			}

			file = malloc(sizeof(fa_file_t) + sizeof(FA_COMPRESSION_MAX_BLOCK));
			memset(file, 0, sizeof(fa_file_t));

			file->archive = archive;
			file->entry = begin;

			file->buffer.data = (uint8_t*)(file + 1);

			return file;
		}
		break;

		case FA_MODE_WRITE:
		{
			fa_archive_writer_t* writer = (fa_archive_writer_t*)archive;
			fa_writer_entry_t* entry;
			fa_file_writer_t* file;
			char* begin;
			char* end;

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
			entry->offset = writer->offset.compressed;
			entry->compression = compression;

			for (begin = entry->path, end = begin + strlen(begin); begin != end; ++begin)
			{
				if (*begin == '\\')
					*begin = '/';
			}

			file->file.archive = archive;
			file->file.buffer.data = malloc(FA_COMPRESSION_MAX_BLOCK);
			file->entry = entry;

			SHA1Reset(&(entry->hash));

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
			free(file);
			return 0;
		}
		break;

		case FA_MODE_WRITE:
		{
			fa_file_writer_t* writer = (fa_file_writer_t*)file;
			fa_archive_writer_t* awriter = (fa_archive_writer_t*)file->archive;
			fa_writer_entry_t* entry = writer->entry;
			int result = 0;

			SHA1Result(&(entry->hash));

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

				awriter->offset.original += block.original;
				awriter->offset.compressed += sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE);

				entry->size.original += block.original;
				entry->size.compressed += sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE); 
			}
			while (0);

			if (dirinfo != NULL)
			{
				int i,j;

				dirinfo->name = strrchr(entry->path, '/') ? strrchr(entry->path, '/') + 1 : entry->path;
				dirinfo->type = FA_ENTRY_FILE;
				dirinfo->compression = entry->compression;

				dirinfo->size.compressed = entry->size.compressed;
				dirinfo->size.original = entry->size.original;

				for (i = 0; i < 5; ++i)
				{
					for (j = 0; j < 4; ++j)
					{
						uint8_t v = (uint8_t)((entry->hash.Message_Digest[i] >> ((3-j) * 8)) & 0xff);
						dirinfo->hash.data[i * 4 + j] = v;
					}
				}
			}

			free(writer->file.buffer.data);
			free(writer);

			return result;
		}
		break;
	}

	return -1;
}

size_t fa_read_file(fa_file_t* file, void* buffer, size_t length)
{
	size_t totalRead = 0;

	do
	{
		uint32_t maxPreRead, maxFileRead, maxBufferRead, maxRawRead, maxRead;

		if ((file == NULL) || (file->archive->mode != FA_MODE_READ))
		{
			break;
		}

		maxPreRead = file->buffer.fill - file->buffer.offset;
		if (maxPreRead > 0)
		{
			maxRead = length > maxPreRead ? maxPreRead : length;
			memcpy(buffer, file->buffer.data + file->buffer.offset, maxRead);

			file->buffer.offset += maxRead;
			buffer = ((uint8_t*)buffer) + maxRead;
			length -= maxRead;
			totalRead += maxRead;
		}

		if (length == 0)
		{
			break;
		}

		maxFileRead = file->entry->size.original - file->offset.original;
		maxBufferRead = maxFileRead & ~(FA_COMPRESSION_MAX_BLOCK-1);
		maxRawRead = length & ~(FA_COMPRESSION_MAX_BLOCK-1);
		maxRead = maxRawRead > maxBufferRead ? maxBufferRead : maxRawRead;

		if (fseek(file->archive->fd, file->archive->base + file->offset.compressed, SEEK_SET) < 0)
		{
			break;
		}

		if (file->entry->compression == FA_COMPRESSION_NONE)
		{
			if (fread(buffer, 1, maxRead, file->archive->fd) != maxRead)
			{
				break;
			}

			buffer = ((uint8_t*)buffer) + maxRead;
			length -= maxRead;
			totalRead += maxRead;

			file->offset.original += maxRead;
			file->offset.compressed += maxRead;

			maxFileRead -= maxRead;
			maxFileRead = maxFileRead > FA_COMPRESSION_MAX_BLOCK ? FA_COMPRESSION_MAX_BLOCK : maxFileRead;

			maxRead = length > maxFileRead ? maxFileRead : length;

			if (fread(file->buffer.data, 1, maxFileRead, file->archive->fd) != maxFileRead)
			{
				break;
			}

			memcpy(buffer, file->buffer.data, maxRead);
			totalRead += maxRead;

			file->buffer.offset = maxRead;
			file->buffer.fill = maxFileRead;

			file->offset.original += maxFileRead;
			file->offset.compressed += maxFileRead;
		}
		else
		{
			fprintf(stderr, "TODO: implement compressed reads\n");
		}
	}
	while (0);

	return totalRead;
}

size_t fa_write_file(fa_file_t* file, const void* buffer, size_t length)
{
	size_t written = 0;

	do
	{
		fa_file_writer_t* writer;
		fa_archive_writer_t* awriter;

		if ((file == NULL) || (file->archive->mode != FA_MODE_WRITE))
		{
			break;
		}

		writer = (fa_file_writer_t*)file;
		awriter = (fa_archive_writer_t*)file->archive;

		SHA1Input(&(writer->entry->hash), buffer, length);

		if (writer->entry->compression == FA_COMPRESSION_NONE)
		{
			size_t result = fwrite(buffer, 1, length, (FILE*)file->archive->fd);

			writer->entry->size.original += result;
			writer->entry->size.compressed += result;

			awriter->offset.original += result;
			awriter->offset.compressed += result;
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

				awriter->offset.original += block.original;
				awriter->offset.compressed += sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE);

				writer->file.buffer.fill = 0;

				writer->entry->size.original += block.original;
				writer->entry->size.compressed += sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE); 
			}

			length -= maxWrite;
			written += maxWrite;
		}
	}
	while (0);

	return written;
}

int fa_seek(fa_file_t* file, int64_t offset, fa_seek_t whence)
{
	if ((file == NULL) || (file->archive->mode != FA_MODE_READ))
	{
		return -1;
	}

	uint32_t fixedOffset;
	switch (whence)
	{
		case FA_SEEK_SET:
		{
			fixedOffset = offset;
		}
		break;

		case FA_SEEK_CURR:
		{
			fixedOffset = file->offset.original + offset;
		}
		break;

		case FA_SEEK_END:
		{
			fixedOffset = file->entry->size.original + offset;
		}
		break;
	}

	if (file->entry->size.original < fixedOffset)
	{
		return -1;
	}

	if (file->entry->compression == FA_COMPRESSION_NONE)
	{
		uint32_t alignedOffset = fixedOffset & ~(FA_COMPRESSION_MAX_BLOCK-1);
		uint32_t maxFileRead = file->entry->size.original - alignedOffset;
		maxFileRead = maxFileRead > FA_COMPRESSION_MAX_BLOCK ? FA_COMPRESSION_MAX_BLOCK : maxFileRead;

		if (alignedOffset != fixedOffset)
		{
			if (fseek(file->archive->fd, file->archive->base + alignedOffset, SEEK_SET) < 0)
			{
				return -1;
			}

			if (fread(file->buffer.data, 1, maxFileRead, file->archive->fd) != maxFileRead)
			{
				return -1;
			}
		}
		else
		{
			maxFileRead = 0;
		}

		file->buffer.offset = fixedOffset - alignedOffset;
		file->buffer.fill = maxFileRead;

		file->offset.compressed = file->offset.original = alignedOffset + maxFileRead;
	}
	else
	{
		fprintf(stderr, "Seeking inside compressed files currently unsupported\n");
	}

	return -1;
}

size_t fa_tell(fa_file_t* file)
{
	if ((file == NULL) || (file->archive->mode != FA_MODE_READ))
	{
		return 0;
	}

	return file->offset.original - file->buffer.fill + file->buffer.offset;	
}
