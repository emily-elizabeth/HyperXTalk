/* cairo - a vector graphics library with display and print output
 *
 * Copyright � 2002 University of Southern California
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Ian Macphail <ian@runrev.com>
 */

#ifndef CAIRO_PDF_EXT_OBJECT_H
#define CAIRO_PDF_EXT_OBJECT_H

typedef enum _cairo_pdf_value_type
{
	// basic types
	CAIRO_PDF_VALUE_TYPE_BOOLEAN,
	CAIRO_PDF_VALUE_TYPE_INTEGER,
	CAIRO_PDF_VALUE_TYPE_REAL,
	CAIRO_PDF_VALUE_TYPE_STRING,
	CAIRO_PDF_VALUE_TYPE_NAME,
	CAIRO_PDF_VALUE_TYPE_ARRAY,
	CAIRO_PDF_VALUE_TYPE_DICTIONARY,
	CAIRO_PDF_VALUE_TYPE_STREAM,
	CAIRO_PDF_VALUE_TYPE_NULL,

	// composite types
	CAIRO_PDF_VALUE_TYPE_ACTION,
	CAIRO_PDF_VALUE_TYPE_ANNOTATION,
	CAIRO_PDF_VALUE_TYPE_DATE,
	CAIRO_PDF_VALUE_TYPE_DEST,
	CAIRO_PDF_VALUE_TYPE_REFERENCE,
	CARIO_PDF_OBJECT_TYPE_OUTLINE_ENTRY,
} cairo_pdf_value_type_t;

// currently, only link & uri action annotations are supported
typedef enum _cairo_pdf_action_type
{
	CAIRO_PDF_ACTION_TYPE_URI,
} cairo_pdf_action_type_t;

typedef enum _cairo_pdf_annotation_type
{
	CAIRO_PDF_ANNOTATION_TYPE_LINK,
} cairo_pdf_annotation_type_t;

typedef enum _cairo_pdf_dest_type
{
	CAIRO_PDF_DEST_TYPE_XYZ,
	CAIRO_PDF_DEST_TYPE_FIT,
	CAIRO_PDF_DEST_TYPE_FIT_H,
	CAIRO_PDF_DEST_TYPE_FIT_V,
	CAIRO_PDF_DEST_TYPE_FIT_R,
	CAIRO_PDF_DEST_TYPE_FIT_B,
	CAIRO_PDF_DEST_TYPE_FIT_BH,
	CAIRO_PDF_DEST_TYPE_FIT_BV,
} cairo_pdf_dest_type_t;

struct _cairo_pdf_value;

typedef struct _cairo_pdf_array
{
	unsigned int size;
	struct _cairo_pdf_value *elements;
} cairo_pdf_array_t;

typedef struct _cairo_pdf_dictionary
{
	unsigned int size;
	const char **keys;
	struct _cairo_pdf_value *elements;
} cairo_pdf_dictionary_t;

typedef struct _cairo_pdf_reference
{
	int id;
	int generation;
} cairo_pdf_reference_t;

typedef struct _cairo_pdf_action
{
	cairo_pdf_action_type_t type;
	union
	{
		struct
		{
			const char *uri;
			int is_map;
		} uri;
	};
} cairo_pdf_action_t;

typedef struct _cairo_pdf_annotation
{
	cairo_pdf_annotation_type_t type;
	cairo_rectangle_t rect;
	union
	{
		struct
		{
			const char *dest;
			cairo_pdf_reference_t action;
			const char *uri;
		} link;
	};
} cairo_pdf_annotation_t;

typedef struct _cairo_pdf_datetime
{
	int year, month, day;
	int hour, minute, second;
	int utc_minute_offset;
} cairo_pdf_datetime_t;

typedef struct _cairo_pdf_dest
{
	cairo_pdf_dest_type_t type;
	int page;
	double top, left, bottom, right;
	double zoom;
} cairo_pdf_dest_t;

typedef struct _cairo_pdf_string
{
	char *_buffer;

	const char *data;
	int length;
} cairo_pdf_string_t;

typedef struct _cairo_pdf_outline_entry
{
	cairo_pdf_string_t title;
	cairo_pdf_dest_t destination;
	int depth;
	int count;
	int closed;

	int parent;

	int next, prev;
	int first, last;
} cairo_pdf_outline_entry_t;

typedef int cairo_pdf_boolean_t;
typedef int cairo_pdf_integer_t;
typedef double cairo_pdf_real_t;
typedef const char * cairo_pdf_name_t;

typedef struct _cairo_pdf_value
{
	cairo_pdf_value_type_t type;
	int id;
	union
	{
		cairo_pdf_boolean_t boolean;
		cairo_pdf_integer_t integer;
		cairo_pdf_real_t real;
		cairo_pdf_name_t name;
		cairo_pdf_string_t string;
		cairo_pdf_array_t array;
		cairo_pdf_dictionary_t dictionary;

		cairo_pdf_action_t action;
		cairo_pdf_annotation_t annotation;
		cairo_pdf_datetime_t date;
		cairo_pdf_dest_t dest;
		cairo_pdf_reference_t reference;
	};
} cairo_pdf_value_t;

#endif /* CAIRO_PDF_EXT_OBJECT_H */

