/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include <map>
#include <string>
#include <list>
#include <iostream>
#include <sstream>
#include <cstring>
#include <cstdlib>

#include <fcntl.h>
#include <errno.h>

#include <zip.h>

#include <revolution/external.h>
#include <revolution/support.h>

#if defined(_MACOSX)
#define stricmp strcasecmp
#include <CoreServices/CoreServices.h>
#endif

#ifdef _WINDOWS
#define stricmp _stricmp
#endif

#ifdef _LINUX
#include <unistd.h>
#define stricmp strcasecmp
#endif

#if defined(TARGET_SUBPLATFORM_IPHONE) || defined(TARGET_SUBPLATFORM_ANDROID)
#define stricmp strcasecmp
#include <unistd.h>
#endif

#include <climits>
#include <new>
#include <set>
#include <vector>

#define REVZIP_READ_BUFFER_SIZE 8192

// libzip 1.12 has no per-item progress callback (LiveCode's old modified copy
// of libzip 0.6.1 had one, called from inside zip_fread and zip_close). revZip
// now reports the same messages itself: unpacking from its own read loops, and
// packing from a layer over every source it adds, which sees each block that
// zip_close reads.
struct RevZipArchive;

struct RevZipProgressSource
{
	RevZipArchive *archive;
	zip_source_t *source;
	zip_int64_t index;
	zip_uint64_t progress;
	bool cancelled;
	// The file the item is read from, for revZipAddItemWithFile and
	// revZipReplaceItemWithFile; empty for data.
	std::string file;
};

struct RevZipArchive
{
	zip_t *archive;
	std::string path;
	// The layers over the sources this archive will write at close: each
	// removes itself when libzip frees it (on delete, a later replace, or
	// close), so the set is exactly the items still to be packed.
	std::set<RevZipProgressSource *> sources;
	zip_uint64_t global_progress;
	zip_uint64_t global_total;
	// Set while zip_close runs, and while items are read from the archive:
	// progress messages run script, and a handler that changed, closed or
	// reopened the archive then would free what the operation still uses.
	bool closing;
	int reading;
};

typedef std::map<std::string, RevZipArchive *> zipmap_t;
typedef zipmap_t::iterator zipmap_iterator_t;
typedef zipmap_t::const_iterator zipmap_const_iterator_t;

static zipmap_t s_zip_container;

static char *s_progress_callback = NULL;
static bool s_operation_in_progress = false;
static bool s_operation_cancelled = false;


// Utility function to abstract the process of converting a path to native format
// and resolving it, as this is done several times in this external. The returned
// buffer must be freed by the caller.
char *utilityProcessPath(const char *p_path)
{
    // SN-2014-11-17: [[ Bug 14032 ]] Now gets a UTF-8 encoded string as input
    char *t_resolved_path;
    t_resolved_path = os_path_resolve(p_path);
    
    return t_resolved_path;
}

static RevZipArchive *find_archive_by_name(const char *p_name)
{
	zipmap_const_iterator_t t_it = s_zip_container.find(p_name);
	if (t_it == s_zip_container.end())
		return NULL;

	return t_it->second;
}

// An archive that is being closed or read from can't be changed, closed or
// reopened until that finishes (from a progress message, say).
static bool revzip_archive_busy(const RevZipArchive *p_archive)
{
	return p_archive->closing || p_archive->reading > 0;
}

/*
 Searches for zip* in the container.
 Returns the pointer if successfully, the NULL - if not has found.
*/
struct zip *find_zip_by_name(const char* p_name)
{
	RevZipArchive *t_archive;
	t_archive = find_archive_by_name(p_name);
	return t_archive != NULL ? t_archive->archive : NULL;
}

void* imemdup(const void *p_sptr, size_t p_size)
{
  void *t_dptr;
  // malloc(0) may return NULL, which zip_source_buffer would then refuse.
  t_dptr = malloc(p_size != 0 ? p_size : 1);
  if (t_dptr != NULL)
	  memcpy(t_dptr, p_sptr, p_size);
  return t_dptr;
}

// The libzip message for an error code from zip_open (zip_error_to_str is
// deprecated in libzip 1.x).
static std::string zip_open_error_string(int p_error)
{
	zip_error_t t_error;
	zip_error_init_with_code(&t_error, p_error);
	std::string t_string(zip_error_strerror(&t_error));
	zip_error_fini(&t_error);
	return t_string;
}

// Sends the progress callback message, if one is set. p_type is 0 for
// unpacking and 1 for packing. Returns false if the operation was cancelled.
static bool revzip_progress_message(const char *p_archive_path, const char *p_item,
								  int p_type, zip_uint64_t p_item_progress, zip_uint64_t p_item_total,
								  zip_uint64_t p_global_progress, zip_uint64_t p_global_total)
{
	if (s_progress_callback == NULL)
		return true;

	if (s_operation_cancelled)
		return false;

	// SN-2014-11-17: [[ Bug 14032 ]] The path is kept in UTF-8
	std::ostringstream t_message;
	t_message << s_progress_callback << " \"" << p_archive_path << "\", \""
			  << (p_item != NULL ? p_item : "") << "\", \""
			  << (p_type == 0 ? "unpacking" : "packing") << "\", "
			  << p_item_progress << ", " << p_item_total << ", "
			  << p_global_progress << ", " << p_global_total;

	// SN-2014-11-17: [[ Bug 14032 ]] The name of the callback, and the path, are UTF-8 encoded
	int t_return_value;
	SendCardMessageUTF8(t_message.str().c_str(), &t_return_value);

	return !s_operation_cancelled;
}

// Begins reading an item: sends its first progress message, at 0, and
// starts the operation that revZipCancel cancels. The old libzip sent that
// message for a compressed item from zip_fopen_index, before the operation
// began, so revZipCancel there was refused and the item was read anyway; it
// sent none for a stored item, whose first message could cancel, as this
// one can.
static void revzip_begin_unpacking(const char *p_archive_path, const char *p_item, const zip_stat_t &p_stat)
{
	bool t_stored;
	t_stored = p_stat . comp_method == ZIP_CM_STORE;
	if (!t_stored)
		revzip_progress_message(p_archive_path, p_item, 0, 0, p_stat . size, 0, p_stat . size);

	s_operation_in_progress = true;
	s_operation_cancelled = false;

	if (t_stored)
		revzip_progress_message(p_archive_path, p_item, 0, 0, p_stat . size, 0, p_stat . size);
}

static zip_uint64_t revzip_source_size(zip_source_t *p_source)
{
	zip_stat_t t_stat;
	zip_stat_init(&t_stat);
	if (zip_source_stat(p_source, &t_stat) != 0 || (t_stat.valid & ZIP_STAT_SIZE) == 0)
		return 0;
	return t_stat.size;
}

// Reports the packing progress of one item, as the old libzip did from
// zip_close: once with no progress when the item is opened, then after every
// block read from it.
static bool revzip_report_packing(RevZipProgressSource *p_context)
{
	RevZipArchive *t_archive;
	t_archive = p_context->archive;

	const char *t_name;
	t_name = zip_get_name(t_archive->archive, p_context->index, 0);

	return revzip_progress_message(t_archive->path.c_str(), t_name, 1,
								 p_context->progress, revzip_source_size(p_context->source),
								 t_archive->global_progress, t_archive->global_total);
}

