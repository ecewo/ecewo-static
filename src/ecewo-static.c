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

#include "ecewo-static.h"
#include "ecewo-fs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

const char *ecewo_static_mime_type(const char *path) {
  if (!path)
    return "application/octet-stream";

  const char *ext = strrchr(path, '.');
  if (!ext)
    return "application/octet-stream";

  char ext_lower[32];
  size_t i;
  for (i = 0; i < sizeof(ext_lower) - 1 && ext[i]; i++)
    ext_lower[i] = (char)tolower((unsigned char)ext[i]);
  ext_lower[i] = '\0';

  // HTML/CSS/JS
  if (strcmp(ext_lower, ".html") == 0 || strcmp(ext_lower, ".htm") == 0)
    return "text/html; charset=utf-8";
  if (strcmp(ext_lower, ".css") == 0)
    return "text/css; charset=utf-8";
  if (strcmp(ext_lower, ".js") == 0 || strcmp(ext_lower, ".mjs") == 0)
    return "application/javascript; charset=utf-8";
  if (strcmp(ext_lower, ".json") == 0)
    return "application/json; charset=utf-8";
  if (strcmp(ext_lower, ".xml") == 0)
    return "application/xml; charset=utf-8";

  // Images
  if (strcmp(ext_lower, ".png") == 0)
    return "image/png";
  if (strcmp(ext_lower, ".jpg") == 0 || strcmp(ext_lower, ".jpeg") == 0)
    return "image/jpeg";
  if (strcmp(ext_lower, ".gif") == 0)
    return "image/gif";
  if (strcmp(ext_lower, ".svg") == 0)
    return "image/svg+xml";
  if (strcmp(ext_lower, ".ico") == 0)
    return "image/x-icon";
  if (strcmp(ext_lower, ".webp") == 0)
    return "image/webp";
  if (strcmp(ext_lower, ".bmp") == 0)
    return "image/bmp";
  if (strcmp(ext_lower, ".tiff") == 0 || strcmp(ext_lower, ".tif") == 0)
    return "image/tiff";

  // Fonts
  if (strcmp(ext_lower, ".woff") == 0)
    return "font/woff";
  if (strcmp(ext_lower, ".woff2") == 0)
    return "font/woff2";
  if (strcmp(ext_lower, ".ttf") == 0)
    return "font/ttf";
  if (strcmp(ext_lower, ".otf") == 0)
    return "font/otf";
  if (strcmp(ext_lower, ".eot") == 0)
    return "application/vnd.ms-fontobject";

  // Documents
  if (strcmp(ext_lower, ".pdf") == 0)
    return "application/pdf";
  if (strcmp(ext_lower, ".txt") == 0)
    return "text/plain; charset=utf-8";
  if (strcmp(ext_lower, ".md") == 0)
    return "text/markdown; charset=utf-8";
  if (strcmp(ext_lower, ".csv") == 0)
    return "text/csv; charset=utf-8";

  // Media
  if (strcmp(ext_lower, ".mp4") == 0)
    return "video/mp4";
  if (strcmp(ext_lower, ".webm") == 0)
    return "video/webm";
  if (strcmp(ext_lower, ".ogg") == 0)
    return "video/ogg";
  if (strcmp(ext_lower, ".mp3") == 0)
    return "audio/mpeg";
  if (strcmp(ext_lower, ".wav") == 0)
    return "audio/wav";
  if (strcmp(ext_lower, ".m4a") == 0)
    return "audio/mp4";

  // Archives
  if (strcmp(ext_lower, ".zip") == 0)
    return "application/zip";
  if (strcmp(ext_lower, ".tar") == 0)
    return "application/x-tar";
  if (strcmp(ext_lower, ".gz") == 0)
    return "application/gzip";
  if (strcmp(ext_lower, ".7z") == 0)
    return "application/x-7z-compressed";

  // WebAssembly
  if (strcmp(ext_lower, ".wasm") == 0)
    return "application/wasm";

  return "application/octet-stream";
}

