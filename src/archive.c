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

static fa_archive_t* openArchiveReading(const char* filename, fa_archiveinfo_t* info);
static fa_archive_t* openArchiveWriting(const char* filename, uint32_t alignment);

static int writeToc(fa_archive_writer_t* archive, fa_compression_t compression, fa_archiveinfo_t* info);

static fa_offset_t findContainer(const char* path, const fa_container_t* containers, const char* strings);

fa_archive_t* fa_open_archive(const char* filename, fa_mode_t mode, uint32_t alignment, fa_archiveinfo_t* info)
{
	fa_archive_t* archive = NULL;

	switch (mode)
	{
		case FA_MODE_READ:
		{
			archive = openArchiveReading(filename, info);
		}
		break;

		case FA_MODE_WRITE:
		{
			archive = openArchiveWriting(filename, alignment);
		}
		break;
	}

	return archive;
}

int fa_close_archive(fa_archive_t* archive, fa_compression_t compression, fa_archiveinfo_t* info)
{
	int result = 0;

	if (archive == NULL)
	{
		return -1;
	}

	if (archive->mode == FA_MODE_WRITE)
	{
		fa_archive_writer_t* writer = (fa_archive_writer_t*)archive;
		if (writeToc(writer, compression, info))
		{
			result = -1;
		}

		free(writer->entries.data);
	}

	archive->ops->close(archive->handle);

	free(archive->toc);
	free(archive);

	return result;
}

static fa_archive_t* openArchiveReading(const char* filename, fa_archiveinfo_t* info)
{
	fa_archive_t* archive = malloc(sizeof(fa_archive_t) + FA_ARCHIVE_CACHE_SIZE);
	memset(archive, 0, sizeof(fa_archive_t));

	archive->ops = fa_get_default_ops();

	archive->mode = FA_MODE_READ;
	archive->cache.data = (uint8_t*)(archive + 1);

	do
	{
		size_t maxRead;
		long fileSize;
		unsigned int i, j;
		fa_footer_t footer;
		SHA1Context state;
		fa_hash_t hash;

		archive->handle = archive->ops->open(filename, FA_MODE_READ);
		if (archive->handle == FA_IO_INVALID_HANDLE)
		{
			break;
		}

		if (archive->ops->lseek(archive->handle, 0, FA_SEEK_END) < 0)
		{
			break;
		}	

		fileSize = archive->ops->tell(archive->handle);
		if (fileSize == -1)
		{
			break;
		}

		maxRead = fileSize > FA_ARCHIVE_CACHE_SIZE ? FA_ARCHIVE_CACHE_SIZE : fileSize;
		if ((maxRead < sizeof(footer)) || (archive->ops->lseek(archive->handle, -maxRead, FA_SEEK_CURR) < 0))
		{
			break;
		}

		if (archive->ops->read(archive->handle, archive->cache.data, maxRead) != maxRead)
		{
			break;
		}

		memset(&footer, 0, sizeof(footer));
		for (i = maxRead - sizeof(footer); i > 0; --i)
		{
			uint32_t cookie;
			memcpy(&cookie, archive->cache.data + i, sizeof(cookie));

			if (cookie == FA_MAGIC_COOKIE_FOOTER)
			{
				memcpy(&footer, archive->cache.data + i, sizeof(footer));
				break;
			}
		}

		if (footer.cookie != FA_MAGIC_COOKIE_FOOTER)
		{
			break;
		}

		archive->base = fileSize - (maxRead - i) - (footer.toc.compressed + footer.data.compressed);

		if (archive->ops->lseek(archive->handle, fileSize - (maxRead - i) - footer.toc.compressed, FA_SEEK_SET) < 0)
		{
			break;
		}

		archive->toc = malloc(footer.toc.original);

		if (footer.toc.compression == FA_COMPRESSION_NONE)
		{
			if (archive->ops->read(archive->handle, archive->toc, footer.toc.original) != footer.toc.original)
			{
				break;
			}
		}
		else
		{
			uint32_t length = footer.toc.compressed;
			uint32_t written = 0;
			uint32_t cacheSize = 0;

			while ((length > 0) && (written < footer.toc.original))
			{
				uint32_t maxRead = length > (FA_ARCHIVE_CACHE_SIZE - cacheSize) ? (FA_ARCHIVE_CACHE_SIZE - cacheSize) : length;
				uint32_t cacheOffset = 0;

				if (archive->ops->read(archive->handle, archive->cache.data + cacheSize, maxRead) != maxRead)
				{
					break;
				}

				cacheSize += maxRead;

				while (cacheOffset < cacheSize)
				{
					uint32_t left = cacheSize - cacheOffset;
					fa_block_t block;

					if (left < sizeof(block))
					{
						break;
					}

					memcpy(&block, archive->cache.data + cacheOffset, sizeof(block));

					if ((block.compressed & ~FA_COMPRESSION_SIZE_IGNORE) + sizeof(block) > left)
					{
						break;
					}

					if (block.original + written > footer.toc.original)
					{
						length = 0;
						break;
					}

					if (block.compressed & FA_COMPRESSION_SIZE_IGNORE)
					{
						memcpy(((uint8_t*)archive->toc) + written, archive->cache.data + cacheOffset + sizeof(block), block.original);
					}
					else
					{
						if (fa_decompress_block(footer.toc.compression, ((uint8_t*)archive->toc) + written, block.original, archive->cache.data + cacheOffset + sizeof(block), block.compressed) != block.original)
						{
							length = 0;
							break;
						}
					}

					cacheOffset += sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE);
					written += block.original;
				}

				if ((cacheSize - cacheOffset) > 0)
				{
					memcpy(archive->cache.data, archive->cache.data + cacheOffset, (cacheSize - cacheOffset));
					cacheSize -= cacheOffset;
				}

				length -= maxRead;
			}

			if ((length != 0) || (written != footer.toc.original))
			{
				break;
			} 
		}

		if (archive->toc->cookie != FA_MAGIC_COOKIE_HEADER)
		{
			break;
		}

		SHA1Reset(&state);
		SHA1Input(&state, (const uint8_t*)archive->toc, footer.toc.original);
		SHA1Result(&state);

		for (i = 0; i < 5; ++i)
		{
			for (j = 0; j < 4; ++j)
			{
				uint8_t v = (uint8_t)((state.Message_Digest[i] >> ((3-j) * 8)) & 0xff);
				hash.data[i * 4 + j] = v;
			}
		}	

		if (memcmp(&hash, &footer.toc.hash, sizeof(fa_hash_t)))
		{
			break;
		}

		if (info)
		{
			info->header = *archive->toc;
			info->footer = footer;
		}

		return archive;
	}
	while (0);

	if (archive->handle != FA_IO_INVALID_HANDLE)
	{
		archive->ops->close(archive->handle);
	}

	free(archive->toc);
	free(archive);

	return NULL;
}