static zip_int64_t revzip_progress_layer(zip_source_t *p_lower, void *p_context, void *p_data, zip_uint64_t p_length, zip_source_cmd_t p_command)
{
	RevZipProgressSource *t_context;
	t_context = (RevZipProgressSource *)p_context;

	switch (p_command)
	{
		case ZIP_SOURCE_OPEN:
			t_context->progress = 0;
			t_context->cancelled = false;
			if (t_context->archive != NULL && !revzip_report_packing(t_context))
			{
				t_context->cancelled = true;
				return -1;
			}
			return 0;

		case ZIP_SOURCE_READ:
		{
			zip_int64_t t_read;
			t_read = zip_source_pass_to_lower_layer(p_lower, p_data, p_length, p_command);
			if (t_read > 0 && t_context->archive != NULL)
			{
				t_context->progress += t_read;
				t_context->archive->global_progress += t_read;
				if (!revzip_report_packing(t_context))
				{
					t_context->cancelled = true;
					return -1;
				}
			}
			return t_read;
		}

		case ZIP_SOURCE_ERROR:
			if (t_context->cancelled)
			{
				zip_error_t t_error;
				zip_error_init_with_code(&t_error, ZIP_ER_CANCELLED);
				zip_int64_t t_result;
				t_result = zip_error_to_data(&t_error, p_data, p_length);
				zip_error_fini(&t_error);
				return t_result;
			}
			return zip_source_pass_to_lower_layer(p_lower, p_data, p_length, p_command);

		case ZIP_SOURCE_FREE:
			if (t_context->archive != NULL)
				t_context->archive->sources.erase(t_context);
			delete t_context;
			return 0;

		case ZIP_SOURCE_GET_FILE_ATTRIBUTES:
		{
			// A device or a pipe added as a file is read as a file (see
			// revzip_file_source), so record it as a regular file with its
			// permissions, not as something an unzipper might make a
			// device of.
			// (libzip fills in what a layer leaves out from the source below
			// it, so this layer gives those attributes itself.)
			if (p_length < sizeof(zip_file_attributes_t))
				return -1;
			zip_file_attributes_t *t_attributes;
			t_attributes = (zip_file_attributes_t *)p_data;
			if (zip_source_get_file_attributes(p_lower, t_attributes) != 0)
				return -1;
			if ((t_attributes->valid & ZIP_FILE_ATTRIBUTES_HOST_SYSTEM) != 0 && t_attributes->host_system == ZIP_OPSYS_UNIX &&
				(t_attributes->valid & ZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES) != 0)
			{
				// S_IFMT and S_IFREG, which Windows does not define.
				t_attributes->external_file_attributes &= ~((zip_uint32_t)0170000 << 16);
				t_attributes->external_file_attributes |= (zip_uint32_t)0100000 << 16;
			}
			return 0;
		}

		default:
			return zip_source_pass_to_lower_layer(p_lower, p_data, p_length, p_command);
	}
}

// Puts the progress layer over p_source. On success the layer owns p_source;
// on failure p_source is freed. The layer reports nothing until
// revzip_attach_source gives it its archive and index.
static zip_source_t *revzip_progress_source(RevZipArchive *p_archive, zip_source_t *p_source, const char *p_file, RevZipProgressSource **r_context)
{
	if (p_source == NULL)
		return NULL;

	RevZipProgressSource *t_context;
	t_context = new (std::nothrow) RevZipProgressSource;
	if (t_context == NULL)
	{
		zip_source_free(p_source);
		return NULL;
	}

	t_context->archive = NULL;
	t_context->source = NULL;
	t_context->index = -1;
	t_context->progress = 0;
	t_context->cancelled = false;
	if (p_file != NULL)
		t_context->file = p_file;

	zip_source_t *t_layer;
	t_layer = zip_source_layered(p_archive->archive, p_source, revzip_progress_layer, t_context);
	if (t_layer == NULL)
	{
		delete t_context;
		zip_source_free(p_source);
		return NULL;
	}

	t_context->source = t_layer;
	*r_context = t_context;
	return t_layer;
}

// Once the item has an index, the layer reports its progress at close.
static void revzip_attach_source(RevZipArchive *p_archive, RevZipProgressSource *p_context, zip_int64_t p_index)
{
	p_context->archive = p_archive;
	p_context->index = p_index;
	p_archive->sources.insert(p_context);
}

// Whether p_source, a file source for a path that exists, is a folder.
// libzip's file sources keep a file's Unix mode, where it has one, in the top
// half of its external attributes; its Windows sources keep none, so there a
// folder is not known.
static bool revzip_source_is_folder(zip_source_t *p_source)
{
	zip_file_attributes_t t_attributes;
	zip_file_attributes_init(&t_attributes);
	if (zip_source_get_file_attributes(p_source, &t_attributes) != 0)
		return false;

	if ((t_attributes.valid & ZIP_FILE_ATTRIBUTES_HOST_SYSTEM) == 0 || t_attributes.host_system != ZIP_OPSYS_UNIX ||
		(t_attributes.valid & ZIP_FILE_ATTRIBUTES_EXTERNAL_FILE_ATTRIBUTES) == 0)
		return false;

	// S_IFMT and S_IFDIR, which Windows does not define.
	return ((t_attributes.external_file_attributes >> 16) & 0170000) == 0040000;
}

static zip_int64_t revzip_folder_callback(void *p_state, void *p_data, zip_uint64_t p_length, zip_source_cmd_t p_command)
{
	time_t *t_mtime;
	t_mtime = (time_t *)p_state;

	switch (p_command)
	{
		case ZIP_SOURCE_OPEN:
		case ZIP_SOURCE_READ:
		case ZIP_SOURCE_CLOSE:
			return 0;

		case ZIP_SOURCE_STAT:
		{
			if (p_length < sizeof(zip_stat_t))
				return -1;
			zip_stat_t *t_stat;
			t_stat = (zip_stat_t *)p_data;
			t_stat->size = 0;
			t_stat->mtime = *t_mtime;
			t_stat->valid |= ZIP_STAT_SIZE | ZIP_STAT_MTIME;
			return sizeof(zip_stat_t);
		}

		case ZIP_SOURCE_ERROR:
		{
			// Only a stat with too small a buffer fails.
			zip_error_t t_error;
			zip_error_init_with_code(&t_error, ZIP_ER_INVAL);
			zip_int64_t t_result;
			t_result = zip_error_to_data(&t_error, p_data, p_length);
			zip_error_fini(&t_error);
			return t_result;
		}

		case ZIP_SOURCE_FREE:
			delete t_mtime;
			return 0;

		case ZIP_SOURCE_SUPPORTS:
			return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, -1);

		default:
			return -1;
	}
}

// The item for a folder added as a file. The old libzip opened the folder at
// close and read nothing from it (on Mac and Linux; on Windows it could not
// open it), so the item was empty, with the folder's date; libzip 1.12 would
// fail the close reading it.
static zip_source_t *revzip_folder_source(zip_t *p_archive, time_t p_mtime)
{
	time_t *t_mtime;
	t_mtime = new (std::nothrow) time_t(p_mtime);
	if (t_mtime == NULL)
	{
		zip_error_set(zip_get_error(p_archive), ZIP_ER_MEMORY, 0);
		return NULL;
	}

	zip_source_t *t_source;
	t_source = zip_source_function(p_archive, revzip_folder_callback, t_mtime);
	if (t_source == NULL)
		delete t_mtime;

	return t_source;
}

