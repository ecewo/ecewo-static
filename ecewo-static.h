// Copyright 2025-2026 Savas Sahin <savashn@proton.me>

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef ECEWO_STATIC_H
#define ECEWO_STATIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecewo.h"
#include "ecewo-static-export.h"
#include <stdbool.h>

// ---------------------------------------------------------------------------
// LIFECYCLE
// ---------------------------------------------------------------------------

/** Initialize the static-file module. Internally initializes ecewo-fs.
 *  Must be called once per process before ecewo_serve_static() or ecewo_send_file().
 *  Returns 0 on success, -1 on failure. */
ECEWO_STATIC_EXPORT int ecewo_static_init(void);

/** Tear down the static-file module. Internally tears down ecewo-fs.
 *  Per-app state (mounts) lives in the app arena and is released automatically
 *  when the app shuts down; this only manages process-level state. */
ECEWO_STATIC_EXPORT void ecewo_static_cleanup(void);

// ---------------------------------------------------------------------------
// SERVE-STATIC OPTIONS
// ---------------------------------------------------------------------------

/** Opaque configuration for ecewo_serve_static().
 *  Create with ecewo_static_options_new(), populate via the setters, pass to
 *  ecewo_serve_static(), then free with ecewo_static_options_free(). The library
 *  copies whatever it needs at the call site, so options can be freed afterwards. */
typedef struct ecewo_static_options_s ecewo_static_options_t;

/** Allocate a new options object initialized to defaults:
 *    index="index.html", etag=true, max_age=0, dotfiles=false (deny),
 *    redirect=true, immutable=false, no extension fallbacks.
 *  Returns NULL on allocation failure. */
ECEWO_STATIC_EXPORT ecewo_static_options_t *ecewo_static_options_new(void);

/** Free an options object created by ecewo_static_options_new(). */
ECEWO_STATIC_EXPORT void ecewo_static_options_free(ecewo_static_options_t *opts);

/** Set the index file name served for directory requests (default "index.html"). */
ECEWO_STATIC_EXPORT void ecewo_static_options_set_index(ecewo_static_options_t *opts, const char *index);

/** Set the list of extension fallbacks tried when a file is not found.
 *  Extensions may be given with or without the leading dot ("html" or ".html").
 *  The library deep-copies the array; caller can free its memory after the call.
 *  Pass count=0 (and exts may be NULL) to clear. */
ECEWO_STATIC_EXPORT void ecewo_static_options_set_extensions(ecewo_static_options_t *opts, const char *const *exts, int count);

/** Enable/disable ETag generation (default: true). */
ECEWO_STATIC_EXPORT void ecewo_static_options_set_etag(ecewo_static_options_t *opts, bool etag);

/** Set Cache-Control max-age in seconds. 0 disables caching headers (default: 0). */
ECEWO_STATIC_EXPORT void ecewo_static_options_set_max_age(ecewo_static_options_t *opts, int seconds);

/** Allow serving dotfiles. Default false (deny). Enable with care; never expose .env etc. */
ECEWO_STATIC_EXPORT void ecewo_static_options_set_dotfiles(ecewo_static_options_t *opts, bool allow);

/** Redirect /path → /path/ when the path resolves to a directory (default: true). */
ECEWO_STATIC_EXPORT void ecewo_static_options_set_redirect(ecewo_static_options_t *opts, bool redirect);

/** Add the immutable directive to Cache-Control (only meaningful when max_age > 0; default: false). */
ECEWO_STATIC_EXPORT void ecewo_static_options_set_immutable(ecewo_static_options_t *opts, bool immutable);

// ---------------------------------------------------------------------------
// SEND-FILE OPTIONS
// ---------------------------------------------------------------------------

/** Opaque configuration for ecewo_send_file(). Same lifecycle pattern as
 *  ecewo_static_options_t. */
typedef struct ecewo_send_file_options_s ecewo_send_file_options_t;

/** Allocate a new send-file options object initialized to defaults:
 *    etag=true, max_age=0, last_modified=true, cache_control=true, immutable=false,
 *    content_type=NULL (auto-detect), root=NULL, dotfiles=false (deny).
 *  Returns NULL on allocation failure. */
ECEWO_STATIC_EXPORT ecewo_send_file_options_t *ecewo_send_file_options_new(void);

/** Free a send-file options object created by ecewo_send_file_options_new(). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_free(ecewo_send_file_options_t *opts);

/** Set Cache-Control max-age in seconds (default: 0). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_set_max_age(ecewo_send_file_options_t *opts, int seconds);

/** Send a Last-Modified header derived from the file mtime (default: true). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_set_last_modified(ecewo_send_file_options_t *opts, bool enabled);

/** Send a Cache-Control header when max_age > 0 (default: true). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_set_cache_control(ecewo_send_file_options_t *opts, bool enabled);

/** Add the immutable directive to Cache-Control (default: false). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_set_immutable(ecewo_send_file_options_t *opts, bool immutable);

/** Enable/disable ETag generation and If-None-Match validation (default: true). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_set_etag(ecewo_send_file_options_t *opts, bool etag);

/** Override Content-Type. NULL means auto-detect from extension (default: NULL). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_set_content_type(ecewo_send_file_options_t *opts, const char *content_type);

/** Base directory for relative filepaths. NULL means filepath is taken as-is (default: NULL). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_set_root(ecewo_send_file_options_t *opts, const char *root);

/** Allow serving dotfiles (default: false, deny). */
ECEWO_STATIC_EXPORT void ecewo_send_file_options_set_dotfiles(ecewo_send_file_options_t *opts, bool allow);

// ---------------------------------------------------------------------------
// CORE API
// ---------------------------------------------------------------------------

/** Mount dir_path on the given app under URL prefix mount_path.
 *  Registers two GET routes: mount_path itself and a wildcard child.
 *  options may be NULL for defaults; the library copies what it needs and
 *  the caller may free options immediately after the call.
 *  Returns 0 on success, -1 on failure (already mounted, allocation failure, etc.). */
ECEWO_STATIC_EXPORT int ecewo_serve_static(
    ecewo_app_t *app,
    const char *mount_path,
    const char *dir_path,
    const ecewo_static_options_t *options);

/** Send a single file as the response. options may be NULL for defaults.
 *  Caller may free options immediately after the call. */
ECEWO_STATIC_EXPORT void ecewo_send_file(
    ecewo_request_t *req,
    ecewo_response_t *res,
    const char *filepath,
    const ecewo_send_file_options_t *options);

/** Return the MIME type for a file extension. Never returns NULL.
 *  Unknown extensions yield "application/octet-stream". */
ECEWO_STATIC_EXPORT const char *ecewo_static_mime_type(const char *path);

#ifdef __cplusplus
}
#endif

#endif