typedef struct {
  const char *index;
  bool etag;
  int max_age;
  bool dotfiles;
  bool redirect;
  bool immutable;
  char **extensions; // borrowed pointers into app arena
  int extensions_count;
} static_cfg_t;

typedef struct {
  int max_age;
  bool last_modified;
  bool cache_control;
  bool immutable;
  bool etag;
  const char *content_type; // borrowed from ctx-owned strdup
  const char *root;
  bool dotfiles;
} send_cfg_t;

struct ecewo_static_options_s {
  char *index;
  char **extensions;
  int extensions_count;
  bool etag;
  int max_age;
  bool dotfiles;
  bool redirect;
  bool immutable;
};

struct ecewo_send_file_options_s {
  int max_age;
  bool last_modified;
  bool cache_control;
  bool immutable;
  bool etag;
  char *content_type;
  char *root;
  bool dotfiles;
};

ecewo_static_options_t *ecewo_static_options_new(void) {
  ecewo_static_options_t *opts = calloc(1, sizeof(*opts));
  if (!opts)
    return NULL;

  opts->index = strdup("index.html");
  if (!opts->index) {
    free(opts);
    return NULL;
  }
  opts->etag = true;
  opts->redirect = true;
  // max_age=0, dotfiles=false, immutable=false, extensions=NULL implicit via calloc
  return opts;
}

static void free_extensions(char **exts, int count) {
  if (!exts)
    return;
  for (int i = 0; i < count; i++)
    free(exts[i]);
  free(exts);
}

void ecewo_static_options_free(ecewo_static_options_t *opts) {
  if (!opts)
    return;
  free(opts->index);
  free_extensions(opts->extensions, opts->extensions_count);
  free(opts);
}

void ecewo_static_options_set_index(ecewo_static_options_t *opts, const char *index) {
  if (!opts)
    return;
  char *copy = NULL;
  if (index) {
    copy = strdup(index);
    if (!copy)
      return;
  }
  free(opts->index);
  opts->index = copy;
}

void ecewo_static_options_set_extensions(ecewo_static_options_t *opts, const char *const *exts, int count) {
  if (!opts)
    return;
  if (count < 0)
    count = 0;

  char **copy = NULL;
  if (count > 0 && exts) {
    copy = calloc((size_t)count, sizeof(char *));
    if (!copy)
      return;
    for (int i = 0; i < count; i++) {
      if (!exts[i]) {
        free_extensions(copy, i);
        return;
      }
      copy[i] = strdup(exts[i]);
      if (!copy[i]) {
        free_extensions(copy, i);
        return;
      }
    }
  }
  free_extensions(opts->extensions, opts->extensions_count);
  opts->extensions = copy;
  opts->extensions_count = (copy ? count : 0);
}

void ecewo_static_options_set_etag(ecewo_static_options_t *opts, bool etag) {
  if (opts)
    opts->etag = etag;
}
void ecewo_static_options_set_max_age(ecewo_static_options_t *opts, int seconds) {
  if (opts)
    opts->max_age = seconds;
}
void ecewo_static_options_set_dotfiles(ecewo_static_options_t *opts, bool allow) {
  if (opts)
    opts->dotfiles = allow;
}
void ecewo_static_options_set_redirect(ecewo_static_options_t *opts, bool redirect) {
  if (opts)
    opts->redirect = redirect;
}
void ecewo_static_options_set_immutable(ecewo_static_options_t *opts, bool immutable) {
  if (opts)
    opts->immutable = immutable;
}

ecewo_send_file_options_t *ecewo_send_file_options_new(void) {
  ecewo_send_file_options_t *opts = calloc(1, sizeof(*opts));
  if (!opts)
    return NULL;
  opts->last_modified = true;
  opts->cache_control = true;
  opts->etag = true;
  // everything else zero/NULL/false via calloc
  return opts;
}

void ecewo_send_file_options_free(ecewo_send_file_options_t *opts) {
  if (!opts)
    return;
  free(opts->content_type);
  free(opts->root);
  free(opts);
}