// A source that reads the file at p_path when the archive is closed. libzip
// 1.x accepts a file that does not exist (it is how a new archive is made),
// so a missing file is refused here, as the old libzip did when the item was
// added rather than failing the whole close.
//
// The old libzip only checked that the file was there when it was added, and
// at close read it to its end, whatever it was. So a file that is not a
// regular one (a device) is read to its end here too, and so is one that says
// it is empty but is not (those in /proc, whose size is 0), where libzip 1.12
// would read none of it, or fail the close; a folder is an empty item.
static zip_source_t *revzip_file_source(zip_t *p_archive, const char *p_path)
{
	zip_source_t *t_source;
	t_source = zip_source_file(p_archive, p_path, 0, ZIP_LENGTH_TO_END);
	if (t_source == NULL)
	{
		// The path could not be looked at (a part of it is a file, say),
		// which the old libzip reported as a missing file.
		if (zip_error_code_zip(zip_get_error(p_archive)) == ZIP_ER_READ)
			zip_error_set(zip_get_error(p_archive), ZIP_ER_NOENT, 0);
		return NULL;
	}

	zip_stat_t t_stat;
	zip_stat_init(&t_stat);
	if (zip_source_stat(t_source, &t_stat) != 0)
	{
		zip_source_free(t_source);
		zip_error_set(zip_get_error(p_archive), ZIP_ER_NOENT, 0);
		return NULL;
	}

	if ((t_stat.valid & ZIP_STAT_SIZE) == 0)
	{
		if (!revzip_source_is_folder(t_source))
			return t_source;

		time_t t_mtime;
		t_mtime = (t_stat.valid & ZIP_STAT_MTIME) != 0 ? t_stat.mtime : time(NULL);
		zip_source_free(t_source);
		return revzip_folder_source(p_archive, t_mtime);
	}

	if (t_stat.size == 0)
	{
		// Read one byte to see whether the file is really empty: an empty
		// file keeps its source of known length, which libzip stores without
		// the Zip64 header fields a file of unknown length needs.
		zip_source_t *t_to_end;
		t_to_end = zip_source_file(p_archive, p_path, 0, ZIP_LENGTH_UNCHECKED);
		if (t_to_end != NULL)
		{
			bool t_has_data;
			t_has_data = false;
			if (zip_source_open(t_to_end) == 0)
			{
				char t_byte;
				t_has_data = zip_source_read(t_to_end, &t_byte, 1) > 0;
				zip_source_close(t_to_end);
			}

			if (t_has_data)
			{
				zip_source_free(t_source);
				return t_to_end;
			}

			zip_source_free(t_to_end);
		}
	}

	return t_source;
}

// Adds an item from p_source (which may be NULL, when making it failed),
// wrapped in the progress layer; p_file is the file it reads, or NULL. Sets
// r_result to the error on failure.
static void revzip_add_source(RevZipArchive *p_archive, const char *p_name, zip_source_t *p_source, const char *p_file, bool p_compressed, char *&r_result)
{
	RevZipProgressSource *t_context;
	t_context = NULL;

	zip_source_t *t_layer;
	t_layer = revzip_progress_source(p_archive, p_source, p_file, &t_context);

	zip_int64_t t_index;
	t_index = -1;
	if (t_layer != NULL)
	{
		// The name is UTF-8, as the old copy of libzip always marked it.
		t_index = zip_file_add(p_archive->archive, p_name, t_layer, ZIP_FL_ENC_GUESS);
		if (t_index < 0)
			zip_source_free(t_layer);
	}

	if (t_index < 0)
	{
		std::string t_outerr = "ziperr," + std::string((zip_strerror(p_archive->archive)));
		r_result = strdup(t_outerr.c_str());
		return;
	}

	revzip_attach_source(p_archive, t_context, t_index);

	if (!p_compressed)
		zip_set_file_compression(p_archive->archive, t_index, ZIP_CM_STORE, 0);
}

// Replaces item p_index with p_source (which may be NULL, when making it
// failed), wrapped in the progress layer; p_file is the file it reads, or
// NULL. Sets r_result to the error on failure.
static void revzip_replace_source(RevZipArchive *p_archive, zip_int64_t p_index, zip_source_t *p_source, const char *p_file, char *&r_result)
{
	RevZipProgressSource *t_context;
	t_context = NULL;

	zip_source_t *t_layer;
	t_layer = revzip_progress_source(p_archive, p_source, p_file, &t_context);

	if (t_layer == NULL || zip_file_replace(p_archive->archive, p_index, t_layer, 0) < 0)
	{
		if (t_layer != NULL)
			zip_source_free(t_layer);
		std::string t_outerr = "ziperr," + std::string((zip_strerror(p_archive->archive)));
		r_result = strdup(t_outerr.c_str());
		return;
	}

	revzip_attach_source(p_archive, t_context, p_index);
}

// libzip 1.12 fixes a file source's length when the source is made, so a
// file that grew between adding it and closing the archive would be cut
// short, and one that shrank would fail the whole close. The old libzip read
// each file to its end at close: so make every file's source again just
// before closing, with the file's length then. A file that can't be read
// any more keeps its first source, and the close fails on it, as before.
static void revzip_reopen_files(RevZipArchive *p_archive)
{
	// Replacing an item frees its old layer, which takes itself out of the
	// set, so work from a list of the items and their files.
	std::vector<std::pair<zip_int64_t, std::string> > t_files;
	for (std::set<RevZipProgressSource *>::const_iterator t_it = p_archive->sources.begin(); t_it != p_archive->sources.end(); ++t_it)
		if (!(*t_it)->file.empty())
			t_files.push_back(std::make_pair((*t_it)->index, (*t_it)->file));

	for (size_t i = 0; i < t_files.size(); i++)
	{
		zip_source_t *t_source;
		t_source = revzip_file_source(p_archive->archive, t_files[i].second.c_str());
		if (t_source == NULL)
			continue;

		char *t_error;
		t_error = NULL;
		revzip_replace_source(p_archive, t_files[i].first, t_source, t_files[i].second.c_str(), t_error);
		free(t_error);
	}
}

// libzip's message for an error code, with the system error that goes with it.
static std::string revzip_error_string(int p_error, int p_system_error)
{
	zip_error_t t_error;
	zip_error_init(&t_error);
	zip_error_set(&t_error, p_error, p_system_error);
	std::string t_string(zip_error_strerror(&t_error));
	zip_error_fini(&t_error);
	return t_string;
}

