/*!

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

#ifndef FILEARCHIVE_FILEARCHIVE_H 
#define FILEARCHIVE_FILEARCHIVE_H

/*! \file filearchive.h
 * File archive format definition
 *
 * This contains all data structures and enumerations needed to access file archives.
 *
 */

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

/*! Compression type */
typedef enum
{
	FA_COMPRESSION_NONE = (0), /*!< No compression */
	FA_COMPRESSION_FASTLZ = (('F' << 24) | ('L' << 16) | ('Z' << 8) | ('0')), /*!< FastLZ compression */
	FA_COMPRESSION_DEFLATE = (('Z' << 24) | ('L' << 16) | ('D' << 8) | ('F')), /*!< Deflate compression (zlib) */
} fa_compression_t;

/*! Version enumeration */
typedef enum
{
	FA_VERSION_1 = 1,

	FA_VERSION_CURRENT = FA_VERSION_1
} fa_version_t;

/*! Cookie pattern */
typedef enum
{
	FA_MAGIC_COOKIE_HEADER = (('F' << 24)|('A' << 16)|('R' << 8)|('H')), /*!< Magic cookie used in the fa_header_t.cookie member */
	FA_MAGIC_COOKIE_FOOTER = (('F' << 24)|('A' << 16)|('R' << 8)|('F')) /*!< Magic cookie used in the fa_footer_t.cookie member */ 
} fa_magic_cookie_t;

/*! Archive entry container */
struct fa_container_t
{
	fa_offset_t parent;		/*!< Offset to parent container (Relative to start of TOC) */
	fa_offset_t children;		/*!< Offset to child containers (Relative to start of TOC) */
	fa_offset_t next;		/*!< Offset to next sibling container (Relative to start of TOC) */

	fa_offset_t name;		/*!< Offset to container name (Relative to start of TOC) */

	struct
	{
		fa_offset_t offset;	/*! Offset to first entry (Relative to start of TOC) */
		uint32_t count;		/*! Number of entries */
	} entries; /*!< Data entries in container */
};

/*! Data block entry descriptor */
struct fa_entry_t
{
	fa_offset_t data;		/*!< Offset to file data (Relative to start of data stream) */
	fa_offset_t name;		/*!< Offset to name (Relative to start of TOC) */

	uint32_t compression;		/*!< Compression method used in file */
	uint32_t blockSize;		/*!< Block size required when decompressing */

	struct
	{
		uint32_t original;	/*!< Uncompressed size */
		uint32_t compressed;	/*!< Compressed size */	
	} size; /*!< Entry size */
};

/*!
 * \brief Compression block header
 *
 * This header is present before each and every block in a compressed stream (compression != FA_COMPRESSION_NONE)
 *
*/
struct fa_block_t
{
	uint16_t original;		/*!< Size of the original block when decompressed */
	uint16_t compressed; 		/*!< Size of the compressed block in the stream; if the highest bit is set (FILEARCHIVE_COMPRESSION_SIZE_IGNORE), the block is not compressed */
};

/*! Content hash */
struct fa_hash_t
{
	uint8_t data[20]; /*!< 160 bit SHA-1 hash */
};

/*!
 * \brief Table-of-Contents header structure
 *
 * This structure is the first data in the TOC
 *
*/
struct fa_header_t
{
	uint32_t cookie;		/*!< Magic cookie (FA_MAGIC_COOKIE_HEADER) */
	uint32_t version;		/*!< Version of archive */
	uint32_t size;			/*!< Size of TOC */
	uint32_t flags;			/*!< Flags */

	// Version 1

	struct
	{
		fa_offset_t offset;	/*!< Offset to containers (relative to start of TOC) */
		uint32_t count;		/*!< Number of containers in archive */
	} containers; /*!< Container information for archive */

	struct
	{
		fa_offset_t offset;	/*!< Offset to entries (relative to start of TOC) */
		uint32_t count;		/*!< Number of entries in archive */
	} entries; /*!< Entry information for archive */

	fa_offset_t hashes;		/*!< Offset to content hashes (relative to start of TOC) */
};

/*!
 * \brief File archive footer
 *
 * Present and the end of a file archive, contains information on how to locate TOC and data
 */
struct fa_footer_t
{
	uint32_t cookie;		/*!< Magic cookie (FA_MAGIC_COOKIE_FOOTER) */

	struct
	{
		uint32_t compression;	/*!< TOC compression format */
		uint32_t original;	/*!< TOC size, uncompressed */
		uint32_t compressed;	/*!< TOC size, compressed */
		fa_hash_t hash;		/*!< TOC hash */
	} toc; /*!< TOC block information */

	struct
	{
		uint32_t original;	/*!< Data size, uncompressed */
		uint32_t compressed;	/*!< Data size, compressed */
	} data; /*!< Data block information */
};

#define FA_COMPRESSION_SIZE_IGNORE (0x8000) /*!< Compression disable bit for compressed data blocks */ 

#define FA_INVALID_OFFSET (0xffffffff) /*!< Any offset matching this define is not referencing any data and should be considered a NULL pointer */

#endif