void ecewo_send_file_options_set_max_age(ecewo_send_file_options_t *opts, int seconds) {
  if (opts)
    opts->max_age = seconds;
}
void ecewo_send_file_options_set_last_modified(ecewo_send_file_options_t *opts, bool enabled) {
  if (opts)
    opts->last_modified = enabled;
}
void ecewo_send_file_options_set_cache_control(ecewo_send_file_options_t *opts, bool enabled) {
  if (opts)
    opts->cache_control = enabled;
}
void ecewo_send_file_options_set_immutable(ecewo_send_file_options_t *opts, bool immutable) {
  if (opts)
    opts->immutable = immutable;
}
void ecewo_send_file_options_set_etag(ecewo_send_file_options_t *opts, bool etag) {
  if (opts)
    opts->etag = etag;
}
void ecewo_send_file_options_set_content_type(ecewo_send_file_options_t *opts, const char *content_type) {
  if (!opts)
    return;
  char *copy = NULL;
  if (content_type) {
    copy = strdup(content_type);
    if (!copy)
      return;
  }
  free(opts->content_type);
  opts->content_type = copy;
}
void ecewo_send_file_options_set_root(ecewo_send_file_options_t *opts, const char *root) {
  if (!opts)
    return;
  char *copy = NULL;
  if (root) {
    copy = strdup(root);
    if (!copy)
      return;
  }
  free(opts->root);
  opts->root = copy;
}
void ecewo_send_file_options_set_dotfiles(ecewo_send_file_options_t *opts, bool allow) {
  if (opts)
    opts->dotfiles = allow;
}

// ============================================================================
// Per-app state
// ============================================================================
//
// All mutable state lives in app_state_t, allocated from the app arena and
// indexed via ecewo_set/get_app_data keyed by &app_state_key. The struct
// outlives every request and is freed automatically on app shutdown.
//
// All access happens on the event loop thread (handlers, fs callbacks, and
// ecewo_serve_static() called pre-ecewo_run() from main), so no locking.

typedef struct static_mount_s {
  char *mount_path; // arena-allocated
  char *dir_path; // arena-allocated
  size_t mount_len;
  static_cfg_t cfg;
  struct static_mount_s *next;
} static_mount_t;

typedef struct {
  static_mount_t *mounts;
  int mount_count;
} app_state_t;

static int app_state_key;

static bool g_initialized = false;
static bool g_owns_fs = false;

static app_state_t *get_app_state(ecewo_app_t *app) {
  if (!app)
    return NULL;
  return (app_state_t *)ecewo_get_app_data(app, &app_state_key);
}

static app_state_t *get_or_create_app_state(ecewo_app_t *app) {
  app_state_t *state = get_app_state(app);
  if (state)
    return state;

  ecewo_arena_t *arena = ecewo_app_arena(app);
  if (!arena)
    return NULL;

  state = ecewo_alloc(arena, sizeof(*state));
  if (!state)
    return NULL;

  memset(state, 0, sizeof(*state));
  ecewo_set_app_data(app, &app_state_key, state);
  return state;
}

// ============================================================================
// Lifecycle
// ============================================================================

int ecewo_static_init(void) {
  if (g_initialized)
    return 0;

  if (fs_init() != 0) {
    fprintf(stderr, "[ecewo-static] Failed to initialize fs module\n");
    return -1;
  }
  g_owns_fs = true;
  g_initialized = true;
  return 0;
}

void ecewo_static_cleanup(void) {
  if (!g_initialized)
    return;

  bool owns_fs = g_owns_fs;
  g_owns_fs = false;
  g_initialized = false;

  if (owns_fs)
    fs_cleanup();
}

// ============================================================================
// Helpers
// ============================================================================

static bool is_safe_path(const char *path) {
  if (!path || *path == '\0')
    return false;
  if (strstr(path, "..") != NULL)
    return false;
  if (strstr(path, "//") != NULL)
    return false;
  if (path[1] == ':') // Windows drive letter (C:...)
    return false;
  return true;
}