// The old libzip looked at the archive's path when it was opened, and failed
// there, with the system's reason, when the path could not be looked at or,
// for "write" and "update", when no archive could be made there. libzip 1.12
// makes a new archive in memory and only finds out at close, after every
// item has been added; and it reports a path it could not look at as a read
// error, and a folder as an unsupported operation. So look at the path first,
// as the old libzip did, and fail with its message. Returns false, with the
// message in r_error, if the open must fail; true leaves it to zip_open (an
// archive that is there, or a missing one to read, which libzip reports as
// "No such file").
static bool revzip_check_path(const char *p_path, int p_flags, std::string &r_error)
{
	// The old libzip could not open an empty path, even to make an archive.
	if (*p_path == '\0')
	{
		r_error = revzip_error_string((p_flags & ZIP_CREATE) != 0 ? ZIP_ER_OPEN : ZIP_ER_NOENT, ENOENT);
		return false;
	}

	zip_error_t t_error;
	zip_error_init(&t_error);

	zip_source_t *t_source;
	t_source = zip_source_file_create(p_path, 0, ZIP_LENGTH_TO_END, &t_error);
	if (t_source == NULL)
	{
		// The path could not be looked at: a part of it is a file, say, or
		// can't be searched (or, on Windows, a folder in it is missing).
		int t_code, t_system;
		t_code = zip_error_code_zip(&t_error);
		t_system = zip_error_code_system(&t_error);
		zip_error_fini(&t_error);
		if (t_code != ZIP_ER_READ)
			return true;

		if (t_system == ENOENT && (p_flags & ZIP_CREATE) == 0)
			r_error = revzip_error_string(ZIP_ER_NOENT, 0);
		else
			r_error = revzip_error_string(ZIP_ER_OPEN, t_system);
		return false;
	}
	zip_error_fini(&t_error);

	bool t_ok;
	t_ok = true;

	zip_stat_t t_stat;
	zip_stat_init(&t_stat);
	if (zip_source_stat(t_source, &t_stat) != 0)
	{
		// Nothing is there. Make the archive's temporary file, as zip_close
		// will, and remove it again: the old libzip made the archive's file
		// (and removed it) to see that it could.
		if ((p_flags & ZIP_CREATE) != 0)
		{
			if (zip_source_begin_write(t_source) != 0)
			{
				r_error = revzip_error_string(ZIP_ER_OPEN, zip_error_code_system(zip_source_error(t_source)));
				t_ok = false;
			}
			else
				zip_source_rollback_write(t_source);
		}
	}
	else if ((p_flags & ZIP_EXCL) != 0)
	{
		r_error = revzip_error_string(ZIP_ER_EXISTS, 0);
		t_ok = false;
	}
	else if ((t_stat.valid & ZIP_STAT_SIZE) == 0)
	{
		// Not a regular file. The old libzip opened a folder, and failed to
		// read it: on Windows a folder can't be opened as a file at all.
#ifdef _WINDOWS
		r_error = revzip_error_string(ZIP_ER_OPEN, EACCES);
		t_ok = false;
#else
		if (revzip_source_is_folder(t_source))
		{
			r_error = revzip_error_string(ZIP_ER_READ, EISDIR);
			t_ok = false;
		}
#endif
	}

	zip_source_free(t_source);
	return t_ok;
}

void revZipOpenArchive(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 2)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	if (!t_error)
	{
        // SN-2014-11-17: [[ Bug 14032 ]] Update the parameters to UTF-8
		if (!SecurityCanAccessFileUTF8(p_arguments[0]))
		{
			t_result = strdup("ziperr,file access not permitted");
			t_error = True;
		}
	}

	char *t_path = NULL;
	if (!t_error)
	{
		t_path = utilityProcessPath(p_arguments[0]);

		if (t_path == NULL)
		{
			t_result = strdup("ziperr,illegal path");
		}
	}

	struct zip *t_archive = NULL;
	int t_err;

	if (t_result == NULL)
	{
		int t_openflag = -1;
		if((stricmp(p_arguments[1], "write")) == 0)
			t_openflag = ZIP_CREATE | ZIP_EXCL;
		if((stricmp(p_arguments[1], "read")) == 0)
			t_openflag = 0;
		if((stricmp(p_arguments[1], "update")) == 0)
			t_openflag = ZIP_CREATE;
		if( t_openflag == -1 )
		{
			t_result = strdup("ziperr,unknown access mode");
			t_error = False;
		}
		else
		{
			RevZipArchive *t_record;
			t_record = NULL;
			RevZipArchive *t_previous;
			t_previous = find_archive_by_name(t_path);
			std::string t_path_error;
			if (t_previous != NULL && revzip_archive_busy(t_previous))
			{
				t_result = strdup("ziperr,archive busy");
			}
			else if (!revzip_check_path(t_path, t_openflag, t_path_error))
			{
				std::string t_outerr = "ziperr," + t_path_error;
				t_result = strdup(t_outerr.c_str());
			}
			else if((t_archive = zip_open(t_path, t_openflag, &t_err)) == NULL)
			{
				// The old libzip called every archive it could not make
				// sense of "Not a zip archive"; libzip 1.12 says what it
				// found wrong (and is stricter).
				if (t_err == ZIP_ER_INCONS || t_err == ZIP_ER_TRUNCATED_ZIP)
					t_err = ZIP_ER_NOZIP;
				std::string t_outerr = "ziperr," + zip_open_error_string(t_err);
				t_result = strdup(t_outerr.c_str());
			}
			else if ((t_record = new (std::nothrow) RevZipArchive) == NULL)
			{
				zip_discard(t_archive);
				t_result = strdup("ziperr,out of memory");
			}
			else
			{
				t_record->archive = t_archive;
				t_record->path = t_path;
				t_record->global_progress = 0;
				t_record->global_total = 0;
				t_record->closing = false;
				t_record->reading = 0;

				// Opening an archive that is already open replaces the first
				// handle, as before; its unsaved changes are discarded rather
				// than leaked.
				if (t_previous != NULL)
				{
					zip_discard(t_previous->archive);
					delete t_previous;
				}

				s_zip_container[t_path] = t_record;
			}
		}
	}

	if( t_result == NULL )
		t_result = strdup("");

	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipCloseArchive(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 1)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if (t_path == NULL)
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	RevZipArchive *t_archive;
	t_archive = NULL;
	if (t_result == NULL)
	{
		t_archive = find_archive_by_name( t_path );
		if( !t_archive )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
		else if (revzip_archive_busy(t_archive))
		{
			t_result = strdup("ziperr,archive busy");
			t_error = False;
		}
	}
	
	if (t_result == NULL)
	{
		int t_err;

		revzip_reopen_files(t_archive);

		// The global total is every byte still to be packed, as the old
		// libzip counted it at the start of zip_close.
		t_archive->global_progress = 0;
		t_archive->global_total = 0;
		for (std::set<RevZipProgressSource *>::const_iterator t_it = t_archive->sources.begin(); t_it != t_archive->sources.end(); ++t_it)
			t_archive->global_total += revzip_source_size((*t_it)->source);

		s_operation_in_progress = true;
		s_operation_cancelled = false;
		t_archive->closing = true;
		t_err = zip_close(t_archive->archive);
		t_archive->closing = false;
		s_operation_in_progress = false;
		
		if (t_err != 0)
		{
			std::string t_outerr = "ziperr," + std::string(zip_strerror(t_archive->archive));

			// A failed close leaves the archive open and the file on disk as
			// it was; the old libzip forgot the handle anyway, so revZip
			// still closes it, discarding the changes.
			zip_discard(t_archive->archive);

			if (!s_operation_cancelled)
				t_result = strdup(t_outerr.c_str());
			t_error = False;
		}

		if (s_operation_cancelled)
		{
			s_operation_cancelled = false;
			t_result = strdup("cancelled");
			t_error = False;
		}

		s_zip_container.erase(t_path);
		delete t_archive;
	}

	if (t_result == NULL)
		t_result = strdup("");

	if (t_path)
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipOpenArchives(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 0)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	if (t_result == NULL)
	{
		std::string t_strlist;
		for(zipmap_const_iterator_t it = s_zip_container.begin(); it != s_zip_container.end(); ++it)
		{
			char* t_line;
            // SN-2014-11-17: [[ Bug 14032 ]] We now keep the UTF-8 encoded string
			t_line = strdup(it->first.c_str());
			if( t_line )
			{
				t_strlist += std::string(t_line);
				t_strlist += "\n";
				free(t_line);
			}
		}
		if( !t_strlist.empty() )
			t_result = strdup(t_strlist.c_str());
	}

	if (t_result == NULL)
		t_result = strdup("");

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}

