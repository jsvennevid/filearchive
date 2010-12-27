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

typedef uint32_t fa_offset_t;

typedef enum
{
	FA_COMPRESSION_NONE = (0),
	FA_COMPRESSION_FASTLZ = (('F' << 24) | ('L' << 16) | ('Z' << 8) | ('0'))
} fa_compression_t;

struct fa_container_t
{
	fa_offset_t parent;		// Offset to parent container (Relative to start of TOC)
	fa_offset_t children;		// Offset to child containers (Relative to start of TOC)
	fa_offset_t next;		// Offset to next sibling container (Relative to start of TOC)

	fa_offset_t name;		// Offset to container name (Relative to start of TOC)

	struct
	{
		fa_offset_t offset;	// Offset to first file in container (Relative to start of TOC)
		uint32_t count;		// Number of files in container
	} files;
};

struct fa_entry_t
{
	fa_offset_t data;		// Offset to file data (Relative to start of data)
	fa_offset_t name;		// Offset to name (Relative to start of TOC)

	uint32_t compression;		// Compression method used in file
	uint32_t blockSize;		// Block size required when decompressing

	struct
	{
		uint32_t original;	// original file size when uncompressed
		uint32_t compressed;	// compressed file size in archive
	} size;
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

	struct
	{
		fa_offset_t offset;	// Offset to containers (relative to start of TOC)
		uint32_t count;		// Number of containers in archive
	} containers;

	struct
	{
		fa_offset_t offset;	// Offset to file entries (relative to start of TOC)
		uint32_t count;		// Number of files in archive
	} files;

	fa_offset_t hashes;		// Offset to file hashes
};

struct fa_footer_t
{
	uint32_t cookie;		// Magic cookie

	struct
	{
		uint32_t compression;	// TOC compression format
		uint32_t original;	// TOC size, uncompressed
		uint32_t compressed;	// TOC size, compressed
	} toc;

	struct
	{
		uint32_t original;	// Data size, uncompressed
		uint32_t compressed;	// Data size, compressed
	} data;
};

struct fa_hash_t
{
	uint8_t data[20];
};

#define FA_VERSION_1 (1)
#define FA_VERSION_CURRENT (FA_VERSION_1)

#define FA_MAGIC_COOKIE_HEADER (('F' << 24) | ('A' << 16) | ('R' << 8) | ('H'))
#define FA_MAGIC_COOKIE_FOOTER (('F' << 24) | ('A' << 16) | ('R' << 8) | ('F'))

#define FA_COMPRESSION_SIZE_IGNORE (0x8000)

#define FA_INVALID_OFFSET (0xffffffff)

#endif