static bool is_dotfile(const char *path) {
  if (!path)
    return false;
  const char *last_slash = strrchr(path, '/');
  const char *filename = last_slash ? last_slash + 1 : path;
  return filename[0] == '.';
}

static bool should_deny_dotfile(bool allow_dotfiles, const char *filepath) {
  if (!is_dotfile(filepath))
    return false;
  return !allow_dotfiles;
}

static char *generate_etag(ecewo_arena_t *arena, const fs_stat_t *stat) {
  if (!arena || !stat)
    return NULL;
  return ecewo_sprintf(arena, "\"%llu-%lld\"",
                       (unsigned long long)fs_stat_size(stat),
                       (long long)fs_stat_mtime_sec(stat));
}

static bool check_etag_match(const char *if_none_match, const char *etag) {
  if (!if_none_match || !etag)
    return false;
  return strcmp(if_none_match, etag) == 0;
}

// S_IFDIR bit check, portable without sys/stat.h
#define IS_DIR_MODE(m) (((m) & 0170000U) == 0040000U)

typedef struct {
  ecewo_request_t *req;
  ecewo_response_t *res;
  static_cfg_t cfg;
  char *filepath;
  char *path_stem; // original filepath before any extension suffix is appended
  int ext_index; // next extension index to try on ENOENT
  char *mime_type;
  char *etag;
} static_file_ctx_t;

static void on_file_stat(const char *error, const fs_stat_t *stat, void *user_data);
static void on_file_read(const char *error, const char *data, size_t size, void *user_data);

static void send_file_internal(ecewo_request_t *req,
                               ecewo_response_t *res,
                               const char *filepath,
                               const static_cfg_t *cfg) {
  if (!res || !filepath) {
    if (res)
      ecewo_send_text(res, 500, "Internal server error");
    return;
  }

  if (!is_safe_path(filepath)) {
    ecewo_send_text(res, 403, "Forbidden: Invalid path");
    return;
  }

  if (should_deny_dotfile(cfg->dotfiles, filepath)) {
    ecewo_send_text(res, 403, "Forbidden: Dotfile access denied");
    return;
  }

  ecewo_arena_t *arena = ecewo_res_arena(res);
  static_file_ctx_t *ctx = ecewo_alloc(arena, sizeof(*ctx));
  if (!ctx) {
    ecewo_send_text(res, 500, "Memory allocation failed");
    return;
  }

  ctx->req = req;
  ctx->res = res;
  ctx->cfg = *cfg;
  ctx->filepath = ecewo_strdup(arena, filepath);
  ctx->path_stem = ctx->filepath; // extension retrying always starts from here
  ctx->ext_index = 0;
  ctx->mime_type = ecewo_strdup(arena, ecewo_static_mime_type(filepath));
  ctx->etag = NULL;

  if (!ctx->filepath || !ctx->mime_type) {
    ecewo_send_text(res, 500, "Memory allocation failed");
    return;
  }

  int result = fs_stat(filepath, on_file_stat, ctx);
  if (result != 0)
    ecewo_send_text(res, 503, "Service temporarily unavailable");
}