static void revZipAddItemWithDataAndCompression(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err, bool p_compressed)
{
		char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 3)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	RevZipArchive *t_archive;
	t_archive = NULL;
	if (t_result == NULL)
	{
		t_archive = find_archive_by_name( t_path );
		if( !t_archive )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
		else if (revzip_archive_busy(t_archive))
		{
			t_result = strdup("ziperr,archive busy");
			t_error = False;
		}
	}
	
	if (t_result == NULL)
	{
		ExternalString mcData;
		int intRetValue;
        // SN-2014-11-17: [[ Bug 14032 ]] The variable name is UTF-8 encoded - not the data
		GetVariableExUTF8 (p_arguments[2], "", &mcData, false, &intRetValue);
		if( intRetValue != EXTERNAL_SUCCESS )
		{
			t_result = strdup("ziperr,illegal variable");
			t_error = False;
		}
		else
		{
			char* t_data = NULL;
			t_data = (char*) imemdup(mcData.buffer, mcData.length);
			if (t_data == NULL)
			{
				t_result = strdup("ziperr,out of memory");
				t_error = False;
			}
			else
			{
				zip_source_t *t_buffer;
				t_buffer = zip_source_buffer(t_archive->archive, t_data, mcData.length, 1);
				if (t_buffer == NULL)
					free(t_data);
				revzip_add_source(t_archive, p_arguments[1], t_buffer, NULL, p_compressed, t_result);
			}
		}
	}

	if( t_result == NULL )
		t_result = strdup("");
	
	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}

void revZipAddItemWithData(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	revZipAddItemWithDataAndCompression(p_arguments, p_argument_count, r_result, r_pass, r_err, true);
}

void revZipAddUncompressedItemWithData(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	revZipAddItemWithDataAndCompression(p_arguments, p_argument_count, r_result, r_pass, r_err, false);
}

static void revZipAddItemWithFileAndCompression(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err, bool p_compressed)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 3)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	char *t_filepath = NULL;
	if (t_result == NULL)
	{
		t_path = utilityProcessPath(p_arguments[0]);
		t_filepath = utilityProcessPath(p_arguments[2]);

		if (t_path == NULL || t_filepath == NULL)
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	RevZipArchive *t_archive;
	t_archive = NULL;
	if (t_result == NULL)
	{
		t_archive = find_archive_by_name( t_path );
		if( !t_archive )
		{
			t_result =strdup("ziperr,archive not open");
			t_error = False;
		}
		else if (revzip_archive_busy(t_archive))
		{
			t_result = strdup("ziperr,archive busy");
			t_error = False;
		}
	}

	if (t_result == NULL)
	{
		// The file is read at close, not now, as before (and to its length
		// then: see revzip_reopen_files).
		revzip_add_source(t_archive, p_arguments[1],
						  revzip_file_source(t_archive->archive, t_filepath),
						  t_filepath, p_compressed, t_result);
	}

	if (t_result == NULL)
		t_result = strdup("");

	if( t_path ) 
		free( t_path );
		
	if( t_filepath ) 
		free( t_filepath );

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}

void revZipAddItemWithFile(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	revZipAddItemWithFileAndCompression(p_arguments, p_argument_count, r_result, r_pass, r_err, true);
}

void revZipAddUncompressedItemWithFile(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	revZipAddItemWithFileAndCompression(p_arguments, p_argument_count, r_result, r_pass, r_err, false);
}

