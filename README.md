# Static File Serving

ecewo-static is a static file serving plugin for [ecewo](https://github.com/ecewo/ecewo) with automatic MIME type detection, ETag-based caching, directory redirect, extension fallback, and security features. Built on top of [`ecewo-fs`](https://github.com/ecewo/ecewo-fs).

## Table of Contents

1. [Installation](#installation)
2. [Quick Start](#quick-start)
3. [Project Structure](#project-structure)
    1. [CMake Static Files Setup](#cmake-static-files-setup)
4. [API Reference](#api-reference)
    1. [`ecewo_serve_static()`](#ecewo_serve_static)
    2. [`ecewo_send_file()`](#ecewo_send_file)
    3. [`ecewo_static_mime_type()`](#ecewo_static_mime_type)
5. [Configuration Options](#configuration-options)
    1. [serve_static options](#serve_static-options)
    2. [send_file options](#send_file-options)
6. [Features](#features)
    1. [Automatic MIME Type Detection](#automatic-mime-type-detection)
    2. [ETag Caching](#etag-caching)
    3. [Cache Control](#cache-control)
    4. [Security Features](#security-features)
    5. [Index Files](#index-files)
    6. [Directory Redirect](#directory-redirect)
    7. [Extension Fallback](#extension-fallback)
7. [Advanced Examples](#advanced-examples)
8. [Limitations](#limitations)

## Installation

Add to your `CMakeLists.txt`:

```cmake
ecewo_add(
    fs@v0.2.0
    static@v0.2.0
)

target_link_libraries(app PRIVATE
    ecewo::ecewo
    ecewo::fs
    ecewo::static
)
```

Initialize the static module in your application:

```c
#include "ecewo.h"
#include "ecewo-static.h"

int main(void) {
    ecewo_app_t *app = ecewo_create();

    // Initialize static file serving (also initializes ecewo-fs internally)
    if (ecewo_static_init() != 0) {
        fprintf(stderr, "Failed to initialize static module\n");
        return 1;
    }

    // Your routes and static mounts...

    ecewo_atexit(app, ecewo_static_cleanup);
    ecewo_listen(app, 3000);
    return 0;
}
```

## Quick Start

### Basic Static File Serving

```c
#include "ecewo.h"
#include "ecewo-static.h"

int main(void) {
    ecewo_app_t *app = ecewo_create();
    ecewo_static_init();

    // Serve ./public at the root URL
    if (ecewo_serve_static(app, "/", "./public", NULL) != 0) {
        fprintf(stderr, "Failed to mount static directory\n");
        return 1;
    }

    /*
     * GET /               -> ./public/index.html
     * GET /about.html     -> ./public/about.html
     * GET /css/style.css  -> ./public/css/style.css
     * GET /js/app.js      -> ./public/js/app.js
     * GET /img/logo.png   -> ./public/img/logo.png
     */

    ecewo_atexit(app, ecewo_static_cleanup);
    ecewo_listen(app, 3000);
    return 0;
}
```

### Single File Serving

```c
void download_handler(ecewo_request_t *req, ecewo_response_t *res) {
    const char *filename = ecewo_query(req, "file");

    if (!filename) {
        ecewo_send_text(res, 400, "Missing file parameter");
        return;
    }

    char *filepath = ecewo_sprintf(ecewo_req_arena(req), "downloads/%s", filename);
    ecewo_send_file(req, res, filepath, NULL);
}

int main(void) {
    ecewo_app_t *app = ecewo_create();
    ecewo_static_init();

    ECEWO_GET(app, "/download", download_handler);

    ecewo_atexit(app, ecewo_static_cleanup);
    ecewo_listen(app, 3000);
    return 0;
}
```

## Project Structure

### Example Directory Layout

```
your-project/
├── main.c
├── CMakeLists.txt
├── build/
│   ├── server          # Executable
│   └── public/         # Copied/symlinked by CMake
│       ├── index.html
│       ├── css/
│       │   └── style.css
│       ├── js/
│       │   └── app.js
│       └── images/
│           └── logo.png
└── public/             # Source static files
    ├── index.html
    ├── css/
    │   └── style.css
    ├── js/
    │   └── app.js
    └── images/
        └── logo.png
```

### CMake Static Files Setup

Add this to your `CMakeLists.txt` to automatically copy/symlink the `public/` directory:

```cmake
if(WIN32)
    add_custom_command(TARGET server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/public
            ${CMAKE_BINARY_DIR}/public
        COMMENT "Copying public directory to build folder"
    )
else()
    add_custom_command(TARGET server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/public
            ${CMAKE_BINARY_DIR}/public
        COMMENT "Creating symlink to public directory"
    )
endif()
```

- **Windows:** Copies `public/` → `build/public/` on every build
- **Linux/Mac:** Creates symlink `build/public/` → `../public/` (one-time)

## API Reference

### `ecewo_serve_static()`

Mount a directory to serve static files from a URL prefix.

```c
int ecewo_serve_static(
    ecewo_app_t *app,
    const char *mount_path,
    const char *dir_path,
    const ecewo_static_options_t *options
);
```

**Parameters:**

- `app`: The ecewo application instance
- `mount_path`: URL prefix (e.g., `"/"`, `"/static"`, `"/assets"`)
- `dir_path`: Filesystem directory to serve (e.g., `"./public"`, `"/var/www"`)
- `options`: Configuration options (`NULL` for defaults). Caller may free after the call.

**Returns:** `0` on success, `-1` on failure (mount already exists, allocation failure, etc.)

**URL mapping examples:**

```c
ecewo_serve_static(app, "/", "./public", NULL);
// GET /style.css       -> ./public/style.css
// GET /js/app.js       -> ./public/js/app.js
// GET /img/logo.png    -> ./public/img/logo.png

ecewo_serve_static(app, "/static", "./assets", NULL);
// GET /static/style.css    -> ./assets/style.css
// GET /static/logo.png     -> ./assets/logo.png

ecewo_serve_static(app, "/cdn", "./dist", NULL);
// GET /cdn/bundle.js       -> ./dist/bundle.js
```

**With options:**

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
ecewo_static_options_set_index(opts, "home.html");
ecewo_static_options_set_etag(opts, true);
ecewo_static_options_set_max_age(opts, 86400);

ecewo_serve_static(app, "/", "./public", opts);
ecewo_static_options_free(opts);
```

### `ecewo_send_file()`

Send a single file as the HTTP response. Equivalent to Express.js `res.sendFile()`.

```c
void ecewo_send_file(
    ecewo_request_t *req,
    ecewo_response_t *res,
    const char *filepath,
    const ecewo_send_file_options_t *options
);
```

**Parameters:**

- `req`: The request object (used for `If-None-Match` ETag validation)
- `res`: The response object
- `filepath`: File path, absolute or relative to `options` root
- `options`: Send options (`NULL` for defaults). Caller may free after the call.

**Automatically handles:**
- MIME type detection from file extension
- `Content-Type` header
- `ETag` header + `304 Not Modified` on match
- `Last-Modified` header
- `Cache-Control` header
- Dotfile access control
- `404` if file not found, `403` if permission denied

**Basic example:**

```c
void report_handler(ecewo_request_t *req, ecewo_response_t *res) {
    ecewo_send_file(req, res, "/data/report.pdf", NULL);
}
```

**With long-term caching:**

```c
void asset_handler(ecewo_request_t *req, ecewo_response_t *res) {
    ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
    ecewo_send_file_options_set_max_age(opts, 31536000);   // 1 year
    ecewo_send_file_options_set_immutable(opts, true);
    // Cache-Control: public, max-age=31536000, immutable

    ecewo_send_file(req, res, "/assets/app.v123.js", opts);
    ecewo_send_file_options_free(opts);
}
```

**With root directory:**

```c
void download_handler(ecewo_request_t *req, ecewo_response_t *res) {
    const char *filename = ecewo_param(req, "filename");

    ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
    ecewo_send_file_options_set_root(opts, "/var/downloads");

    // Serves /var/downloads/<filename>
    ecewo_send_file(req, res, filename, opts);
    ecewo_send_file_options_free(opts);
}
```

**Custom MIME type:**

```c
void binary_handler(ecewo_request_t *req, ecewo_response_t *res) {
    ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
    ecewo_send_file_options_set_content_type(opts, "application/octet-stream");

    ecewo_send_file(req, res, "/data/file.bin", opts);
    ecewo_send_file_options_free(opts);
}
```

### `ecewo_static_mime_type()`

Look up the MIME type for a file path or extension.

```c
const char *ecewo_static_mime_type(const char *path);
```

**Parameters:**

- `path`: File path or bare extension (e.g., `"app.js"` or `".js"`)

**Returns:** MIME type string; never `NULL`. Unknown extensions return `"application/octet-stream"`.

**Example:**

```c
ecewo_static_mime_type("app.js");      // "application/javascript; charset=utf-8"
ecewo_static_mime_type(".png");        // "image/png"
ecewo_static_mime_type("unknown.xyz"); // "application/octet-stream"
```

## Configuration Options

All option objects are opaque. Create with `_new()`, configure with setters, pass to the function, then free with `_free()`. The library copies everything it needs at the call site, so options can be freed immediately after.

### serve_static options

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
```

| Setter | Default | Description |
|--------|---------|-------------|
| `ecewo_static_options_set_index(opts, name)` | `"index.html"` | File served for directory requests |
| `ecewo_static_options_set_extensions(opts, exts, count)` | none | Extension fallbacks tried on 404 |
| `ecewo_static_options_set_etag(opts, bool)` | `true` | Generate `ETag` headers |
| `ecewo_static_options_set_max_age(opts, seconds)` | `0` | `Cache-Control` max-age (0 = no header) |
| `ecewo_static_options_set_dotfiles(opts, bool)` | `false` | Allow serving dotfiles |
| `ecewo_static_options_set_redirect(opts, bool)` | `true` | 301 redirect `/path` → `/path/` for directories |
| `ecewo_static_options_set_immutable(opts, bool)` | `false` | Add `immutable` to `Cache-Control` |

```c
void ecewo_static_options_free(ecewo_static_options_t *opts);
```

**Example:**

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
ecewo_static_options_set_max_age(opts, 86400);     // 1 day
ecewo_static_options_set_etag(opts, true);
ecewo_static_options_set_immutable(opts, false);

ecewo_serve_static(app, "/assets", "./dist", opts);
ecewo_static_options_free(opts);
```

### send_file options

```c
ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
```

| Setter | Default | Description |
|--------|---------|-------------|
| `ecewo_send_file_options_set_etag(opts, bool)` | `true` | Generate `ETag` + handle `If-None-Match` |
| `ecewo_send_file_options_set_max_age(opts, seconds)` | `0` | `Cache-Control` max-age |
| `ecewo_send_file_options_set_last_modified(opts, bool)` | `true` | Send `Last-Modified` header |
| `ecewo_send_file_options_set_cache_control(opts, bool)` | `true` | Send `Cache-Control` when max_age > 0 |
| `ecewo_send_file_options_set_immutable(opts, bool)` | `false` | Add `immutable` to `Cache-Control` |
| `ecewo_send_file_options_set_content_type(opts, str)` | `NULL` | Override MIME type (NULL = auto-detect) |
| `ecewo_send_file_options_set_root(opts, str)` | `NULL` | Base directory for relative paths |
| `ecewo_send_file_options_set_dotfiles(opts, bool)` | `false` | Allow serving dotfiles |

```c
void ecewo_send_file_options_free(ecewo_send_file_options_t *opts);
```

## Features

### Automatic MIME Type Detection

The correct `Content-Type` header is set automatically from the file extension:

| Extension | MIME Type | Category |
|-----------|-----------|----------|
| .html, .htm | text/html; charset=utf-8 | HTML |
| .css | text/css; charset=utf-8 | Stylesheets |
| .js, .mjs | application/javascript; charset=utf-8 | Scripts |
| .json | application/json; charset=utf-8 | Data |
| .xml | application/xml; charset=utf-8 | Data |
| .png | image/png | Images |
| .jpg, .jpeg | image/jpeg | Images |
| .gif | image/gif | Images |
| .svg | image/svg+xml | Images |
| .ico | image/x-icon | Images |
| .webp | image/webp | Images |
| .bmp | image/bmp | Images |
| .tiff, .tif | image/tiff | Images |
| .woff | font/woff | Fonts |
| .woff2 | font/woff2 | Fonts |
| .ttf | font/ttf | Fonts |
| .otf | font/otf | Fonts |
| .eot | application/vnd.ms-fontobject | Fonts |
| .pdf | application/pdf | Documents |
| .txt | text/plain; charset=utf-8 | Text |
| .md | text/markdown; charset=utf-8 | Text |
| .csv | text/csv; charset=utf-8 | Data |
| .mp4 | video/mp4 | Video |
| .webm | video/webm | Video |
| .ogg | video/ogg | Video |
| .mp3 | audio/mpeg | Audio |
| .wav | audio/wav | Audio |
| .m4a | audio/mp4 | Audio |
| .zip | application/zip | Archives |
| .tar | application/x-tar | Archives |
| .gz | application/gzip | Archives |
| .7z | application/x-7z-compressed | Archives |
| .wasm | application/wasm | WebAssembly |

Unknown extensions default to `application/octet-stream`.

### ETag Caching

ETags enable efficient cache validation. Both `ecewo_serve_static` and `ecewo_send_file` support full ETag round-trips.

**How it works:**

1. Server sends file with ETag:
   ```
   HTTP/1.1 200 OK
   ETag: "12345-1609459200"
   Content-Type: text/html
   ```

2. Browser caches the file and stores the ETag.

3. On the next request, the browser sends:
   ```
   GET /index.html HTTP/1.1
   If-None-Match: "12345-1609459200"
   ```

4. If the file is unchanged, the server responds with no body:
   ```
   HTTP/1.1 304 Not Modified
   ETag: "12345-1609459200"
   ```

**ETag format:** `"<size>-<mtime>"`

**Disable ETags:**

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
ecewo_static_options_set_etag(opts, false);
ecewo_serve_static(app, "/", "./public", opts);
ecewo_static_options_free(opts);
```

### Cache Control

**Basic caching:**

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
ecewo_static_options_set_max_age(opts, 3600);  // 1 hour
ecewo_serve_static(app, "/", "./public", opts);
ecewo_static_options_free(opts);
// Cache-Control: public, max-age=3600
```

**Long-term caching for versioned assets:**

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
ecewo_static_options_set_max_age(opts, 31536000);  // 1 year
ecewo_static_options_set_immutable(opts, true);
ecewo_serve_static(app, "/assets", "./dist", opts);
ecewo_static_options_free(opts);
// Cache-Control: public, max-age=31536000, immutable
```

**Best practices:**

- **HTML:** No caching; users always get the latest version
  ```c
  ecewo_static_options_set_max_age(opts, 0);  // default
  ```

- **Versioned assets** (e.g. `app.v123.js`): Long max-age + immutable
  ```c
  ecewo_static_options_set_max_age(opts, 31536000);
  ecewo_static_options_set_immutable(opts, true);
  ```

- **Regular assets:** Moderate max-age with ETag validation
  ```c
  ecewo_static_options_set_max_age(opts, 3600);
  ecewo_static_options_set_etag(opts, true);  // already the default
  ```

### Security Features

#### Path Traversal Protection

All paths are validated before any filesystem access:

```
GET /../../../etc/passwd   -> 403 Forbidden
GET /files/../secret.txt   -> 403 Forbidden
GET /path//double/slash    -> 403 Forbidden
```

#### Dotfile Protection

Dotfiles are blocked by default to prevent exposing sensitive files:

```
GET /.env           -> 403 Forbidden
GET /.gitignore     -> 403 Forbidden
GET /.htaccess      -> 403 Forbidden
GET /config/.env    -> 403 Forbidden
```

> [!WARNING]
>
> Never enable dotfiles unless you have a specific reason and understand the security implications.

### Index Files

Directory requests are automatically resolved to the configured index file:

```c
ecewo_serve_static(app, "/", "./public", NULL);

// GET /           -> ./public/index.html
// GET /docs/      -> ./public/docs/index.html
// GET /about/     -> ./public/about/index.html
```

**Custom index file:**

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
ecewo_static_options_set_index(opts, "home.html");
ecewo_serve_static(app, "/", "./public", opts);
ecewo_static_options_free(opts);

// GET /      -> ./public/home.html
// GET /docs/ -> ./public/docs/home.html
```

### Directory Redirect

When `redirect` is enabled (the default), a request for `/docs` automatically redirects to `/docs/` if `docs` is a directory:

```
GET /docs   -> 301 Location: /docs/
GET /docs/  -> ./public/docs/index.html
```

**Disable redirect:**

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
ecewo_static_options_set_redirect(opts, false);
ecewo_serve_static(app, "/", "./public", opts);
ecewo_static_options_free(opts);
// GET /docs -> ./public/docs/index.html (served directly, no redirect)
```

### Extension Fallback

When `extensions` is set, requests that don't match any file will retry with each extension in order before returning 404:

```c
ecewo_static_options_t *opts = ecewo_static_options_new();
const char *exts[] = { "html", "htm" };
ecewo_static_options_set_extensions(opts, exts, 2);
ecewo_serve_static(app, "/", "./public", opts);
ecewo_static_options_free(opts);

// GET /about       -> tries ./public/about, ./public/about.html, ./public/about.htm
// GET /about.html  -> ./public/about.html (exact match, no fallback needed)
```

Extensions may be given with or without a leading dot (`"html"` and `".html"` are both accepted).

## Advanced Examples

### Multiple Static Directories

```c
int main(void) {
    ecewo_app_t *app = ecewo_create();
    ecewo_static_init();

    // Main site; no caching (HTML changes frequently)
    if (ecewo_serve_static(app, "/", "./public", NULL) != 0) {
        fprintf(stderr, "Failed to mount main site\n");
        return 1;
    }

    // Versioned assets; long-term caching
    ecewo_static_options_t *asset_opts = ecewo_static_options_new();
    ecewo_static_options_set_max_age(asset_opts, 31536000);
    ecewo_static_options_set_immutable(asset_opts, true);
    if (ecewo_serve_static(app, "/assets", "./dist", asset_opts) != 0) {
        fprintf(stderr, "Failed to mount assets\n");
        return 1;
    }
    ecewo_static_options_free(asset_opts);

    // Docs; moderate caching
    ecewo_static_options_t *docs_opts = ecewo_static_options_new();
    ecewo_static_options_set_max_age(docs_opts, 3600);
    if (ecewo_serve_static(app, "/docs", "./documentation", docs_opts) != 0) {
        fprintf(stderr, "Failed to mount docs\n");
        return 1;
    }
    ecewo_static_options_free(docs_opts);

    ecewo_atexit(app, ecewo_static_cleanup);
    ecewo_listen(app, 3000);
    return 0;
}
```

### API + Static Files

> [!WARNING]
>
> Register API routes **before** static mounts to ensure proper routing priority.

```c
void api_users(ecewo_request_t *req, ecewo_response_t *res) {
    ecewo_send_json(res, 200, "{\"users\":[]}");
}

void api_products(ecewo_request_t *req, ecewo_response_t *res) {
    ecewo_send_json(res, 200, "{\"products\":[]}");
}

int main(void) {
    ecewo_app_t *app = ecewo_create();
    ecewo_static_init();

    // API routes first
    ECEWO_GET(app, "/api/users",    api_users);
    ECEWO_GET(app, "/api/products", api_products);

    // Static files last (fallback)
    if (ecewo_serve_static(app, "/", "./public", NULL) != 0) {
        fprintf(stderr, "Failed to mount static files\n");
        return 1;
    }

    ecewo_atexit(app, ecewo_static_cleanup);
    ecewo_listen(app, 3000);
    return 0;
}
```

### SPA (Single Page Application) Support

```c
void spa_fallback(ecewo_request_t *req, ecewo_response_t *res) {
    ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
    ecewo_send_file_options_set_max_age(opts, 0);
    ecewo_send_file_options_set_cache_control(opts, false);

    ecewo_send_file(req, res, "./public/index.html", opts);
    ecewo_send_file_options_free(opts);
}

int main(void) {
    ecewo_app_t *app = ecewo_create();
    ecewo_static_init();

    // API routes
    ECEWO_GET(app, "/api/*", api_handler);

    // Versioned static assets
    ecewo_static_options_t *asset_opts = ecewo_static_options_new();
    ecewo_static_options_set_max_age(asset_opts, 31536000);
    ecewo_static_options_set_immutable(asset_opts, true);
    ecewo_serve_static(app, "/static", "./public/static", asset_opts);
    ecewo_static_options_free(asset_opts);

    // SPA fallback; all other routes serve index.html
    ECEWO_GET(app, "/*", spa_fallback);

    ecewo_atexit(app, ecewo_static_cleanup);
    ecewo_listen(app, 3000);
    return 0;
}
```

### Protected File Download

```c
void protected_download(ecewo_request_t *req, ecewo_response_t *res) {
    const char *token = ecewo_header_get(req, "Authorization");
    if (!token || !validate_token(token)) {
        ecewo_send_text(res, 401, "Unauthorized");
        return;
    }

    const char *file = ecewo_param(req, "file");

    ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
    ecewo_send_file_options_set_root(opts, "/var/protected");
    ecewo_send_file_options_set_max_age(opts, 0);

    ecewo_send_file(req, res, file, opts);
    ecewo_send_file_options_free(opts);
}
```

### Custom 404 Page

```c
void custom_404(ecewo_request_t *req, ecewo_response_t *res) {
    ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
    ecewo_send_file_options_set_max_age(opts, 3600);

    ecewo_send_file(req, res, "./public/404.html", opts);
    ecewo_send_file_options_free(opts);
}

int main(void) {
    ecewo_app_t *app = ecewo_create();
    ecewo_static_init();

    if (ecewo_serve_static(app, "/", "./public", NULL) != 0) {
        fprintf(stderr, "Failed to mount static files\n");
        return 1;
    }

    ECEWO_GET(app, "/*", custom_404);  // must be last

    ecewo_atexit(app, ecewo_static_cleanup);
    ecewo_listen(app, 3000);
    return 0;
}
```

## Limitations

- Maximum file size: 100 MB (configurable via `ECEWO_FS_MAX_FILE_SIZE`)
- No built-in compression (use a reverse proxy such as nginx)
- No range request support
- No directory listing