static void on_file_stat(const char *error, const fs_stat_t *stat, void *user_data) {
  static_file_ctx_t *ctx = user_data;
  ecewo_response_t *res = ctx->res;
  ecewo_arena_t *arena = ecewo_res_arena(res);

  if (error) {
    if (strstr(error, "ENOENT")) {
      // Try next extension fallback before giving up
      if (ctx->ext_index < ctx->cfg.extensions_count) {
        const char *ext = ctx->cfg.extensions[ctx->ext_index++];
        const char *dot = (ext[0] == '.') ? "" : ".";
        char *new_path = ecewo_sprintf(arena, "%s%s%s", ctx->path_stem, dot, ext);
        if (new_path) {
          ctx->filepath = new_path;
          ctx->mime_type = ecewo_strdup(arena, ecewo_static_mime_type(new_path));
          ctx->etag = NULL;
          if (fs_stat(new_path, on_file_stat, ctx) == 0)
            return;
        }
      }
      ecewo_send_text(res, 404, "File not found");
    } else if (strstr(error, "EACCES")) {
      ecewo_send_text(res, 403, "Permission denied");
    } else {
      ecewo_send_text(res, 500, "Internal server error");
    }
    return;
  }

  // Directory handling
  if (IS_DIR_MODE(fs_stat_mode(stat))) {
    if (ctx->cfg.redirect) {
      const char *url_path = ecewo_req_path(ctx->req);
      char *location = ecewo_sprintf(arena, "%s/", url_path);
      if (location)
        ecewo_header_set(res, "Location", location);
      ecewo_send(res, 301, NULL, 0);
    } else {
      // Serve the index file directly; skip ETag since we lack the index's stat
      char *index_path = ecewo_sprintf(arena, "%s/%s", ctx->filepath, ctx->cfg.index);
      if (!index_path) {
        ecewo_send_text(res, 500, "Memory allocation failed");
        return;
      }
      ctx->filepath = index_path;
      ctx->mime_type = ecewo_strdup(arena, ecewo_static_mime_type(index_path));
      ctx->cfg.etag = false;
      if (fs_read_file(ctx->filepath, arena, on_file_read, ctx) != 0)
        ecewo_send_text(res, 503, "Service temporarily unavailable");
    }
    return;
  }

  // Regular file - ETag check
  if (ctx->cfg.etag) {
    ctx->etag = generate_etag(arena, stat);

    if (ctx->etag) {
      const char *if_none_match = ecewo_header_get(ctx->req, "If-None-Match");
      if (if_none_match && check_etag_match(if_none_match, ctx->etag)) {
        ecewo_header_set(res, "ETag", ctx->etag);

        if (ctx->cfg.max_age > 0) {
          char *cache_control = ecewo_sprintf(arena,
                                              ctx->cfg.immutable
                                                  ? "public, max-age=%d, immutable"
                                                  : "public, max-age=%d",
                                              ctx->cfg.max_age);
          if (cache_control)
            ecewo_header_set(res, "Cache-Control", cache_control);
        }

        ecewo_send(res, 304, NULL, 0);
        return;
      }
    }
  }

  int result = fs_read_file(ctx->filepath, arena, on_file_read, ctx);
  if (result != 0)
    ecewo_send_text(res, 503, "Service temporarily unavailable");
}

static void on_file_read(const char *error, const char *data, size_t size, void *user_data) {
  static_file_ctx_t *ctx = user_data;
  ecewo_response_t *res = ctx->res;

  if (error) {
    if (strstr(error, "ENOENT")) {
      ecewo_send_text(res, 404, "File not found");
    } else if (strstr(error, "EACCES")) {
      ecewo_send_text(res, 403, "Permission denied");
    } else {
      ecewo_send_text(res, 500, "Internal server error");
    }
    return;
  }

  ecewo_arena_t *arena = ecewo_res_arena(res);

  ecewo_header_set(res, "Content-Type", ctx->mime_type);

  if (ctx->cfg.etag && ctx->etag)
    ecewo_header_set(res, "ETag", ctx->etag);

  if (ctx->cfg.max_age > 0) {
    char *cache_control = ecewo_sprintf(arena,
                                        ctx->cfg.immutable
                                            ? "public, max-age=%d, immutable"
                                            : "public, max-age=%d",
                                        ctx->cfg.max_age);
    if (cache_control)
      ecewo_header_set(res, "Cache-Control", cache_control);
  }

  ecewo_send(res, 200, data, size);
}

typedef struct {
  ecewo_request_t *req;
  ecewo_response_t *res;
  send_cfg_t cfg;
  char *resolved_path;
  char *mime_type;
  int64_t mtime;
  char *etag_value;
} send_file_ctx;

static void send_file_on_read(const char *error, const char *data, size_t size, void *user_data);

