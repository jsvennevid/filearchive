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

#ifndef FILEARCHIVE_API_H
#define FILEARCHIVE_API_H

/*!
 * \file api.h
 * \brief Public API documentation for filearchive
 */

#if !defined(FILEARCHIVE_INTERNAL_API_H)
typedef void fa_archive_t; /*!< Archive handle */
typedef void fa_file_t; /*!< File handle */
typedef void fa_dir_t; /*!< Directory handle */
#endif

#include "filearchive.h"

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct fa_dirinfo_t fa_dirinfo_t;
typedef struct fa_archiveinfo_t fa_archiveinfo_t;

/*! Mode for archive access */
typedef enum
{
	FA_MODE_READ = 0, /*!< Open archive for read access. No methods writing data to the archive are accessible in this mode */
	FA_MODE_WRITE = 1 /*!< Open archive for write access. No methods reading, seeking or enumerating entries in the archive are available in this mode */
} fa_mode_t;

/*! What origin to use when seeking inside an archive file */
typedef enum
{
	FA_SEEK_SET = 0, /*!< Origin is set to the beginning of the file */
	FA_SEEK_CURR = 1, /*!< The current offset is used as the origin */
	FA_SEEK_END = 2 /*!< Origin is set to the end of the file */
} fa_seek_t;

/*! What kind of entry that has been enumerated */
typedef enum
{
	FA_ENTRY_FILE = 0, /*!< Entry is a file - name, compression, size and hash are valid */
	FA_ENTRY_DIR = 1, /*!< Entry is a directory - name is valid */
} fa_entrytype_t;

struct fa_dirinfo_t
{
	const char* name; /*!< Current entry name, only valid as long as archive is opened */

	fa_entrytype_t type; /*!< Type of the enumerated entry */
	fa_compression_t compression; /*!< What kind of compression that has been used; only valid for files */

	struct
	{
		uint32_t original;
		uint32_t compressed;
	} size;

	fa_hash_t hash;
};

struct fa_archiveinfo_t
{
	fa_header_t header;
	fa_footer_t footer;
};

/*! \defgroup Archive
 * \{ */

/*!
 *
 * \brief Open archive for reading or writing
 *
 * \param filename Path to archive
 * \param mode Mode to use when opening
 * \param alignment Alignment for resulting archive when writing (when reading, pass 0)
 * \param info When reading, this structure will be filled with info about the archive (can be NULL)
 *
 * \note Currently opening existing archives (already containing data) for writing is not supported, all archives will be started from a clean slate
 */
fa_archive_t* fa_open_archive(const char* filename, fa_mode_t mode, uint32_t alignment, fa_archiveinfo_t* info);

/*!
 *
 * \brief Close previously opened archive and finalize changes
 *
 * \param archive Archive to close
 * \param compression Compression to use for the TOC when writing (else pass FA_COMPRESSION_NONE)
 * \param info When writing, this structure will be filled with info about the archive (can be NULL)
 *
 */
int fa_close_archive(fa_archive_t* archive, fa_compression_t compression, fa_archiveinfo_t* info);

/*! \} */

/*! \defgroup File
 * \{ */

/*!
 *
 * \brief Open a file in an archive for reading or writing
 *
 * File mode is decided by how the archive was initially opened.
 *
 * \param archive Archive to access
 * \param filename File to access
 * \param compression What compression to use (when reading, pass FA_COMPRESSION_NONE)
 * \param dirinfo If specified when opening for reading, this instance will receive information about the file
 *
 * \return File ready to access, or NULL on error
 *
 * \note When writing, opening a file with the same name more than once will NOT replace the old one; a new instance will be created (but will be inaccessible by name)
 * \note When opening a file for reading, passing @ followed by a 40-character hexadecimal string will allow opening a file for access through its content hash
 *
 */
fa_file_t* fa_open_file(fa_archive_t* archive, const char* filename, fa_compression_t compression, fa_dirinfo_t* info);

/*!
 *
 * \brief Open an entry in the archive based on its content hash
 *
 * This is an optimized path if you already have the content hash in a binary format
 *
 * \param archive Archive to access
 * \param hash Hash to use as key
 *
 * \return File ready to read from
 *
 * \note This only supports read-access for obvious reasons
 */
fa_file_t* fa_open_hash(fa_archive_t* archive, const fa_hash_t* hash);

/*!
 *
 * \brief Close a file and finalize changes
 *
 * \param file File to close
 * \param info If specified, this instance will receive information about the file (including final compressed size and content hash)
 *
 * \return 0 if operation was successful, <0 otherwise
 *
 */
int fa_close_file(fa_file_t* file, fa_dirinfo_t* info);

/*!
 *
 * \brief Read data from file
 *
 * \param file File to read data from
 * \param buffer Buffer that receives read data
 *
 * \return Number of bytes read from file
 *
 * \note This method will transparently decompress data
 *
 */
size_t fa_read_file(fa_file_t* file, void* buffer, size_t length);

/*!
 *
 * \brief Write data to file
 *
 * \param file File to write data into
 * \param buffer Buffer that contains data to write
 * \param length Number of bytes
 *
 * \return Number of bytes written; in case of an error this will NOT match the incoming length parameter
 *
 */
size_t fa_write_file(fa_file_t* file, const void* buffer, size_t length);

/*!
 *
 * \brief Seek to a new location in file
 *
 * \param file File to seek in
 * \param offset Offset to use when seeking
 * \param whence What mode to use when seeking
 *
 * \return 0 if seeking was successful, <0 otherwise
 *
 * \note Seeking when writing is not supported
 *
 */
int fa_seek(fa_file_t* file, int64_t offset, fa_seek_t whence);

/*!
 *
 * \brief Return current location in file
 *
 * \param file File to query for current file pointer
 *
 * \return Offset in file
 *
 */
size_t fa_tell(fa_file_t* file);

/* \} */

/*! \defgroup Directory
 * \{ */

/*!
 *
 * \brief Begin enumerating directory
 *
 * \param archive Archive to enumerate in
 * \param dir Path to enumerate
 *
 * \return Handle to use when enumerating
 *
 * \note Enumerating when writing is not supported
 *
 */
fa_dir_t* fa_open_dir(fa_archive_t* archive, const char* dir);

/*!
 *
 * \brief Enumerate directory entry
 *
 * \param dir Directory currently being enumerated
 * \param dirinfo Directory information structure that data should be stored into
 *
 * \return 0 if a new entry was enumerated, <0 otherwise
 *
 */
int fa_read_dir(fa_dir_t* dir, fa_dirinfo_t* dirinfo);

/*!
 *
 * \brief Complete enumeration of directory
 *
 * \param dir Directory to finish enumerating
 *
 * \return 0 if operation was successful, <0 otherwise
 *
 */
int fa_close_dir(fa_dir_t* dir);

/*!
 * \}
 */

#if defined(__cplusplus)
}
#endif

#endif

