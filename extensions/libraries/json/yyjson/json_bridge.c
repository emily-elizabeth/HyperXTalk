/*
 * json_bridge.c
 *
 * Thin C bridge between yyjson and the LCB foreign-handler callback interface.
 *
 * Parses a UTF-8 JSON string with yyjson, then walks the value tree depth-first
 * and fires events into a caller-supplied callback.  The LCB side receives one
 * call per JSON token and builds the HyperXTalk value structure.
 *
 * Event codes (pEvent parameter):
 *   JSON_EVENT_NULL         1   pValue is NULL
 *   JSON_EVENT_BOOL         2   pValue is "true" or "false"
 *   JSON_EVENT_NUMBER       3   pValue is the number as a UTF-8 string
 *   JSON_EVENT_STRING       4   pValue is the string value (UTF-8)
 *   JSON_EVENT_OBJECT_START 5   pValue is NULL
 *   JSON_EVENT_OBJECT_KEY   6   pValue is the key (UTF-8)
 *   JSON_EVENT_OBJECT_END   7   pValue is NULL
 *   JSON_EVENT_ARRAY_START  8   pValue is NULL
 *   JSON_EVENT_ARRAY_END    9   pValue is NULL
 *   JSON_EVENT_ERROR        10  pValue is a human-readable error message
 */

#include "yyjson.h"
#include <stdio.h>
#include <string.h>

#define JSON_EVENT_NULL         1
#define JSON_EVENT_BOOL         2
#define JSON_EVENT_NUMBER       3
#define JSON_EVENT_STRING       4
#define JSON_EVENT_OBJECT_START 5
#define JSON_EVENT_OBJECT_KEY   6
#define JSON_EVENT_OBJECT_END   7
#define JSON_EVENT_ARRAY_START  8
#define JSON_EVENT_ARRAY_END    9
#define JSON_EVENT_ERROR        10

typedef void (*json_event_fn)(int pEvent, const char *pValue);

/* Ensure json_parse is exported even when built with -fvisibility=hidden */
#if defined(_WIN32)
#   define JSON_EXPORT __declspec(dllexport)
#else
#   define JSON_EXPORT __attribute__((visibility("default")))
#endif

/* Forward declaration */
static void walk_value(yyjson_val *val, json_event_fn cb);

static void walk_value(yyjson_val *val, json_event_fn cb)
{
    yyjson_type type = yyjson_get_type(val);

    switch (type)
    {
        case YYJSON_TYPE_NULL:
            cb(JSON_EVENT_NULL, NULL);
            break;

        case YYJSON_TYPE_BOOL:
            cb(JSON_EVENT_BOOL, yyjson_get_bool(val) ? "true" : "false");
            break;

        case YYJSON_TYPE_NUM:
        {
            /* yyjson gives us the raw number string — use it directly to
               preserve precision without printf rounding. */
            const char *raw = yyjson_get_raw(val);
            if (raw)
                cb(JSON_EVENT_NUMBER, raw);
            else
            {
                /* Fallback: format as double */
                char buf[64];
                snprintf(buf, sizeof(buf), "%.17g", yyjson_get_real(val));
                cb(JSON_EVENT_NUMBER, buf);
            }
            break;
        }

        case YYJSON_TYPE_STR:
            cb(JSON_EVENT_STRING, yyjson_get_str(val));
            break;

        case YYJSON_TYPE_OBJ:
        {
            yyjson_obj_iter iter;
            yyjson_val *key;
            cb(JSON_EVENT_OBJECT_START, NULL);
            yyjson_obj_iter_init(val, &iter);
            while ((key = yyjson_obj_iter_next(&iter)) != NULL)
            {
                yyjson_val *v = yyjson_obj_iter_get_val(key);
                cb(JSON_EVENT_OBJECT_KEY, yyjson_get_str(key));
                walk_value(v, cb);
            }
            cb(JSON_EVENT_OBJECT_END, NULL);
            break;
        }

        case YYJSON_TYPE_ARR:
        {
            yyjson_arr_iter iter;
            yyjson_val *elem;
            cb(JSON_EVENT_ARRAY_START, NULL);
            yyjson_arr_iter_init(val, &iter);
            while ((elem = yyjson_arr_iter_next(&iter)) != NULL)
                walk_value(elem, cb);
            cb(JSON_EVENT_ARRAY_END, NULL);
            break;
        }

        default:
            cb(JSON_EVENT_ERROR, "unexpected yyjson value type");
            break;
    }
}

/*
 * json_parse
 *
 * Entry point called from LCB via a foreign handler.
 *
 * pJson    - NUL-terminated UTF-8 JSON string
 * pCallback - LCB handler cast to json_event_fn
 */
JSON_EXPORT void json_parse(const char *pJson, json_event_fn pCallback)
{
    yyjson_read_err err;
    yyjson_doc *doc;
    yyjson_val *root;
    char msg[256];

    if (!pJson || !pCallback)
    {
        if (pCallback)
            pCallback(JSON_EVENT_ERROR, "null input");
        return;
    }

    doc = yyjson_read_opts(
        (char *)pJson,
        strlen(pJson),
        YYJSON_READ_NOFLAG,
        NULL,
        &err
    );

    if (!doc)
    {
        snprintf(msg, sizeof(msg), "JSON parse error at position %zu: %s",
                 err.pos, err.msg ? err.msg : "unknown error");
        pCallback(JSON_EVENT_ERROR, msg);
        return;
    }

    root = yyjson_doc_get_root(doc);
    if (root)
        walk_value(root, pCallback);
    else
        pCallback(JSON_EVENT_NULL, NULL);

    yyjson_doc_free(doc);
}