static void send_file_on_stat(const char *error, const fs_stat_t *stat, void *user_data) {
  send_file_ctx *ctx = user_data;
  ecewo_response_t *res = ctx->res;

  if (error) {
    if (strstr(error, "ENOENT") || strstr(error, "no such file"))
      ecewo_send_text(res, 404, "File not found");
    else if (strstr(error, "EACCES") || strstr(error, "permission denied"))
      ecewo_send_text(res, 403, "Permission denied");
    else
      ecewo_send_text(res, 500, "Internal server error");
    return;
  }

  ctx->mtime = fs_stat_mtime_sec(stat);
  ecewo_arena_t *arena = ecewo_res_arena(res);

  if (ctx->cfg.etag) {
    ctx->etag_value = ecewo_sprintf(arena, "\"%llu-%lld\"",
                                    (unsigned long long)fs_stat_size(stat),
                                    (long long)ctx->mtime);

    if (ctx->etag_value && ctx->req) {
      const char *if_none_match = ecewo_header_get(ctx->req, "If-None-Match");
      if (if_none_match && strcmp(if_none_match, ctx->etag_value) == 0) {
        ecewo_header_set(res, "ETag", ctx->etag_value);
        if (ctx->cfg.cache_control && ctx->cfg.max_age > 0) {
          char *cc = ecewo_sprintf(arena,
                                   ctx->cfg.immutable
                                       ? "public, max-age=%d, immutable"
                                       : "public, max-age=%d",
                                   ctx->cfg.max_age);
          if (cc)
            ecewo_header_set(res, "Cache-Control", cc);
        }
        ecewo_send(res, 304, NULL, 0);
        return;
      }
    }
  }

  int result = fs_read_file(ctx->resolved_path, arena, send_file_on_read, ctx);
  if (result != 0)
    ecewo_send_text(res, 503, "Service temporarily unavailable");
}

static void send_file_on_read(const char *error, const char *data, size_t size, void *user_data) {
  send_file_ctx *ctx = user_data;
  ecewo_response_t *res = ctx->res;

  if (error) {
    if (strstr(error, "ENOENT") || strstr(error, "no such file"))
      ecewo_send_text(res, 404, "File not found");
    else if (strstr(error, "EACCES") || strstr(error, "permission denied"))
      ecewo_send_text(res, 403, "Permission denied");
    else
      ecewo_send_text(res, 500, "Internal server error");
    return;
  }

  ecewo_arena_t *arena = ecewo_res_arena(res);

  ecewo_header_set(res, "Content-Type", ctx->mime_type);

  if (ctx->cfg.etag && ctx->etag_value)
    ecewo_header_set(res, "ETag", ctx->etag_value);

  if (ctx->cfg.last_modified) {
    char date_buf[128];
    struct tm tm;
    time_t mtime = (time_t)ctx->mtime;
#ifdef _WIN32
    gmtime_s(&tm, &mtime);
#else
    gmtime_r(&mtime, &tm);
#endif
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    ecewo_header_set(res, "Last-Modified", date_buf);
  }

  if (ctx->cfg.cache_control && ctx->cfg.max_age > 0) {
    char *cache_control = ecewo_sprintf(arena,
                                        ctx->cfg.immutable
                                            ? "public, max-age=%d, immutable"
                                            : "public, max-age=%d",
                                        ctx->cfg.max_age);
    if (cache_control)
      ecewo_header_set(res, "Cache-Control", cache_control);
  }

  ecewo_send(res, 200, data, size);
}

