#ifndef ECEWO_STATIC_H
#define ECEWO_STATIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecewo.h"
#include <stdbool.h>

typedef struct {
  int max_age; // Default: 0 (no caching)
  bool last_modified; // Default: true
  bool cache_control; // Default: true (if max_age > 0)
  // bool accept_ranges; // Future feature
  bool immutable; // Default: false (use for: Versioned assets (app.v123.js))
  const char *content_type; // Default: NULL
  const char *root; // Default: NULL
  bool dotfiles; // Default: false (deny)
} SendFile;

typedef struct {
  const char *index; // Default: "index.html"
  const char **extensions; // Default: NULL (no extension fallback)
  int extensions_count;
  bool etag; // Default: true
  int max_age; // Cache duration, default: 0 (no caching)
  bool dotfiles; // Default: false (deny)
  bool redirect; // Default: true
  bool immutable; // Default: false
} Static;

// Returns: 0 on success, -1 on failure
int static_init(void);
void static_cleanup(void);

// Returns: 0 on success, -1 on failure (e.g., mount already exists)
int serve_static(
    const char *mount_path, // URL prefix (e.g., "/" or "/assets" or "/static")
    const char *dir_path, // Filesystem directory path (e.g., "./public" or "/var/www")
    const Static *options); // Configuration options (NULL = use defaults)

void send_file(
    Res *res,
    const char *filepath,
    const SendFile *options);

const char *get_mime_type(const char *path);

typedef struct {
  int mounted_paths; // Number of mounted static directories
  uint64_t total_requests; // Total static file requests
  uint64_t cache_hits; // 304 Not Modified responses
  uint64_t not_found; // 404 responses
  uint64_t forbidden; // 403 responses
  uint64_t total_bytes_served; // Total bytes served
} static_stats_t;

void static_get_stats(static_stats_t *stats);
void static_reset_stats(void);
SendFile send_file_default_options(void);
Static static_default_options(void);

#ifdef __cplusplus
}
#endif

#endif
