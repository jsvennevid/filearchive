#ifndef FILEARCHIVE_API_H
#define FILEARCHIVE_API_H

#if !defined(FILEARCHIVE_INTERNAL_API_H)
typedef void fa_archive_t;
typedef void fa_file_t;
typedef void fa_dir_t;
#endif

#include "filearchive.h"

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct fa_dirinfo_t fa_dirinfo_t;
typedef struct fa_archiveinfo_t fa_archiveinfo_t;

typedef enum
{
	FA_MODE_READ = 0,
	FA_MODE_WRITE = 1
} fa_mode_t;

typedef enum
{
	FA_SEEK_SET = 0,
	FA_SEEK_CURR = 1,
	FA_SEEK_END = 2
} fa_seek_t;

typedef enum
{
	FA_ENTRY_FILE = 0,
	FA_ENTRY_DIR = 1,
} fa_entrytype_t;

struct fa_dirinfo_t
{
	const char* name; /*!< Current entry name, only valid as long as archive is opened */

	fa_entrytype_t type;
	fa_compression_t compression;

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

/*!
 *
 * Open archive for reading or writing
 *
 * \param filename Path to archive
 * \param mode Mode to use when opening
 * \param alignment Alignment for resulting archive when writing (when reading, pass 0)
 * \param info When reading, this structure will be filled with info about the archive (can be NULL)
 *
 * \note Currently opening existing archives (already containing data) for writing is not supported, all archives will be started from a clean slate
**/
fa_archive_t* fa_open_archive(const char* filename, fa_mode_t mode, uint32_t alignment, fa_archiveinfo_t* info);

/*!
 *
 * Close previously opened archive and finalize changes
 *
 * \param archive Archive to close
 * \param compression Compression to use for the TOC when writing (else pass FA_COMPRESSION_NONE)
 * \param info When writing, this structure will be filled with info about the archive (can be NULL)
 *
**/
int fa_close_archive(fa_archive_t* archive, fa_compression_t compression, fa_archiveinfo_t* info);

/*!
 *
 * Open a file in an archive for reading or writing (mode is implicit from how the archive was opened)
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
**/
fa_file_t* fa_open_file(fa_archive_t* archive, const char* filename, fa_compression_t compression, fa_dirinfo_t* info);

/*!
 *
 * Close a file and finalize changes
 *
 * \param file File to close
 * \param info If specified, this instance will receive information about the file (including final compressed size and content hash)
 *
 * \return 0 if operation was successful, <0 otherwise
 *
**/
int fa_close_file(fa_file_t* file, fa_dirinfo_t* info);

/*!
 *
 * Read data from file
 *
 * \param file File to read data from
 * \param buffer Buffer that receives read data
 *
 * \return Number of bytes read from file
 *
 * \note This method will transparently decompress data
 *
**/
size_t fa_read_file(fa_file_t* file, void* buffer, size_t length);

/*!
 *
 * Write data to file
 *
 * \param file File to write data into
 * \param buffer Buffer that contains data to write
 * \param length Number of bytes
 *
 * \return Number of bytes written; in case of an error this will NOT match the incoming length parameter
 *
**/
size_t fa_write_file(fa_file_t* file, const void* buffer, size_t length);

/*!
 *
 * Seek to a new location in file
 *
 * \param file File to seek in
 * \param offset Offset to use when seeking
 * \param whence What mode to use when seeking
 *
 * \return 0 if seeking was successful, <0 otherwise
 *
 * \note Seeking when writing is not supported
 *
**/
int fa_seek(fa_file_t* file, int64_t offset, fa_seek_t whence);

/*!
 *
 * Return current location in file
 *
 * \param file File to query for current file pointer
 *
 * \return Offset in file
 *
**/
size_t fa_tell(fa_file_t* file);

/*!
 *
 * Begin enumerating directory
 *
 * \param archive Archive to enumerate in
 * \param dir Path to enumerate
 *
 * \return Handle to use when enumerating
 *
 * \note Enumerating when writing is not supported
 *
**/
fa_dir_t* fa_open_dir(fa_archive_t* archive, const char* dir);

/*!
 *
 * Enumerate directory entry
 *
 * \param dir Directory currently being enumerated
 * \param dirinfo Directory information structure that data should be stored into
 *
 * \return 0 if a new entry was enumerated, <0 otherwise
 *
**/
int fa_read_dir(fa_dir_t* dir, fa_dirinfo_t* dirinfo);

/*!
 *
 * Complete enumeration of directory
 *
 * \param dir Directory to finish enumerating
 *
 * \return 0 if operation was successful, <0 otherwise
 *
**/
int fa_close_dir(fa_dir_t* dir);

#if defined(__cplusplus)
}
#endif

#endif