void ecewo_send_file(ecewo_request_t *req,
                     ecewo_response_t *res,
                     const char *filepath,
                     const ecewo_send_file_options_t *options) {
  if (!g_initialized) {
    if (res)
      ecewo_send_text(res, 503, "Static module not initialized");
    return;
  }

  if (!res || !filepath) {
    if (res)
      ecewo_send_text(res, 500, "Invalid arguments");
    return;
  }

  // Snapshot caller-provided opts into a local POD; defaults if NULL.
  send_cfg_t cfg = {
    .max_age = 0,
    .last_modified = true,
    .cache_control = true,
    .immutable = false,
    .etag = true,
    .content_type = NULL,
    .root = NULL,
    .dotfiles = false
  };
  if (options) {
    cfg.max_age = options->max_age;
    cfg.last_modified = options->last_modified;
    cfg.cache_control = options->cache_control;
    cfg.immutable = options->immutable;
    cfg.etag = options->etag;
    cfg.content_type = options->content_type;
    cfg.root = options->root;
    cfg.dotfiles = options->dotfiles;
  }

  char resolved_path[2048];
  if (cfg.root && filepath[0] != '/') {
    snprintf(resolved_path, sizeof(resolved_path), "%s/%s", cfg.root, filepath);
  } else {
    strncpy(resolved_path, filepath, sizeof(resolved_path) - 1);
    resolved_path[sizeof(resolved_path) - 1] = '\0';
  }

  if (!is_safe_path(resolved_path)) {
    ecewo_send_text(res, 403, "Forbidden: Invalid path");
    return;
  }

  if (should_deny_dotfile(cfg.dotfiles, resolved_path)) {
    ecewo_send_text(res, 403, "Forbidden: Dotfile access denied");
    return;
  }

  ecewo_arena_t *arena = ecewo_res_arena(res);
  send_file_ctx *ctx = ecewo_alloc(arena, sizeof(*ctx));
  if (!ctx) {
    ecewo_send_text(res, 500, "Memory allocation failed");
    return;
  }

  ctx->req = req;
  ctx->res = res;
  ctx->cfg = cfg;
  ctx->etag_value = NULL;
  ctx->resolved_path = ecewo_strdup(arena, resolved_path);

  if (cfg.content_type)
    ctx->mime_type = ecewo_strdup(arena, cfg.content_type);
  else
    ctx->mime_type = ecewo_strdup(arena, ecewo_static_mime_type(resolved_path));

  if (!ctx->resolved_path || !ctx->mime_type) {
    ecewo_send_text(res, 500, "Memory allocation failed");
    return;
  }

  int result = fs_stat(ctx->resolved_path, send_file_on_stat, ctx);
  if (result != 0)
    ecewo_send_text(res, 503, "Service temporarily unavailable");
}

static void static_handler(ecewo_request_t *req, ecewo_response_t *res) {
  ecewo_app_t *app = ecewo_req_app(req);
  app_state_t *state = get_app_state(app);

  if (!state) {
    ecewo_send_text(res, 404, "Not found");
    return;
  }

  const char *url_path = ecewo_req_path(req);
  static_mount_t *matched = NULL;
  size_t best_len = 0;

  for (static_mount_t *m = state->mounts; m; m = m->next) {
    if (strncmp(url_path, m->mount_path, m->mount_len) != 0)
      continue;
    if (m->mount_len > best_len) {
      matched = m;
      best_len = m->mount_len;
    }
  }

  if (!matched) {
    ecewo_send_text(res, 404, "Not found");
    return;
  }

  static_cfg_t cfg = matched->cfg;
  size_t mount_len = matched->mount_len;
  const char *dir_path = matched->dir_path;

  const char *rel_path = url_path + mount_len;
  if (*rel_path == '/')
    rel_path++;

  if (should_deny_dotfile(cfg.dotfiles, rel_path)) {
    ecewo_send_text(res, 403, "Forbidden: Dotfile access denied");
    return;
  }

  char filepath[2048];

  // URL already targets a directory (trailing slash or root)
  // Serve index directly without redirect or extension fallback.
  bool is_dir = (*rel_path == '\0' || rel_path[strlen(rel_path) - 1] == '/');

  if (is_dir) {
    if (*rel_path == '\0')
      snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, cfg.index);
    else
      snprintf(filepath, sizeof(filepath), "%s/%s%s", dir_path, rel_path, cfg.index);

    if (!is_safe_path(filepath)) {
      ecewo_send_text(res, 403, "Forbidden: Invalid path");
      return;
    }

    // Strip redirect and extensions. The index path is already fully resolved.
    static_cfg_t dir_cfg = cfg;
    dir_cfg.redirect = false;
    dir_cfg.extensions = NULL;
    dir_cfg.extensions_count = 0;
    send_file_internal(req, res, filepath, &dir_cfg);
  } else {
    // URL targets a file or possibly an un-slashed directory.
    // Pass the raw path; on_file_stat handles redirect if it turns out to be a dir.
    snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, rel_path);

    if (!is_safe_path(filepath)) {
      ecewo_send_text(res, 403, "Forbidden: Invalid path");
      return;
    }

    send_file_internal(req, res, filepath, &cfg);
  }
}