void revZipExtractItemToVariable(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 3)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	RevZipArchive *t_record;
	t_record = NULL;
	struct zip *t_archive;
	t_archive = NULL;
	if (t_result == NULL)
	{
		// An item can be read while the archive is being closed (from a
		// packing progress message), as before: zip_close reads only the new
		// items' own sources then, seeks the archive's file before each item
		// it copies from it, and writes to another file.
		t_record = find_archive_by_name( t_path );
		if( !t_record )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
		else
			t_archive = t_record->archive;
	}
	
	zip_int64_t t_index;
	if (t_result == NULL)
	{
		t_index = zip_name_locate(t_archive, p_arguments[1], ZIP_FL_NOCASE);
		if( t_index == -1 )
		{
			t_result = strdup("ziperr,file not found");
			t_error = False;
		}
	}

	struct zip_stat t_stat;
	if (t_result == NULL)
	{
		// The data is read as it was in the file (ZIP_FL_UNCHANGED, below), so
		// its size must be too: a replaced item's new size could be smaller.
		if (zip_stat_index(t_archive, t_index, ZIP_FL_UNCHANGED, &t_stat) != 0)
		{
			std::string t_outerr = "ziperr," + std::string(zip_strerror(t_archive));
			t_result = strdup(t_outerr . c_str());
			t_error = False;
		}
		// A variable holds at most INT_MAX bytes (ExternalString's length is
		// an int). libzip 1.12 reads zip64 items, whose size can be anything
		// up to 2^64, and on 32-bit builds the buffer below would be smaller
		// than the item.
		else if (t_stat . size > (zip_uint64_t)INT_MAX)
		{
			t_result = strdup("ziperr,item too large");
			t_error = False;
		}
	}

	char *t_data = NULL;
	if (t_result == NULL)
	{
		t_data = (char *)malloc(t_stat . size != 0 ? (size_t)t_stat . size : 1);
		if (t_data == NULL)
		{
			t_result = strdup("ziperr,out of memory");
			t_error = False;
		}
	}

	struct zip_file *t_file;
	t_file = NULL;
	if (t_result == NULL)
	{
		t_file = zip_fopen_index(t_archive, t_index, ZIP_FL_UNCHANGED);
		if (t_file == NULL)
		{
			std::string t_outerr = "ziperr," + std::string((zip_strerror(t_archive)));
			t_result = strdup(t_outerr.c_str());
			t_error = False;
		}
	}

	if (t_result == NULL)
	{
		zip_uint64_t t_read;
		t_read = 0;
		bool t_failed;
		t_failed = false;

		std::string t_item(t_stat . name != NULL ? t_stat . name : "");
		
		t_record->reading++;
		revzip_begin_unpacking(t_path, t_item . c_str(), t_stat);
		while(t_read != t_stat . size && !s_operation_cancelled)
		{
			zip_uint64_t t_wanted;
			t_wanted = t_stat . size - t_read;
			if (t_wanted > REVZIP_READ_BUFFER_SIZE)
				t_wanted = REVZIP_READ_BUFFER_SIZE;

			zip_int64_t t_bytes_read;
			t_bytes_read = zip_fread(t_file, t_data + t_read, t_wanted);
			if (t_bytes_read <= 0)
			{
				t_failed = true;
				break;
			}

			t_read += t_bytes_read;
			revzip_progress_message(t_path, t_item . c_str(), 0, t_read, t_stat . size, t_read, t_stat . size);
		}
		// libzip checks an item's CRC when it is read past its end, so read
		// once more: a damaged item is an error, as it is for
		// revZipExtractItemToFile, not data.
		if (!t_failed && !s_operation_cancelled)
		{
			char t_probe;
			if (zip_fread(t_file, &t_probe, 1) != 0)
				t_failed = true;
		}
		t_record->reading--;
		s_operation_in_progress = false;

		if (s_operation_cancelled)
		{
			s_operation_cancelled = false;
			t_result = strdup("cancelled");
			t_error = False;
		}
		else if (!t_failed)
		{
			ExternalString t_mcData;
			int t_retval;

			t_mcData.buffer = t_data;
			t_mcData.length = (int)t_stat . size;
            // SN-2014-11-17: [[ Bug 14032 ]] The variable name is UTF-8 encoded - not the data
			SetVariableExUTF8 (p_arguments[2], "", &t_mcData, false, &t_retval);
			if(t_retval != EXTERNAL_SUCCESS)
			{
				t_result = strdup("ziperr,illegal variable");
				t_error = False;
			}
		}
		else
		{
			// A read error is the file's, not the archive's. One that read
			// too little or too much without an error is a damaged item.
			std::string t_outerr;
			if (zip_error_code_zip(zip_file_get_error(t_file)) != ZIP_ER_OK)
				t_outerr = "ziperr," + std::string(zip_file_strerror(t_file));
			else
				t_outerr = "ziperr,error while reading zipped data";
			t_result = strdup(t_outerr.c_str());
			t_error = False;
		}
	}

	if (t_file != NULL)
		zip_fclose(t_file);

	if (t_data != NULL)
		free(t_data);

	if (t_path != NULL)
		free(t_path);

	if (t_result == NULL)
	{
		t_result = strdup("");
		t_error = False;
	}

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipExtractItemToFile(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = True;

	if (p_argument_count != 3)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	char *t_out_filename = NULL;
	if (t_result == NULL)
	{
		t_path = utilityProcessPath(p_arguments[0]);
		t_out_filename = utilityProcessPath(p_arguments[2]);
		if( t_path == NULL || t_out_filename == NULL )
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	RevZipArchive *t_record;
	t_record = NULL;
	struct zip *t_archive;
	t_archive = NULL;
	if (t_result == NULL)
	{
		// An item can be read while the archive is being closed, as in
		// revZipExtractItemToVariable.
		t_record = find_archive_by_name( t_path );
		if (!t_record)
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
		else
			t_archive = t_record->archive;
	}
	
	zip_int64_t t_index;
	t_index = -1;
	if (t_result == NULL)
	{
		t_index = zip_name_locate(t_archive, p_arguments[1], ZIP_FL_NOCASE);
		if (t_index == -1)
		{
			t_result = strdup("ziperr,file not found");
			t_error = False;
		}
	}
	
	struct zip_stat t_stat;
	if (t_result == NULL)
	{
		if (zip_stat_index(t_archive, t_index, ZIP_FL_UNCHANGED, &t_stat) != 0)
		{
			std::string t_outerr = "ziperr," + std::string(zip_strerror(t_archive));
			t_result = strdup(t_outerr . c_str());
			t_error = False;
		}
	}

	struct zip_file *t_file;
	t_file = NULL;
	if (t_result == NULL)
	{
		t_file = zip_fopen_index(t_archive, t_index, ZIP_FL_UNCHANGED);
		if (t_file == NULL)
		{
			std::string t_outerr = "ziperr," + std::string((zip_strerror(t_archive)));
			t_result = strdup(t_outerr.c_str());
			t_error = False;
		}
	}
	
	FILE *t_out_stream;
	t_out_stream = NULL;
	if (t_result == NULL)
	{
		t_out_stream = fopen(t_out_filename, "wb");
		if (t_out_stream == NULL)
		{
			t_result = strdup("ziperr,unable to open output file");
			t_error = False;
		}
	}
	
	if (t_result == NULL)
	{
		char t_buffer[REVZIP_READ_BUFFER_SIZE];
		zip_int64_t t_read;
		t_read = 0;
		zip_uint64_t t_total_read;
		t_total_read = 0;
		
		std::string t_item(t_stat . name != NULL ? t_stat . name : "");

		t_record->reading++;
		revzip_begin_unpacking(t_path, t_item . c_str(), t_stat);
		// (A stored item can be cancelled from its first message.)
		if (!s_operation_cancelled) do
		{
			t_read = zip_fread(t_file, t_buffer, REVZIP_READ_BUFFER_SIZE);
			if (t_read > 0)
			{
				t_total_read += t_read;
				revzip_progress_message(t_path, t_item . c_str(), 0, t_total_read, t_stat . size, t_total_read, t_stat . size);

				int t_written;
				t_written = fwrite(t_buffer, t_read, 1, t_out_stream);
				if (t_written != 1)
				{
					t_result = strdup("ziperr,error while writing file");
					t_error = False;
				}
			}
			else if (t_read < 0)
			{
				t_result = strdup("ziperr,error while reading zipped data");
				t_error = False;
			}
		}
		while(t_read != 0 && t_result == NULL && !s_operation_cancelled);
		t_record->reading--;
		s_operation_in_progress = false;

		if (s_operation_cancelled)
		{
			s_operation_cancelled = false;
			t_result = strdup("cancelled");
			t_error = False;
		}
	}
	
	if (t_out_stream != NULL)
	{
		fclose(t_out_stream);
		if (t_result != NULL)
			unlink(t_out_filename);
	}
	
	if (t_file != NULL)
		zip_fclose(t_file);

	if (t_path != NULL)
		free(t_path);
		
	if (t_out_filename != NULL)
		free(t_out_filename);
	
	if (t_result == NULL)
	{
		t_result = strdup("");
		t_error = False;
	}
	
	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}

void revZipReplaceItemWithFile(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 3)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	char *t_filepath = NULL;
	if (t_result == NULL)
	{
		t_path = utilityProcessPath(p_arguments[0]);
		t_filepath = utilityProcessPath(p_arguments[2]);
		if (t_path == NULL || t_filepath == NULL)
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	RevZipArchive *t_archive;
	t_archive = NULL;
	if(t_result == NULL)
	{
		t_archive = find_archive_by_name( t_path );
		if( !t_archive )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = True;
		}
		else if (revzip_archive_busy(t_archive))
		{
			t_result = strdup("ziperr,archive busy");
			t_error = False;
		}
	}

	zip_int64_t t_index;

	if(t_result == NULL)
	{
		t_index = zip_name_locate(t_archive->archive, p_arguments[1], ZIP_FL_NOCASE);
		if( t_index == -1 )
		{
			t_result = strdup("ziperr,file not found");
			t_error = False;
		}
		else
		{
			revzip_replace_source(t_archive, t_index,
								  revzip_file_source(t_archive->archive, t_filepath),
								  t_filepath, t_result);
		}
	}

	if (t_result == NULL)
		t_result = strdup("");

	if( t_path ) 
		free( t_path );
	if( t_filepath ) 
		free( t_filepath );

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipReplaceItemWithData(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 3)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	RevZipArchive *t_archive;
	zip_int64_t t_index;
	t_archive = NULL;
	if (t_result == NULL)
	{
		t_archive = find_archive_by_name( t_path );
		if( !t_archive )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
		else if (revzip_archive_busy(t_archive))
		{
			t_result = strdup("ziperr,archive busy");
			t_error = False;
		}
	}
	
	if (t_result == NULL)
	{
		t_index = zip_name_locate(t_archive->archive, p_arguments[1], ZIP_FL_NOCASE);
		if( t_index == -1 )
		{
			t_result = strdup("ziperr,file not found");
			t_error = False;
		}
		else
		{
			ExternalString mcData;
			int intRetValue;
            // SN-2014-11-17: [[ Bug 14032 ]] The variable name is UTF-8 encoded - not the data
			GetVariableExUTF8 (p_arguments[2], "", &mcData, false, &intRetValue);
			if( intRetValue != EXTERNAL_SUCCESS )
			{
				t_result = strdup("ziperr,illegal variable");
				t_error = False;
			}
			else
			{
				char* t_data = NULL;
				t_data = (char*) imemdup(mcData.buffer, mcData.length);
				if (t_data == NULL)
				{
					t_result = strdup("ziperr,out of memory");
					t_error = False;
				}
				else
				{
					zip_source_t *t_buffer;
					t_buffer = zip_source_buffer(t_archive->archive, t_data, mcData.length, 1);
					if (t_buffer == NULL)
						free(t_data);
					revzip_replace_source(t_archive, t_index, t_buffer, NULL, t_result);
				}
			}
		}
	}

	if( t_result == NULL )
		t_result = strdup("");
	
	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipRenameItem(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 3)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	struct zip *t_archive;
	zip_int64_t t_index;
	t_archive = NULL;
	if( t_result == NULL )
	{
		t_archive = find_zip_by_name( t_path );
		if( !t_archive )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
		else if (revzip_archive_busy(find_archive_by_name(t_path)))
		{
			t_result = strdup("ziperr,archive busy");
			t_error = False;
		}
	}
	
	if( t_result == NULL )
	{
		t_index = zip_name_locate(t_archive, p_arguments[1], ZIP_FL_NOCASE);
		if( t_index == -1 )
		{
			t_result = strdup("ziperr,file not found");
			t_error = False;
		}
		else
		{
			if (zip_file_rename(t_archive, t_index, p_arguments[2], ZIP_FL_ENC_GUESS) != 0)
			{
				std::string t_outerr = "ziperr," + std::string((zip_strerror(t_archive)));
				t_result = strdup(t_outerr.c_str());
				t_error = False;
			}
		}
	}

	if( t_result == NULL )
		t_result = strdup("");
	
	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipGetItemAttributes(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 2)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	struct zip *t_archive;
	zip_int64_t t_index;
	t_archive = NULL;
	if( t_result == NULL )
	{
		t_archive = find_zip_by_name(t_path);
		if( !t_archive )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
	}
	
	if( t_result == NULL )
	{
		t_index = zip_name_locate(t_archive, p_arguments[1], ZIP_FL_NOCASE);
		if( t_index == -1 )
		{
			t_result = strdup("ziperr,file not found");
			t_error = False;
		}
		else
		{
			// The host system ("made by") and the external attributes.
			zip_uint8_t t_madeby;
			zip_uint32_t t_attributes;

			if (zip_file_get_external_attributes(t_archive, t_index, 0, &t_madeby, &t_attributes) != 0)
			{
				std::string t_outerr = "ziperr," + std::string((zip_strerror(t_archive)));
				t_result = strdup(t_outerr.c_str());
				t_error = False;
			}
			else
			{
				// 3 for the madeby, 1 for the separating comma, 10 for the attributes and 1 for a null termination.
				t_result = (char *)malloc(3 + 1 + 10 + 1);
				sprintf(t_result, "%u,%u", (unsigned int)t_madeby, (unsigned int)t_attributes);
				t_error = False;
			}
		}
	}

	if( t_result == NULL )
		t_result = strdup("");
	
	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;

}


