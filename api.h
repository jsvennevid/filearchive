#ifndef FILEARCHIVE_API_H
#define FILEARCHIVE_API_H

#if !defined(FILEARCHIVE_INTERNAL_API_H)
typedef void fa_archive_t;
typedef void fa_file_t;
typedef void fa_dir_t;
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct fa_dirinfo_t fa_dirinfo_t;

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
	FA_COMMIT_DISCARD = 0, /*!< Discard changes done on archive */
	FA_COMMIT_WRITE = 1 /*!< Write changes done to archive */
} fa_commit_t;

struct fa_dirinfo_t
{
	const char* name; /*!< Current entry name, only valid as long as archive is opened */
};

fa_archive_t* fa_open_archive(const char* filename, fa_mode_t mode);
int fa_close_archive(fa_archive_t* archive);

/*!
 *
 * Commit changes done on a writeable archive
 *
 * \param archive - Archive to commit changes on
 * \param type - Commit type
 *
**/
int fa_commit_archive(fa_archive_t* archive, fa_commit_t type);

fa_file_t* fa_open_file(fa_archive_t* archive, const char* filename, fa_mode_t mode);
int fa_close_file(fa_file_t* file);
int fa_delete_file(fa_archive_t* archive, const char* filename);

size_t fa_read(fa_file_t* file, void* buffer, size_t length);
size_t fa_write(fa_file_t* file, const void* buffer, size_t length);
int fa_seek(fa_file_t* file, off_t offset, fa_seek_t whence);

fa_dir_t* fa_open_dir(fa_archive_t* archive, const char* dir);
int fa_close_dir(fa_dir_t* dir);


#if defined(__cplusplus)
}
#endif

#endif