int ecewo_serve_static(ecewo_app_t *app,
                       const char *mount_path,
                       const char *dir_path,
                       const ecewo_static_options_t *options) {
  if (!app || !mount_path || !dir_path) {
    fprintf(stderr, "[ecewo-static] Invalid arguments\n");
    return -1;
  }

  if (!g_initialized) {
    fprintf(stderr, "[ecewo-static] Module not initialized - call ecewo_static_init() first\n");
    return -1;
  }

  app_state_t *state = get_or_create_app_state(app);
  if (!state) {
    fprintf(stderr, "[ecewo-static] Failed to allocate per-app state\n");
    return -1;
  }

  // Snapshot user-provided opts into a POD config; defaults if NULL.
  static_cfg_t cfg;
  const char *index_src;
  if (options) {
    cfg.etag = options->etag;
    cfg.max_age = options->max_age;
    cfg.dotfiles = options->dotfiles;
    cfg.redirect = options->redirect;
    cfg.immutable = options->immutable;
    index_src = (options->index && *options->index) ? options->index : "index.html";
  } else {
    cfg.etag = true;
    cfg.max_age = 0;
    cfg.dotfiles = false;
    cfg.redirect = true;
    cfg.immutable = false;
    index_src = "index.html";
  }
  cfg.extensions = NULL;
  cfg.extensions_count = 0;

  for (static_mount_t *e = state->mounts; e; e = e->next) {
    if (strcmp(e->mount_path, mount_path) == 0) {
      fprintf(stderr, "[ecewo-static] Mount path '%s' already exists\n", mount_path);
      return -1;
    }
  }

  ecewo_arena_t *arena = ecewo_app_arena(app);

  // Arena-copy extensions so cfg.extensions outlives the options object.
  if (options && options->extensions && options->extensions_count > 0) {
    int count = options->extensions_count;
    char **ext_arr = ecewo_alloc(arena, (size_t)count * sizeof(char *));
    if (!ext_arr)
      return -1;
    for (int i = 0; i < count; i++) {
      ext_arr[i] = ecewo_strdup(arena, options->extensions[i]);
      if (!ext_arr[i])
        return -1;
    }
    cfg.extensions = ext_arr;
    cfg.extensions_count = count;
  }

  static_mount_t *mount = ecewo_alloc(arena, sizeof(*mount));
  if (!mount)
    return -1;

  memset(mount, 0, sizeof(*mount));
  mount->mount_path = ecewo_strdup(arena, mount_path);
  mount->dir_path = ecewo_strdup(arena, dir_path);
  mount->mount_len = strlen(mount_path);
  mount->cfg = cfg;
  mount->cfg.index = ecewo_strdup(arena, index_src);

  if (!mount->mount_path || !mount->dir_path || !mount->cfg.index)
    return -1;

  mount->next = state->mounts;
  state->mounts = mount;
  state->mount_count++;

  ECEWO_GET(app, mount_path, static_handler);

  char wildcard[512];
  if (mount_path[strlen(mount_path) - 1] == '/')
    snprintf(wildcard, sizeof(wildcard), "%s*", mount_path);
  else
    snprintf(wildcard, sizeof(wildcard), "%s/*", mount_path);
  ECEWO_GET(app, wildcard, static_handler);

  return 0;
}
