/*

Copyright (c) 2010 Jesper Svennevid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <filearchive/internal/api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fillCache(fa_file_t* file, size_t minFill);

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
			const fa_container_t* container;
			const fa_entry_t* begin = NULL;
			const fa_entry_t* end = NULL;
			const char* local;
			fa_file_t* file;

			if (*filename == '@')
			{
				do
				{
					const char* in = filename + 1;
					const fa_hash_t* hashes;	
					fa_hash_t hash;
					int i, n;

					if (strlen(in) != sizeof(hash) * 2)
					{
						fprintf(stderr, "size mismatch\n");
						break;
					}

					memset(&hash, 0, sizeof(hash));
					for (i = 0; i < sizeof(hash) * 2; ++i)
					{
						char v = *(in++);
						uint8_t out;
						if ((v >= '0') && (v <= '9'))
						{
							out = v - '0';
						}
						else if ((v >= 'A') && (v <= 'F'))
						{
							out = 10 + v - 'A';
						}
						else if ((v >= 'a') && (v <= 'f'))
						{
							out = 10 + v - 'a';
						}
						else
						{
							break;
						}

						hash.data[i >> 1] |= out << (((i & 1)^1) << 2);
					}

					if (i != sizeof(hash) * 2)
					{
						break;
					}

					hashes = (fa_hash_t*)(((uint8_t*)archive->toc) + archive->toc->hashes);
					for (i = 0, n = archive->toc->entries.count; i < n; ++i)
					{
						if (memcmp(&hashes[i], &hash, sizeof(fa_hash_t)))
						{
							continue;
						}

						begin = ((fa_entry_t*)(((uint8_t*)archive->toc) + archive->toc->entries.offset)) + i;
						break;
					}
				}
				while (0);
			}

			if (begin == NULL)
			{
				container = fa_find_container(archive, NULL, filename);
				if (container == NULL)
				{
					return NULL;
				}

				local = strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename;
				for (begin = (fa_entry_t*)(((uint8_t*)archive->toc) + container->entries.offset), end = begin + container->entries.count; begin != end; ++begin)
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
			}

			file = malloc(sizeof(fa_file_t) + sizeof(FA_COMPRESSION_MAX_BLOCK));
			memset(file, 0, sizeof(fa_file_t));

			file->archive = archive;
			file->entry = begin;

			file->base = archive->base + begin->data;

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
			char* out;
			char* end;
			int last;

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
			entry->container = FA_INVALID_OFFSET;
			entry->offset = writer->offset.compressed;
			entry->compression = compression;

			for (begin = entry->path, out = begin, last = '\0', end = begin + strlen(begin); begin != end; ++begin)
			{
				int c = *begin;

				if (c == '\\')
				{
					c = '/';
				}

				if ((c == '/') && ((last == '/') || (last == '\0')))
				{
					continue;
				}

				*out++ = last = c;
			}
			*out = '\0';

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

		if (file->entry->compression == FA_COMPRESSION_NONE)
		{
			if (fseek(file->archive->fd, file->base + file->offset.compressed, SEEK_SET) < 0)
			{
				break;
			}

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
			if (file->archive->cache.owner != file)
			{
				file->archive->cache.offset = 0;
				file->archive->cache.fill = 0;
				file->archive->cache.owner = file;
			}

			length = length > maxFileRead ? maxFileRead : length;
			while (length > 0)
			{
				if (file->buffer.offset == file->buffer.fill)
				{
					fa_block_t block;

					if (fillCache(file, sizeof(block)) < 0)
					{
						break;
					}

					memcpy(&block, file->archive->cache.data + file->archive->cache.offset, sizeof(block)); 

					if (fillCache(file, sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE)) < 0)
					{
						break;
					}

					if (block.original > FA_COMPRESSION_MAX_BLOCK)
					{
						break;
					}

					if (block.compressed & FA_COMPRESSION_SIZE_IGNORE)
					{
						memcpy(file->buffer.data, file->archive->cache.data + file->archive->cache.offset + sizeof(block), block.original);
					}
					else
					{
						if (fa_decompress_block(file->entry->compression, file->buffer.data, block.original, file->archive->cache.data + file->archive->cache.offset + sizeof(block), block.compressed) != block.original)
						{
							break;
						}
					}

					file->archive->cache.offset += sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE);
					file->offset.compressed += sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE);
					file->offset.original += block.original;

					file->buffer.offset = 0;
					file->buffer.fill = block.original;
				}

				maxRead = length > file->buffer.fill - file->buffer.offset ? file->buffer.fill - file->buffer.offset : length;
				if (maxRead == 0)
				{
					break;
				}

				memcpy(buffer, file->buffer.data + file->buffer.offset, maxRead);

				buffer = ((uint8_t*)buffer) + maxRead;
				length -= maxRead;
				totalRead += maxRead;

				file->buffer.offset += maxRead;
			}
		}
	}
	while (0);

	return totalRead;
}

static int fillCache(fa_file_t* file, size_t minFill)
{
	fa_archive_t* archive = file->archive;
	size_t cacheFill = archive->cache.fill - archive->cache.offset;
	size_t cacheMax, fileMax;
	size_t maxRead;

	if (cacheFill >= minFill)
	{
		return 0;
	}

	cacheMax = FA_ARCHIVE_CACHE_SIZE - cacheFill;
	fileMax = file->entry->size.compressed - file->offset.compressed;
	maxRead = cacheMax > fileMax ? fileMax : cacheMax;

	memmove(archive->cache.data, archive->cache.data + archive->cache.offset, cacheFill);

	archive->cache.offset = 0;
	archive->cache.fill = cacheFill;

	if (fseek(archive->fd, file->base + file->offset.compressed, SEEK_SET) < 0)
	{
		return -1;
	}

	if (fread(archive->cache.data + cacheFill, 1, maxRead, archive->fd) != maxRead)
	{
		return -1;
	}

	archive->cache.fill += maxRead;

	if (archive->cache.fill < minFill)
	{
		return -1;
	}

	return 0;
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
			if (fseek(file->archive->fd, file->base + alignedOffset, SEEK_SET) < 0)
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
		// TODO: implement seeking inside compressed files
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

