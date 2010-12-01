#ifndef STREAMER_CONTRIB_FILEARCHIVE_H
#define STREAMER_CONTRIB_FILEARCHIVE_H

#if defined(_IOP)
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
#else
#include <stdint.h>
#endif

typedef struct fa_container_t fa_container_t;
typedef struct fa_entry_t fa_entry_t;
typedef struct fa_block_t fa_block_t;
typedef struct fa_header_t fa_header_t;
typedef struct fa_footer_t fa_footer_t;
typedef struct fa_hash_t fa_hash_t;

typedef enum
{
	FA_COMPRESSION_NONE = (0),
	FA_COMPRESSION_FASTLZ = (('F' << 24) | ('L' << 16) | ('Z' << 8) | ('0'))
} fa_compression_t;

struct fa_container_t
{
	uint32_t parent;	// Offset to parent container (FileArchiveContainer)
	uint32_t children;	// Offset to child containers (FileArchiveContainer)
	uint32_t next;		// Offset to next sibling container (FileArchiveContainer)

	uint32_t files;		// Offset to first file in container (FileArchiveEntry)
	uint32_t count;		// number of entries

	uint32_t name;		// Offset to container name
};

struct fa_entry_t
{
	uint32_t data;			// Offset to file data
	uint32_t name;			// Offset to name

	uint32_t compression;		// Compression method used in file

	struct
	{
		uint32_t original;	// original file size when uncompressed
		uint32_t compressed;	// compressed file size in archive
	} size;

	uint16_t blockSize;		// block size required for decompression
};

struct fa_block_t
{
	uint16_t original;
	uint16_t compressed; 		// FILEARCHIVE_COMPRESSION_SIZE_IGNORE == block uncompressed
};

struct fa_header_t
{
	uint32_t cookie;		// Magic cookie
	uint32_t version;		// Version of archive
	uint32_t size;			// Size of TOC
	uint32_t flags;			// Flags

	// Version 1

	uint32_t containers;		// Offset to containers (first container == root folder)
	uint32_t containerCount;	// Number of containers in archive

	uint32_t files;			// Offset to list of files
	uint32_t fileCount;		// Number of files in archive
	uint32_t hashes;		// Offset to file hashes
};

struct fa_footer_t
{
	uint32_t cookie;		// Magic cookie
	uint32_t toc;			// Offset to TOC (relative to beginning of footer)
	uint32_t data;			// Offset to data (relative to beginning of footer)

	uint32_t compression;		// Compression used on header

	struct
	{
		uint32_t original;	// TOC size, uncompressed
		uint32_t compressed;	// TOC size, compressed
	} size;
};

struct fa_hash_t
{
	uint8_t data[20];
};

#define FA_VERSION_1 (1)
#define FA_VERSION_CURRENT (FA_VERSION_1)

#define FA_MAGIC_COOKIE (('F' << 24) | ('A' << 16) | ('R' << 8) | ('C'))

#define FA_COMPRESSION_SIZE_IGNORE (0x8000)
#define FA_COMPRESSION_SIZE_MASK (0x7fff)

#define FA_INVALID_OFFSET (0xffffffff)

#endif