// arguments[0] : archive path
// arguments[1] : item name
// arguments[2] : made by (integer between 0 and 255)
// arguments[3] : external attributes (integer)
void revZipSetItemAttributes(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 4)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	struct zip *t_archive;
	zip_int64_t t_index;
	t_archive = NULL;
	if( t_result == NULL )
	{
		t_archive = find_zip_by_name( t_path );
		if( !t_archive )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
		else if (revzip_archive_busy(find_archive_by_name(t_path)))
		{
			t_result = strdup("ziperr,archive busy");
			t_error = False;
		}
	}
	
	if( t_result == NULL )
	{
		t_index = zip_name_locate(t_archive, p_arguments[1], ZIP_FL_NOCASE);
		if( t_index == -1 )
		{
			t_result = strdup("ziperr,file not found");
			t_error = False;
		}
		else
		{
			unsigned char t_madeby;
			t_madeby = (unsigned char)atoi(p_arguments[2]);

			// Attributes with the top bit set (a Unix mode in the high half)
			// are above INT_MAX, which atoi cannot read.
			unsigned int t_attributes;
			t_attributes = (unsigned int)strtoul(p_arguments[3], NULL, 10);

			if (zip_file_set_external_attributes(t_archive, t_index, 0, t_madeby, t_attributes) != 0)
			{
				std::string t_outerr = "ziperr," + std::string((zip_strerror(t_archive)));
				t_result = strdup(t_outerr.c_str());
				t_error = False;
			}
		}
	}

	if( t_result == NULL )
		t_result = strdup("");
	
	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipDeleteItem(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 2)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			t_result = strdup("ziperr,illegal path");
			t_error = False;
		}
	}

	struct zip *t_archive;
	zip_int64_t t_index;
	t_archive = NULL;
	if (t_result == NULL)
	{
		t_archive = find_zip_by_name( t_path );
		if( !t_archive )
		{
			t_result = strdup("ziperr,archive not open");
			t_error = False;
		}
		else if (revzip_archive_busy(find_archive_by_name(t_path)))
		{
			t_result = strdup("ziperr,archive busy");
			t_error = False;
		}
	}
	
	if (t_result == NULL)
	{
		t_index = zip_name_locate(t_archive, p_arguments[1], ZIP_FL_NOCASE);
		if( t_index == -1 )
		{
			t_result = strdup("ziperr,file not found");
			t_error = False;
		}
		else
		{
			if (zip_delete(t_archive, t_index) != 0)
			{
				std::string t_outerr = "ziperr," + std::string((zip_strerror(t_archive)));
				t_result = strdup(t_outerr.c_str());
				t_error = False;
			}
		}
	}

	if( t_result == NULL )
		t_result = strdup("");
	
	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipEnumerateItems(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 1)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			//t_result = strdup("ziperr,illegal path");
			t_result = strdup("");
			t_error = False;
		}
	}

	struct zip *t_archive;
	t_archive = NULL;
	if (t_result == NULL)
	{
		t_archive = find_zip_by_name( t_path );
		if( !t_archive )
		{
			//t_result = strdup("ziperr,archive not open");
			t_result = strdup("");
			t_error = False;
		}
	}
	
	if (t_result == NULL)
	{
		zip_int64_t t_num_files;
		t_num_files = zip_get_num_entries(t_archive, 0);
		std::string t_str_names;
	
		for( zip_int64_t i = 0; i < t_num_files; ++i )
		{
			struct zip_stat t_stat;

			if (zip_stat_index(t_archive, i, 0, &t_stat) != 0)
			{
				// An item deleted since the archive was opened keeps its
				// index until close; it is not listed. One that came from
				// the file reports ZIP_ER_DELETED; one added since the
				// archive was opened and then deleted has nothing left, and
				// libzip reports ZIP_ER_INVAL (i is always a valid index).
				int t_code;
				t_code = zip_error_code_zip(zip_get_error(t_archive));
				if (t_code == ZIP_ER_DELETED || t_code == ZIP_ER_INVAL)
					continue;

				std::string t_outerr = "ziperr," + std::string((zip_strerror(t_archive)));
				t_result = strdup(t_outerr.c_str());
                // SN-2015-06-02: [[ CID 90610 ]] Quit the loop if an error is
                //  encountered - and set t_error to the right value.
				t_error = True;
                break;
			}
			else
			{
				// SN-2015-03-10: [[ Bug 14413 ]] revZipEnumerateItems returns
				//  UTF-8. libzip gives every name as UTF-8: as stored when the
				//  item says it is UTF-8 (as every item revZip writes does),
				//  or is valid UTF-8, and otherwise converted from code page
				//  437, the encoding the zip format specifies.
				t_str_names += std::string(t_stat.name);
				t_str_names += "\n";
			}
		}
		
		if( !t_str_names.empty() && t_error == False)
		{
			t_result = strdup(t_str_names.c_str());
			t_error = False;
		}
	}

	if( t_result == NULL )
		t_result = strdup("");
	
	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}


