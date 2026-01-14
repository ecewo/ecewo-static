#include "ecewo-static.h"
#include "ecewo-fs.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

const char *get_mime_type(const char *path) {
  if (!path)
    return "application/octet-stream";

  const char *ext = strrchr(path, '.');
  if (!ext)
    return "application/octet-stream";

  // Convert to lowercase for comparison
  char ext_lower[32];
  size_t i;
  for (i = 0; i < sizeof(ext_lower) - 1 && ext[i]; i++) {
    ext_lower[i] = tolower(ext[i]);
  }
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

typedef struct static_mount_s {
  char *mount_path;
  char *dir_path;
  size_t mount_len;
  Static options;
  struct static_mount_s *next;
} static_mount_t;

typedef struct {
  static_mount_t *mounts;
  int mount_count;

  // Statistics
  uint64_t total_requests;
  uint64_t cache_hits;
  uint64_t not_found;
  uint64_t forbidden;
  uint64_t total_bytes_served;

  uv_mutex_t mutex;
  bool initialized;
  bool owns_fs; // Track if we initialized fs module
} static_module_state_t;

static static_module_state_t static_state = { 0 };

// ============================================================================
// Module Initialization
// ============================================================================

int static_init(void) {
  if (static_state.initialized)
    return 0;

  if (fs_init() != 0) {
    fprintf(stderr, "[ecewo-static] Failed to initialize fs module\n");
    return -1;
  }
  static_state.owns_fs = true;

  if (uv_mutex_init(&static_state.mutex) != 0) {
    fprintf(stderr, "[ecewo-static] Failed to initialize mutex\n");
    return -1;
  }

  static_state.initialized = true;
  return 0;
}

void static_cleanup(void) {
  if (!static_state.initialized)
    return;

  uv_mutex_lock(&static_state.mutex);

  static_mount_t *mount = static_state.mounts;
  while (mount) {
    static_mount_t *next = mount->next;
    free(mount->mount_path);
    free(mount->dir_path);
    free(mount);
    mount = next;
  }

  static_state.mounts = NULL;
  static_state.mount_count = 0;
  static_state.initialized = false;

  bool owns_fs = static_state.owns_fs;
  static_state.owns_fs = false;

  uv_mutex_unlock(&static_state.mutex);
  uv_mutex_destroy(&static_state.mutex);

  if (owns_fs)
    fs_cleanup();
}

void static_get_stats(static_stats_t *stats) {
  if (!stats || !static_state.initialized)
    return;

  uv_mutex_lock(&static_state.mutex);
  stats->mounted_paths = static_state.mount_count;
  stats->total_requests = static_state.total_requests;
  stats->cache_hits = static_state.cache_hits;
  stats->not_found = static_state.not_found;
  stats->forbidden = static_state.forbidden;
  stats->total_bytes_served = static_state.total_bytes_served;
  uv_mutex_unlock(&static_state.mutex);
}

void static_reset_stats(void) {
  if (!static_state.initialized)
    return;

  uv_mutex_lock(&static_state.mutex);
  static_state.total_requests = 0;
  static_state.cache_hits = 0;
  static_state.not_found = 0;
  static_state.forbidden = 0;
  static_state.total_bytes_served = 0;
  uv_mutex_unlock(&static_state.mutex);
}

SendFile send_file_default_options(void) {
  SendFile opts = {
    .max_age = 0, // No caching by default
    .last_modified = true, // Send Last-Modified
    .cache_control = true, // Send Cache-Control (if max_age > 0)
    // .accept_ranges = true, // Support ranges (future feature)
    .immutable = false, // Not immutable
    .content_type = NULL, // Auto-detect MIME
    .root = NULL, // No root (use absolute paths)
    .dotfiles = "ignore" // Deny dotfiles
  };
  return opts;
}

Static static_default_options(void) {
  Static opts = {
    .index = "index.html",
    .extensions = NULL,
    .extensions_count = 0,
    .etag = true, // Generate ETags
    .max_age = 0, // No caching by default
    .dotfiles = false,
    .redirect = true, // Redirect /path to /path/
    .immutable = false
  };
  return opts;
}

static bool is_safe_path(const char *path) {
  if (!path || *path == '\0')
    return false;

  if (strstr(path, "..") != NULL)
    return false;

  if (strstr(path, "//") != NULL)
    return false;

  // Null bytes
  for (const char *p = path; *p; p++) {
    if (*p == '\0')
      return false;
  }

  // Windows drive letters (C:, D:, etc.)
  if (path[0] && path[1] == ':')
    return false;

  // Absolute paths on Unix
  if (path[0] == '/' && path[1] == '/')
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

static char *generate_etag(Arena *arena, const uv_stat_t *stat) {
  if (!arena || !stat)
    return NULL;

  // ETag format: "size-mtime"
  return arena_sprintf(arena, "\"%lld-%ld\"",
                       (long long)stat->st_size,
                       (long)stat->st_mtim.tv_sec);
}

static bool check_etag_match(const char *if_none_match, const char *etag) {
  if (!if_none_match || !etag)
    return false;

  return strcmp(if_none_match, etag) == 0;
}

typedef struct {
  Req *req;
  Res *res;
  Static options;
  char *filepath;
  char *mime_type;
  char *etag;
  bool check_etag;
} static_file_ctx_t;

static void record_request(void) {
  uv_mutex_lock(&static_state.mutex);
  static_state.total_requests++;
  uv_mutex_unlock(&static_state.mutex);
}

static void record_cache_hit(void) {
  uv_mutex_lock(&static_state.mutex);
  static_state.cache_hits++;
  uv_mutex_unlock(&static_state.mutex);
}

static void record_not_found(void) {
  uv_mutex_lock(&static_state.mutex);
  static_state.not_found++;
  uv_mutex_unlock(&static_state.mutex);
}

static void record_forbidden(void) {
  uv_mutex_lock(&static_state.mutex);
  static_state.forbidden++;
  uv_mutex_unlock(&static_state.mutex);
}

static void record_bytes_served(size_t bytes) {
  uv_mutex_lock(&static_state.mutex);
  static_state.total_bytes_served += bytes;
  uv_mutex_unlock(&static_state.mutex);
}

static bool should_deny_dotfile(bool allow_dotfiles, const char *filepath) {
  if (!is_dotfile(filepath))
    return false;

  return !allow_dotfiles; // Deny unless explicitly allowed
}

// For send_file_internal (serve_static)
static void on_file_stat(const char *error, const uv_stat_t *stat, void *user_data);
static void on_file_read(const char *error, const char *data, size_t size, void *user_data);

// For send_file()
static void send_file_on_stat(const char *error, const uv_stat_t *stat, void *user_data);
static void send_file_on_read(const char *error, const char *data, size_t size, void *user_data);

static void send_file_internal(Req *req, Res *res, const char *filepath, const Static *options, bool check_etag) {
  if (!res || !filepath) {
    if (res)
      send_text(res, 500, "Internal server error");
    return;
  }

  if (!is_safe_path(filepath)) {
    send_text(res, 403, "Forbidden: Invalid path");
    record_forbidden();
    return;
  }

  Static opts = options ? *options : static_default_options();

  // Check dotfile access (using new dotfiles string field)
  if (should_deny_dotfile(opts.dotfiles, filepath)) {
    send_text(res, 403, "Forbidden: Dotfile access denied");
    record_forbidden();
    return;
  }

  static_file_ctx_t *ctx = arena_alloc(res->arena, sizeof(static_file_ctx_t));
  if (!ctx) {
    send_text(res, 500, "Memory allocation failed");
    return;
  }

  ctx->req = req;
  ctx->res = res;
  ctx->options = opts;
  ctx->filepath = arena_strdup(res->arena, filepath);
  ctx->mime_type = arena_strdup(res->arena, get_mime_type(filepath));
  ctx->check_etag = check_etag;
  ctx->etag = NULL;

  if (!ctx->filepath || !ctx->mime_type) {
    send_text(res, 500, "Memory allocation failed");
    return;
  }

  record_request();

  int result = fs_stat(filepath, on_file_stat, ctx);
  if (result != 0)
    send_text(res, 503, "Service temporarily unavailable");
}

static void on_file_stat(const char *error, const uv_stat_t *stat, void *user_data) {
  static_file_ctx_t *ctx = (static_file_ctx_t *)user_data;
  Res *res = ctx->res;

  if (error) {
    if (strstr(error, "ENOENT")) {
      send_text(res, 404, "File not found");
      record_not_found();
    } else if (strstr(error, "EACCES")) {
      send_text(res, 403, "Permission denied");
      record_forbidden();
    } else {
      send_text(res, 500, "Internal server error");
    }
    return;
  }

  if (ctx->options.etag) {
    ctx->etag = generate_etag(res->arena, stat);

    if (ctx->check_etag && ctx->etag) {
      const char *if_none_match = get_header(ctx->req, "If-None-Match");
      if (if_none_match && check_etag_match(if_none_match, ctx->etag)) {
        set_header(res, "ETag", ctx->etag);

        if (ctx->options.max_age > 0) {
          char *cache_control = arena_sprintf(res->arena,
                                              ctx->options.immutable
                                                  ? "public, max-age=%d, immutable"
                                                  : "public, max-age=%d",
                                              ctx->options.max_age);
          if (cache_control) {
            set_header(res, "Cache-Control", cache_control);
          }
        }

        reply(res, 304, NULL, 0);
        record_cache_hit();
        return;
      }
    }
  }

  // File exists and not cached - read it
  int result = fs_read_file(ctx->filepath, res->arena, on_file_read, ctx);
  if (result != 0)
    send_text(res, 503, "Service temporarily unavailable");
}

static void on_file_read(const char *error, const char *data, size_t size, void *user_data) {
  static_file_ctx_t *ctx = (static_file_ctx_t *)user_data;
  Res *res = ctx->res;

  if (error) {
    if (strstr(error, "ENOENT")) {
      send_text(res, 404, "File not found");
      record_not_found();
    } else if (strstr(error, "EACCES")) {
      send_text(res, 403, "Permission denied");
      record_forbidden();
    } else {
      send_text(res, 500, "Internal server error");
    }
    return;
  }

  // Success - send file with all headers

  set_header(res, "Content-Type", ctx->mime_type);

  if (ctx->options.etag && ctx->etag)
    set_header(res, "ETag", ctx->etag);

  if (ctx->options.max_age > 0) {
    char *cache_control = arena_sprintf(res->arena,
                                        ctx->options.immutable
                                            ? "public, max-age=%d, immutable"
                                            : "public, max-age=%d",
                                        ctx->options.max_age);
    if (cache_control)
      set_header(res, "Cache-Control", cache_control);
  }

  // Data is in res->arena
  record_bytes_served(size);
  reply(res, 200, data, size);
}

typedef struct {
  Res *res;
  SendFile options;
  char *resolved_path;
  char *mime_type;
  time_t mtime;
} send_file_ctx;

static void send_file_on_stat(const char *error, const uv_stat_t *stat, void *user_data) {
  send_file_ctx *ctx = (send_file_ctx *)user_data;
  Res *res = ctx->res;

  if (error) {
    if (strstr(error, "ENOENT") || strstr(error, "no such file")) {
      send_text(res, 404, "File not found");
      record_not_found();
    } else if (strstr(error, "EACCES") || strstr(error, "permission denied")) {
      send_text(res, 403, "Permission denied");
      record_forbidden();
    } else {
      send_text(res, 500, "Internal server error");
    }
    return;
  }

  // Store mtime for Last-Modified header
  ctx->mtime = stat->st_mtim.tv_sec;

  int result = fs_read_file(ctx->resolved_path, res->arena, send_file_on_read, ctx);
  if (result != 0)
    send_text(res, 503, "Service temporarily unavailable");
}

static void send_file_on_read(const char *error, const char *data, size_t size, void *user_data) {
  send_file_ctx *ctx = (send_file_ctx *)user_data;
  Res *res = ctx->res;

  if (error) {
    if (strstr(error, "ENOENT") || strstr(error, "no such file")) {
      send_text(res, 404, "File not found");
      record_not_found();
    } else if (strstr(error, "EACCES") || strstr(error, "permission denied")) {
      send_text(res, 403, "Permission denied");
      record_forbidden();
    } else {
      send_text(res, 500, "Internal server error");
    }
    return;
  }

  set_header(res, "Content-Type", ctx->mime_type);

  if (ctx->options.last_modified) {
    char date_buf[128];
    struct tm tm;
#ifdef _WIN32
    gmtime_s(&tm, &ctx->mtime);
#else
    gmtime_r(&ctx->mtime, &tm);
#endif
    strftime(date_buf, sizeof(date_buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    set_header(res, "Last-Modified", date_buf);
  }

  if (ctx->options.cache_control && ctx->options.max_age > 0) {
    char *cache_control = arena_sprintf(res->arena,
                                        ctx->options.immutable
                                            ? "public, max-age=%d, immutable"
                                            : "public, max-age=%d",
                                        ctx->options.max_age);
    if (cache_control) {
      set_header(res, "Cache-Control", cache_control);
    }
  }

  // Data is in res->arena
  record_bytes_served(size);
  reply(res, 200, data, size);
}

void send_file(Res *res, const char *filepath, const SendFile *options) {
  if (!res || !filepath) {
    if (res)
      send_text(res, 500, "Invalid arguments");
    return;
  }

  // Apply defaults
  SendFile opts = options ? *options : send_file_default_options();

  // Build resolved path (handle root directory)
  char resolved_path[2048];
  if (opts.root && filepath[0] != '/') {
    // Relative path with root
    snprintf(resolved_path, sizeof(resolved_path), "%s/%s", opts.root, filepath);
  } else {
    // Absolute path or no root
    strncpy(resolved_path, filepath, sizeof(resolved_path) - 1);
    resolved_path[sizeof(resolved_path) - 1] = '\0';
  }

  if (!is_safe_path(resolved_path)) {
    send_text(res, 403, "Forbidden: Invalid path");
    record_forbidden();
    return;
  }

  if (should_deny_dotfile(opts.dotfiles, resolved_path)) {
    send_text(res, 403, "Forbidden: Dotfile access denied");
    record_forbidden();
    return;
  }

  record_request();

  send_file_ctx *ctx = arena_alloc(res->arena, sizeof(send_file_ctx));
  if (!ctx) {
    send_text(res, 500, "Memory allocation failed");
    return;
  }

  ctx->res = res;
  ctx->options = opts;
  ctx->resolved_path = arena_strdup(res->arena, resolved_path);

  if (opts.content_type) {
    ctx->mime_type = arena_strdup(res->arena, opts.content_type);
  } else {
    ctx->mime_type = arena_strdup(res->arena, get_mime_type(resolved_path));
  }

  if (!ctx->resolved_path || !ctx->mime_type) {
    send_text(res, 500, "Memory allocation failed");
    return;
  }

  // Stat file first to get mtime for Last-Modified
  int result = fs_stat(resolved_path, send_file_on_stat, ctx);
  if (result != 0)
    send_text(res, 503, "Service temporarily unavailable");
}

static void static_handler(Req *req, Res *res) {
  const char *url_path = req->path;

  uv_mutex_lock(&static_state.mutex);

  static_mount_t *mount = static_state.mounts;
  static_mount_t *matched_mount = NULL;

  while (mount) {
    if (strncmp(url_path, mount->mount_path, mount->mount_len) == 0) {
      matched_mount = mount;
      break;
    }
    mount = mount->next;
  }

  if (!matched_mount) {
    uv_mutex_unlock(&static_state.mutex);
    send_text(res, 404, "Not found");
    record_not_found();
    return;
  }

  char dir_path[1024];
  char mount_path[256];
  Static opts = matched_mount->options;
  size_t mount_len = matched_mount->mount_len;

  strncpy(dir_path, matched_mount->dir_path, sizeof(dir_path) - 1);
  dir_path[sizeof(dir_path) - 1] = '\0';

  strncpy(mount_path, matched_mount->mount_path, sizeof(mount_path) - 1);
  mount_path[sizeof(mount_path) - 1] = '\0';

  uv_mutex_unlock(&static_state.mutex);

  const char *rel_path = url_path + mount_len;
  if (*rel_path == '/')
    rel_path++;

  if (should_deny_dotfile(opts.dotfiles, rel_path)) {
    send_text(res, 403, "Forbidden: Dotfile access denied");
    record_forbidden();
    return;
  }

  char filepath[2048];

  bool is_dir = (*rel_path == '\0' || rel_path[strlen(rel_path) - 1] == '/');

  if (is_dir) {
    if (*rel_path == '\0') {
      snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, opts.index);
    } else {
      snprintf(filepath, sizeof(filepath), "%s/%s%s", dir_path, rel_path, opts.index);
    }
  } else {
    snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, rel_path);
  }

  if (!is_safe_path(filepath)) {
    send_text(res, 403, "Forbidden: Invalid path");
    record_forbidden();
    return;
  }

  send_file_internal(req, res, filepath, &opts, true);
}

int serve_static(const char *mount_path, const char *dir_path, const Static *options) {
  if (!mount_path || !dir_path) {
    fprintf(stderr, "[ecewo-static] Invalid arguments\n");
    return -1;
  }

  if (!static_state.initialized) {
    fprintf(stderr, "[ecewo-static] Module not initialized - call static_init() first\n");
    return -1;
  }

  Static opts = options ? *options : static_default_options();

  if (!opts.index || *opts.index == '\0') {
    opts.index = "index.html";
  }

  uv_mutex_lock(&static_state.mutex);

  static_mount_t *existing = static_state.mounts;
  while (existing) {
    if (strcmp(existing->mount_path, mount_path) == 0) {
      uv_mutex_unlock(&static_state.mutex);
      fprintf(stderr, "[ecewo-static] Mount path '%s' already exists\n", mount_path);
      return -1;
    }
    existing = existing->next;
  }

  static_mount_t *mount = calloc(1, sizeof(static_mount_t));
  if (!mount) {
    uv_mutex_unlock(&static_state.mutex);
    return -1;
  }

  mount->mount_path = strdup(mount_path);
  mount->dir_path = strdup(dir_path);
  mount->mount_len = strlen(mount_path);
  mount->options = opts;

  if (!mount->mount_path || !mount->dir_path) {
    free(mount->mount_path);
    free(mount->dir_path);
    free(mount);
    uv_mutex_unlock(&static_state.mutex);
    return -1;
  }

  mount->next = static_state.mounts;
  static_state.mounts = mount;
  static_state.mount_count++;

  uv_mutex_unlock(&static_state.mutex);

  get(mount_path, static_handler);

  char wildcard[512];
  if (mount_path[strlen(mount_path) - 1] == '/') {
    snprintf(wildcard, sizeof(wildcard), "%s*", mount_path);
  } else {
    snprintf(wildcard, sizeof(wildcard), "%s/*", mount_path);
  }
  get(wildcard, static_handler);

  return 0;
}