static fa_archive_t* openArchiveWriting(const char* filename, uint32_t alignment)
{
	fa_archive_writer_t* writer = malloc(sizeof(fa_archive_writer_t) + FA_ARCHIVE_CACHE_SIZE);
	memset(writer, 0, sizeof(fa_archive_writer_t));

	writer->archive.ops = fa_get_default_ops();

	writer->archive.mode = FA_MODE_WRITE;
	writer->archive.cache.data = (uint8_t*)(writer + 1);

	do
	{
		writer->archive.handle = writer->archive.ops->open(filename, FA_MODE_WRITE);
		if (writer->archive.handle == FA_IO_INVALID_HANDLE)
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

static int writeToc(fa_archive_writer_t* writer, fa_compression_t compression, fa_archiveinfo_t* info)
{
	struct
	{
		size_t count;
		size_t capacity;
		char* data;
	} strings = { 0, 32768, malloc(32768) };

	struct
	{
		size_t count;
		size_t capacity;
		fa_container_t* data;
	} containers = { 0, 256, malloc(256 * sizeof(fa_container_t)) };

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
		int i, j, count;
		fa_archiveinfo_t local;
		SHA1Context state;

		struct
		{
			void* data;
			size_t size;
		} blocks[5]; // header, containers, entries, hashes, strings

		memset(&local, 0, sizeof(local));

		// construct containers

		containers.data->parent = FA_INVALID_OFFSET;
		containers.data->children = FA_INVALID_OFFSET;
		containers.data->next = FA_INVALID_OFFSET;

		containers.data->name = FA_INVALID_OFFSET;
		containers.data->entries.offset = FA_INVALID_OFFSET;
		containers.data->entries.count = 0;
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

				actual = findContainer(path, containers.data, strings.data);

				if (actual == FA_INVALID_OFFSET)
				{
					fa_container_t* container;
					size_t nlen = strlen(curr);
					char term = *(offset);
					*offset = '\0';

					if (containers.count == containers.capacity)
					{
						containers.capacity *= 2;
						containers.data = realloc(containers.data, containers.capacity * sizeof(fa_container_t));
					}

					parentContainer = (fa_container_t*)(((uint8_t*)containers.data) + parent);
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

					container->entries.offset = FA_INVALID_OFFSET;
					container->entries.count = 0;

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
			fa_offset_t offset;

			if (strlen(entry->path) == 0)
			{
				continue;
			}

			offset = findContainer(entry->path, containers.data, strings.data);
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

		for (i = 0, count = containers.count; i <= count; ++i)
		{
			fa_container_t* container = i < count ? &(containers.data[i]) : NULL;
			fa_offset_t containerOffset = i < count ? (container - containers.data) * sizeof(fa_container_t) : FA_INVALID_OFFSET;
			size_t j;

			for (j = 0; j < writer->entries.count; ++j)
			{
				const fa_writer_entry_t* writerEntry = &(writer->entries.data[j]);
				const char* name;
				size_t nlen, k, l;
				fa_entry_t* entry;
				fa_hash_t* hash;

				if (writerEntry->container != containerOffset)
				{
					continue;
				}

				name = strrchr(writerEntry->path, '/') != NULL ? strrchr(writerEntry->path, '/') + 1 : writerEntry->path;
				nlen = strlen(name) + 1;

				if ((container != NULL) && (container->entries.offset == FA_INVALID_OFFSET))
				{
					container->entries.offset = entries.count * sizeof(fa_entry_t);
				}

				if (entries.count == entries.capacity)
				{
					entries.capacity *= 2;
					entries.data = realloc(entries.data, entries.capacity * sizeof(fa_entry_t));
					entries.hashes = realloc(entries.hashes, entries.capacity * sizeof(fa_hash_t));
				}

				entry = &(entries.data[entries.count]);
				hash = &(entries.hashes[entries.count]);
				++ entries.count;

				entry->data = writerEntry->offset;
				if (strlen(writerEntry->path) > 0)
				{
					entry->name = strings.count;

					if (strings.count + nlen > strings.capacity)
					{
						strings.capacity *= 2;
						strings.data = realloc(strings.data, strings.capacity);
					}

					memcpy(strings.data + strings.count, name, nlen);
					strings.count += nlen;
				}
				else
				{
					entry->name = FA_INVALID_OFFSET;
				}

				entry->compression = writerEntry->compression;
				entry->blockSize = FA_COMPRESSION_MAX_BLOCK;

				entry->size.original = writerEntry->size.original;
				entry->size.compressed = writerEntry->size.compressed;

				for (k = 0; k < 5; ++k)
				{
					for (l = 0; l < 4; ++l)
					{
						uint8_t v = (uint8_t)((writerEntry->hash.Message_Digest[k] >> ((3-l) * 8)) & 0xff);
						hash->data[k * 4 + l] = v;
					}
				}

				if (container != NULL)
				{
					++ container->entries.count;
				}
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
			container->entries.offset = relocateOffset(container->entries.offset, sizeof(fa_header_t) + containers.count * sizeof(fa_container_t));			
		}

		for (i = 0, count = entries.count; i < count; ++i)
		{
			fa_entry_t* entry = &(entries.data[i]);

			entry->name = relocateOffset(entry->name, sizeof(fa_header_t) + containers.count * sizeof(fa_container_t) + entries.count * (sizeof(fa_entry_t) + sizeof(fa_hash_t)));
		}

		// create header

		local.header.cookie = FA_MAGIC_COOKIE_HEADER;
		local.header.version = FA_VERSION_CURRENT;
		local.header.size = sizeof(fa_header_t) + containers.count + sizeof(fa_container_t) + entries.count * (sizeof(fa_entry_t) + sizeof(fa_hash_t)) + strings.count;
		local.header.flags = 0;

		local.header.containers.offset = sizeof(fa_header_t);
		local.header.containers.count = containers.count;

		local.header.entries.offset = sizeof(fa_header_t) + containers.count * sizeof(fa_container_t);
		local.header.entries.count = entries.count;

		local.header.hashes = sizeof(fa_header_t) + containers.count * sizeof(fa_container_t) + entries.count * sizeof(fa_entry_t);

		// write toc to archive

		blocks[0].data = &local.header;
		blocks[0].size = sizeof(local.header);

		blocks[1].data = containers.data;
		blocks[1].size = containers.count * sizeof(fa_container_t);

		blocks[2].data = entries.data;
		blocks[2].size = entries.count * sizeof(fa_entry_t);

		blocks[3].data = entries.hashes;
		blocks[3].size = entries.count * sizeof(fa_hash_t);

		blocks[4].data = strings.data;
		blocks[4].size = strings.count;

		SHA1Reset(&state);

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

			SHA1Input(&state, blockData, blockSize);

			if (compression == FA_COMPRESSION_NONE)
			{
				if (writer->archive.ops->write(writer->archive.handle, blockData, blockSize) != blockSize)
				{
					result = -1;
					break;
				}

				local.footer.toc.original += blockSize;
				local.footer.toc.compressed += blockSize;
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

			if (writer->archive.ops->write(writer->archive.handle, &block, sizeof(block)) != sizeof(block))
			{
				result = -1;
				break;
			}

			if (writer->archive.ops->write(writer->archive.handle, compressedBlock, block.compressed & ~FA_COMPRESSION_SIZE_IGNORE) != (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE))
			{
				result = -1;
				break;
			}

			local.footer.toc.original += block.original;
			local.footer.toc.compressed += sizeof(block) + (block.compressed & ~FA_COMPRESSION_SIZE_IGNORE);
		}

		SHA1Result(&state);

		if (result < 0)
		{
			break;
		}

		local.footer.cookie = FA_MAGIC_COOKIE_FOOTER;
		local.footer.toc.compression = compression;	

		for (i = 0; i < 5; ++i)
		{
			for (j = 0; j < 4; ++j)
			{
				uint8_t v = (uint8_t)((state.Message_Digest[i] >> ((3-j) * 8)) & 0xff);
				local.footer.toc.hash.data[i * 4 + j] = v;
			}
		}

		local.footer.data.original = writer->offset.original;
		local.footer.data.compressed = writer->offset.compressed;

		if (writer->archive.ops->write(writer->archive.handle, &local.footer, sizeof(local.footer)) != sizeof(local.footer))
		{
			result = -1;
			break;
		}

		if (info != NULL)
		{
			*info = local;
		}	
	}
	while (0);

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