void revZipDescribeItem(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count != 2)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	char *t_path = NULL;
	if( t_result == NULL )
	{
		t_path = utilityProcessPath(p_arguments[0]);
		if( t_path == NULL )
		{
			//t_result = strdup("ziperr,illegal path");
			t_result = strdup("");
			t_error = False;
		}
	}

	RevZipArchive *t_record;
	t_record = NULL;
	struct zip *t_archive;
	zip_int64_t t_index;
	t_archive = NULL;
	if (t_result == NULL)
	{
		t_record = find_archive_by_name( t_path );
		if( !t_record )
		{
			//t_result = strdup("ziperr,archive not open");
			t_result = strdup("");
			t_error = False;
		}
		else
			t_archive = t_record->archive;
	}

	if (t_result == NULL)
	{
		t_index = zip_name_locate(t_archive, p_arguments[1], ZIP_FL_NOCASE);
		if( t_index == -1 )
		{
			//t_result = strdup("ziperr,file not found");
			t_result = strdup("");
			t_error = False;
		}
		else
		{
			// An item whose data is still to be written (added or replaced
			// since the archive was opened) has no compressed size or method
			// yet: the old libzip said -1 and "none" (and a CRC of 0, which
			// libzip 1.12 says too). libzip 1.12 says the data's size and
			// whatever method the item had.
			bool t_pending;
			t_pending = false;
			for (std::set<RevZipProgressSource *>::const_iterator t_it = t_record->sources.begin(); t_it != t_record->sources.end(); ++t_it)
				if ((*t_it)->index == t_index)
					t_pending = true;

			struct zip_stat t_stat_data;
			struct zip_stat* t_stat = &t_stat_data;
			if (zip_stat_index(t_archive, t_index, 0, t_stat) == 0)
			{
				std::stringstream t_strstream;
				t_strstream << t_stat->index << "," << t_stat->crc << "," << t_stat->size << ",";
				t_strstream << (long long)t_stat->mtime << ",";
				if (t_pending)
					t_strstream << "-1,";
				else
					t_strstream << t_stat->comp_size << ",";
				switch( t_pending ? ZIP_CM_STORE : t_stat->comp_method )
				{
				case 0:
					t_strstream << "none";
					break;
				case 1:
					t_strstream << "shrink";
					break;
				case 2:
					t_strstream << "reduce_1";
					break;
				case 3:
					t_strstream << "reduce_2";
					break;
				case 4:
					t_strstream << "reduce_3";
					break;
				case 5:
					t_strstream << "reduce_4";
					break;
				case 6:
					t_strstream << "implode";
					break;
				case 8:
					t_strstream << "deflate";
					break;
				case 9:
					t_strstream << "deflate64";
					break;
				case 10:
					t_strstream << "pkware_implode";
					break;
				default:
					t_strstream << "unknown";
				}
				t_strstream << std::ends;
				t_result = strdup(t_strstream.str() . c_str());
			}
		}
	}

	if (t_result == NULL)
		t_result = strdup("");
	
	if( t_path )
		free(t_path);

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}

void revZipSetProgressCallback(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;

	if (p_argument_count > 1)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}

	if (t_result == NULL)
	{
		if (s_progress_callback != NULL)
		{
			free(s_progress_callback);
			s_progress_callback = NULL;
		}
		s_progress_callback = p_argument_count == 0 || *p_arguments[0] == '\0' ? NULL : strdup(p_arguments[0]);
	}

	if (t_result == NULL)
		t_result = strdup("");

	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}

void revZipCancel(char *p_arguments[], int p_argument_count, char **r_result, Bool *r_pass, Bool *r_err)
{
	char *t_result = NULL;
	Bool t_error = False;
	
	if (p_argument_count != 0)
	{
		t_result = strdup("ziperr,illegal arguments");
		t_error = True;
	}
	else if (!s_operation_in_progress)
	{
		t_result = strdup("ziperr,no current operation");
	}

	if (t_result == NULL)
		s_operation_cancelled = true;

	if (t_result == NULL)
		t_result = strdup("");
		
	*r_pass = False;
	*r_err = t_error;
	*r_result = t_result;
}

// SN-2014-11-17: [[ Bug 14032 ]] Update the appropriate functions to get UTF-8 parameters
EXTERNAL_BEGIN_DECLARATIONS("revZip")
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipOpenArchive", revZipOpenArchive)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipCloseArchive", revZipCloseArchive)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipAddItemWithFile", revZipAddItemWithFile)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipAddUncompressedItemWithFile", revZipAddUncompressedItemWithFile)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipAddItemWithData", revZipAddItemWithData)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipAddUncompressedItemWithData", revZipAddUncompressedItemWithData)
	EXTERNAL_DECLARE_FUNCTION_UTF8("revZipOpenArchives", revZipOpenArchives)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipExtractItemToVariable", revZipExtractItemToVariable)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipExtractItemToFile", revZipExtractItemToFile)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipReplaceItemWithFile", revZipReplaceItemWithFile)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipReplaceItemWithData", revZipReplaceItemWithData)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipRenameItem", revZipRenameItem)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipDeleteItem", revZipDeleteItem)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipSetItemAttributes", revZipSetItemAttributes)
	EXTERNAL_DECLARE_FUNCTION_UTF8("revZipGetItemAttributes", revZipGetItemAttributes)
	EXTERNAL_DECLARE_FUNCTION_UTF8("revZipEnumerateItems", revZipEnumerateItems)
	EXTERNAL_DECLARE_FUNCTION_UTF8("revZipDescribeItem", revZipDescribeItem)
	EXTERNAL_DECLARE_COMMAND_UTF8("revZipSetProgressCallback", revZipSetProgressCallback)
	EXTERNAL_DECLARE_COMMAND("revZipCancel", revZipCancel)
EXTERNAL_END_DECLARATIONS

#ifdef _WINDOWS

#include <windows.h>

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved)
{
#ifdef _DEBUG
	if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		s_zip_container . clear();
		_CrtDumpMemoryLeaks();
	}
#endif

    return TRUE;
}

#endif

#ifdef TARGET_SUBPLATFORM_IPHONE
extern "C"
{
extern struct LibInfo __libinfo;
__attribute((section("__DATA,__libs"))) volatile struct LibInfo *__libinfoptr_revzip __attribute__((__visibility__("default"))) = &__libinfo;
}
#endif
