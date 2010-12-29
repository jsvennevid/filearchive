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

#include <stdlib.h>
#include <string.h>

fa_dir_t* fa_open_dir(fa_archive_t* archive, const char* path)
{
	fa_dir_t* dir = NULL;

	do
	{
		const fa_container_t* container;

		if ((archive == NULL) || (archive->mode != FA_MODE_READ))
		{
			break;
		}

		container = fa_find_container(archive, NULL, path);
		if (container == NULL)
		{
			break;
		}

		dir = malloc(sizeof(fa_dir_t));
		memset(dir, 0, sizeof(fa_dir_t));

		dir->archive = archive;
		dir->parent = container;

		dir->container = container->children != FA_INVALID_OFFSET ? (fa_container_t*)(((uint8_t*)archive->toc) + container->children) : NULL;
	}
	while (0);

	return dir;
}

int fa_read_dir(fa_dir_t* dir, fa_dirinfo_t* info)
{
	if ((dir == NULL) || (info == NULL))
	{
		return -1;
	}

	if (dir->container != NULL)
	{
		memset(info, 0, sizeof(fa_dirinfo_t));

		fa_container_t* next = dir->container->next != FA_INVALID_OFFSET ? (fa_container_t*)(((uint8_t*)dir->archive->toc) + dir->container->next) : NULL;

		info->name = dir->container->name != FA_INVALID_OFFSET ? (const char*)(((uint8_t*)dir->archive->toc) + dir->container->name) : NULL;

		info->type = FA_ENTRY_DIR;
		info->compression = FA_COMPRESSION_NONE;

		dir->container = next;
		return 0;
	}

	if (dir->index < dir->parent->entries.count)
	{
		const fa_entry_t* entries = (const fa_entry_t*)(((uint8_t*)(dir->archive->toc)) + dir->archive->toc->entries.offset);
		const fa_hash_t* hashes = (const fa_hash_t*)(((uint8_t*)(dir->archive->toc)) + dir->archive->toc->hashes);

		const fa_entry_t* entry = ((const fa_entry_t*)(((uint8_t*)dir->archive->toc) + dir->parent->entries.offset)) + dir->index++;
		const fa_hash_t* hash = hashes + (entry - entries); 

		memset(info, 0, sizeof(fa_dirinfo_t));

		info->name = entry->name != FA_INVALID_OFFSET ? (const char*)(((uint8_t*)dir->archive->toc) + entry->name) : NULL;

		info->type = FA_ENTRY_FILE;
		info->compression = entry->compression;

		info->size.original = entry->size.original;
		info->size.compressed = entry->size.compressed;

		info->hash = *hash;

		return 0;
	}

	return -1;	
} 

int fa_close_dir(fa_dir_t* dir)
{
	if (dir == NULL)
	{
		return -1;
	}

	free(dir);
	return 0;
}


const fa_container_t* fa_find_container(const fa_archive_t* archive, const fa_container_t* container, const char* path)
{
	char* term;
	fa_offset_t curr;
	uint8_t* toc = (uint8_t*)(archive->toc);
	const fa_container_t* child;

	if (container == NULL)
	{
		container = (fa_container_t*)(toc + archive->toc->containers.offset);
	}

	term = strchr(path, '/');
	if (term == NULL)
	{
		return container;
	}

	for (curr = container->children; curr != FA_INVALID_OFFSET;)
	{
		child = (fa_container_t*)(toc + curr);
		const char* name = child->name != FA_INVALID_OFFSET ? (const char*)(toc + child->name) : NULL;
		size_t nlen = name != NULL ? strlen(name) : 0;

		if (((term - path) == nlen) && !memcmp(name, path, nlen))
		{
			break;
		}

		curr = child->next;
	}

	return (curr != FA_INVALID_OFFSET) ? fa_find_container(archive, child, term + 1) : NULL;
}
