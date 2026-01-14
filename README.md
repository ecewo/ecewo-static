# Static File Serving

ecewo provides a comprehensive static file serving module with automatic MIME type detection, ETag-based caching, security features, and express.js-compatible APIs. Built on top of [`ecewo-fs`](https://github.com/ecewo/ecewo-fs).

## Table of Contents

1. [Installation](#installation)
2. [Quick Start](#quick-start)
3. [Project Structure](#project-structure)
    1. [CMake Static Files Setup](#cmake-static-files-setup)
4. [API Reference](#api-reference)
    1. [`serve_static()`](#serve_static)
    2. [`send_file()`](#send_file)
    3. [`get_mime_type()`](#get_mime_type)
5. [Configuration Options](#configuration-options)
    1. [Static](#Static)
    2. [SendFile](#SendFile)
6. [Features](#features)
    1. [Automatic MIME Type Detection](#automatic-mime-type-detection)
    2. [ETag Caching](#etag-caching)
    3. [Cache Control](#cache-control)
    4. [Security Features](#security-features)
    5. [Index Files](#index-files)
7. [Statistics & Monitoring](#statistics--monitoring)
8. [Advanced Examples](#advanced-examples)

## Installation

Add to your `CMakeLists.txt`:

```sh
ecewo_plugins(
    fs
    static
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
    server_init();
    
    // Initialize static file serving module
    // (automatically initializes ecewo-fs internally)
    if (static_init() != 0) {
        fprintf(stderr, "Failed to initialize static module\n");
        return 1;
    }
    
    // Your routes and static file serving...
    
    // Register cleanup handler (also cleans up fs module)
    server_atexit(static_cleanup);
    
    server_listen(3000);
    server_run();
    return 0;
}
```

## Quick Start

### Basic Static File Serving

```c
#include "ecewo.h"
#include "ecewo-static.h"

int main(void) {
    server_init();
    static_init();  // Automatically initializes ecewo-fs
    
    // Serve ./public directory at root URL
    if (serve_static("/", "./public", NULL) != 0) {
        fprintf(stderr, "Failed to mount static directory\n");
        return 1;
    }
    
    /*
     * Now these URLs work automatically:
     * GET /               -> ./public/index.html
     * GET /about.html     -> ./public/about.html
     * GET /css/style.css  -> ./public/css/style.css
     * GET /js/app.js      -> ./public/js/app.js
     * GET /img/logo.png   -> ./public/img/logo.png
     */
    
    server_atexit(static_cleanup);  // Also cleans up fs module
    
    server_listen(3000);
    server_run();
    return 0;
}
```

### Single File Serving

```c
void download_handler(Req *req, Res *res) {
    const char *filename = get_query(req, "file");
    
    if (!filename) {
        send_text(res, 400, "Missing file parameter");
        return;
    }
    
    char *filepath = arena_sprintf(req->arena, "downloads/%s", filename);
    
    // Send the file with default options
    send_file(res, filepath, NULL);
}

int main(void) {
    server_init();
    static_init();  // Automatically initializes ecewo-fs
    
    get("/download", download_handler);
    
    server_atexit(static_cleanup);
    server_listen(3000);
    server_run();
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
# Platform-aware public directory handling
if(WIN32)
    # Windows: Copy directory (symlinks require admin privileges)
    add_custom_command(TARGET server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/public
            ${CMAKE_BINARY_DIR}/public
        COMMENT "Copying public directory to build folder"
    )
else()
    # Linux/Mac: Create symlink (faster, no duplication)
    add_custom_command(TARGET server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/public
            ${CMAKE_BINARY_DIR}/public
        COMMENT "Creating symlink to public directory"
    )
endif()
```

**What this does:**

- **Windows:** Copies `public/` -> `build/public/` on every build
- **Linux/Mac:** Creates symlink `build/public/` -> `../public/` (one-time)

Server can be run from either project root or build directory.

## API Reference

### `serve_static()`

Mount a directory to serve static files from a URL path.

```c
int serve_static(
    const char *mount_path,
    const char *dir_path,
    const Static *options
);
```

**Parameters:**

- `mount_path`: URL prefix (e.g., `"/"`, `"/static"`, `"/assets"`)
- `dir_path`: Filesystem directory path (e.g., `"./public"`, `"/var/www"`)
- `options`: Configuration options (`NULL` for defaults)

**Returns:**
- `0` on success
- `-1` on failure (e.g., mount already exists, invalid arguments)

**URL Mapping Examples:**

```c
// Mount at root
serve_static("/", "./public", NULL);
// GET /style.css       -> ./public/style.css
// GET /js/app.js       -> ./public/js/app.js
// GET /img/logo.png    -> ./public/img/logo.png

// Mount at /static
serve_static("/static", "./assets", NULL);
// GET /static/style.css    -> ./assets/style.css
// GET /static/logo.png     -> ./assets/logo.png

// Mount at /cdn
serve_static("/cdn", "./dist", NULL);
// GET /cdn/bundle.js       -> ./dist/bundle.js
```

**Example with options:**

```c
Static opts = {
    .index = "home.html",       // Custom index file
    .extensions = NULL,         // No extension fallback
    .extensions_count = 0,
    .etag = true,               // Enable ETag caching
    .max_age = 86400,           // Cache for 1 day
    .dotfiles = false,          // Deny dotfiles (recommended)
    .redirect = true,           // Redirect /path to /path/
    .immutable = false          // Not immutable
};

serve_static("/", "./public", &opts);
```

### `send_file()`

Send a single file as HTTP response. Matches Express.js `res.sendFile()` API.

```c
void send_file(
    Res *res,
    const char *filepath,
    const SendFile *options
);
```

**Parameters:**

- `res`: Response object
- `filepath`: File path (absolute or relative to `options.root`)
- `options`: Send options (`NULL` for defaults)

**Automatically handles:**
- MIME type detection
- `Content-Type` header
- `Last-Modified` header
- `Cache-Control` header
- Dotfile access control
- 404 if file not found
- 403 if permission denied

**Basic example:**

```c
void report_handler(Req *req, Res *res) {
    send_file(res, "/data/report.pdf", NULL);
}
```

**With caching:**

```c
void asset_handler(Req *req, Res *res) {
    SendFile opts = {
        .max_age = 31536000,        // 1 year
        .immutable = true,          // Asset never changes
        .cache_control = true,
        .last_modified = true,
        .content_type = NULL,       // Auto-detect
        .root = NULL,
        .dotfiles = false
    };
    
    send_file(res, "/assets/app.v123.js", &opts);
    // Cache-Control: public, max-age=31536000, immutable
}
```

**With root directory:**

```c
void download_handler(Req *req, Res *res) {
    const char *filename = get_param(req, "filename");
    
    SendFile opts = {
        .root = "/var/downloads",   // Base directory
        .max_age = 0,               // No caching
        .dotfiles = false           // Block dotfiles
    };
    
    // Serves /var/downloads/report.pdf
    send_file(res, "report.pdf", &opts);
}
```

**Custom MIME type:**

```c
void binary_handler(Req *req, Res *res) {
    SendFile opts = {
        .content_type = "application/octet-stream",
        .dotfiles = false
    };
    
    send_file(res, "/data/file.bin", &opts);
}
```

### `get_mime_type()`

Get MIME type for a file extension.

```c
const char *get_mime_type(const char *path);
```

**Parameters:**

- `path`: File path or extension (e.g., `"file.js"` or `".js"`)

**Returns:** MIME type string (never `NULL`, returns `"application/octet-stream"` for unknown types)

**Example:**

```c
const char *mime = get_mime_type("app.js");
// Returns: "application/javascript; charset=utf-8"

mime = get_mime_type(".png");
// Returns: "image/png"

mime = get_mime_type("unknown.xyz");
// Returns: "application/octet-stream"
```

## Configuration Options

### Static

Options for `serve_static()` - matches Express.js `express.static()`:

```c
typedef struct {
    const char *index;          // Index file name
    const char **extensions;    // Extension fallbacks
    int extensions_count;       // Number of extensions
    bool etag;                  // Generate ETags
    int max_age;                // Cache duration (seconds)
    bool dotfiles;              // Allow dotfiles
    bool redirect;              // Redirect /path to /path/
    bool immutable;             // Add immutable directive
} Static;
```

**Field descriptions:**

- `index` (default: `"index.html"`): File to serve for directory requests
  ```c
  // GET / -> serves /index.html
  // GET /about/ -> serves /about/index.html
  ```

- `extensions` (default: `NULL`): File extensions to try
  ```c
  const char *exts[] = { "html", "htm" };
  Static opts = {
      .extensions = exts,
      .extensions_count = 2
  };
  // GET /about -> tries /about, /about.html, /about.htm
  ```

- `etag` (default: `true`): Generate ETag headers for cache validation
  ```
  ETag: "1234567-1609459200"
  ```

- `max_age` (default: `0`): Cache duration in seconds
  ```c
  .max_age = 86400  // 1 day
  // Cache-Control: public, max-age=86400
  ```

- `dotfiles` (default: `false`): Dotfile handling
  - `true`: Allow serving dotfiles normally
  - `false`: Deny serving dotfiles

- `redirect` (default: `true`): Redirect `/path` to `/path/` for directories

- `immutable` (default: `false`): Add `immutable` directive to Cache-Control
  ```c
  .immutable = true
  // Cache-Control: public, max-age=31536000, immutable
  ```

**Get default options:**

```c
Static opts = static_default_options();
// Modify specific fields
opts.max_age = 3600;
serve_static("/", "./public", &opts);
```

### SendFile

Options for `send_file()` - matches Express.js `res.sendFile()`:

```c
typedef struct {
    int max_age;                // Cache duration (seconds)
    bool last_modified;         // Send Last-Modified header
    bool cache_control;         // Send Cache-Control header
    bool accept_ranges;         // Support range requests (future)
    bool immutable;             // Add immutable directive
    const char *content_type;   // Override MIME type
    const char *root;           // Root directory
    bool dotfiles;              // Allow dotfiles
} SendFile;
```

**Field descriptions:**

- `max_age` (default: `0`): Cache duration in seconds
- `last_modified` (default: `true`): Include Last-Modified header
- `cache_control` (default: `true`): Include Cache-Control header if max_age > 0
- `accept_ranges` (default: `true`): Reserved for future range request support
- `immutable` (default: `false`): Add immutable directive
- `content_type` (default: `NULL`): Override MIME type (NULL = auto-detect)
- `root` (default: `NULL`): Base directory for relative paths
- `dotfiles` (default: `false`): Dotfile handling

**Get default options:**

```c
SendFile opts = send_file_default_options();
// Modify specific fields
opts.max_age = 86400;
opts.immutable = true;
send_file(res, "/assets/app.js", &opts);
```

## Features

### Automatic MIME Type Detection

ecewo automatically sets the correct `Content-Type` header based on file extension. **50+ file types supported:**

| Extension          | MIME Type                              | Category     |
|--------------------|----------------------------------------|--------------|
| .html, .htm        | text/html; charset=utf-8               | HTML         |
| .css               | text/css; charset=utf-8                | Stylesheets  |
| .js, .mjs          | application/javascript; charset=utf-8  | Scripts      |
| .json              | application/json; charset=utf-8        | Data         |
| .xml               | application/xml; charset=utf-8         | Data         |
| .png               | image/png                              | Images       |
| .jpg, .jpeg        | image/jpeg                             | Images       |
| .gif               | image/gif                              | Images       |
| .svg               | image/svg+xml                          | Images       |
| .ico               | image/x-icon                           | Images       |
| .webp              | image/webp                             | Images       |
| .bmp               | image/bmp                              | Images       |
| .tiff, .tif        | image/tiff                             | Images       |
| .woff              | font/woff                              | Fonts        |
| .woff2             | font/woff2                             | Fonts        |
| .ttf               | font/ttf                               | Fonts        |
| .otf               | font/otf                               | Fonts        |
| .eot               | application/vnd.ms-fontobject          | Fonts        |
| .pdf               | application/pdf                        | Documents    |
| .txt               | text/plain; charset=utf-8              | Text         |
| .md                | text/markdown; charset=utf-8           | Text         |
| .csv               | text/csv; charset=utf-8                | Data         |
| .mp4               | video/mp4                              | Video        |
| .webm              | video/webm                             | Video        |
| .ogg               | video/ogg                              | Video        |
| .mp3               | audio/mpeg                             | Audio        |
| .wav               | audio/wav                              | Audio        |
| .m4a               | audio/mp4                              | Audio        |
| .zip               | application/zip                        | Archives     |
| .tar               | application/x-tar                      | Archives     |
| .gz                | application/gzip                       | Archives     |
| .7z                | application/x-7z-compressed            | Archives     |
| .wasm              | application/wasm                       | WebAssembly  |

Unknown extensions default to `application/octet-stream`.

### ETag Caching

ETags enable efficient cache validation using the `If-None-Match` header.

**How it works:**

1. Server sends file with ETag header:
   ```
   HTTP/1.1 200 OK
   ETag: "12345-1609459200"
   Content-Type: text/html
   ```

2. Browser caches file and stores ETag

3. On next request, browser sends:
   ```
   GET /index.html HTTP/1.1
   If-None-Match: "12345-1609459200"
   ```

4. If file unchanged, server responds:
   ```
   HTTP/1.1 304 Not Modified
   ETag: "12345-1609459200"
   ```
   No body sent -> bandwidth saved!

**ETag format:** `"<file-size>-<modification-time>"`

**Enable/disable:**

```c
Static opts = {
    .etag = true   // Enable (default)
};
serve_static("/", "./public", &opts);
```

### Cache Control

Control browser caching with `Cache-Control` headers.

**Basic caching:**

```c
Static opts = {
    .max_age = 3600   // 1 hour
};
serve_static("/", "./public", &opts);
// Cache-Control: public, max-age=3600
```

**Long-term caching for versioned assets:**

```c
Static opts = {
    .max_age = 31536000,  // 1 year
    .immutable = true
};
serve_static("/assets", "./dist", &opts);
// Cache-Control: public, max-age=31536000, immutable
```

**Disable caching:**

```c
Static opts = {
    .max_age = 0   // No caching
};
serve_static("/", "./public", &opts);
// No Cache-Control header sent
```

**Best practices:**

- **HTML files:** No caching or short max-age (users get updates quickly)
  ```c
  Static opts = { .max_age = 0 };
  ```

- **Versioned assets** (app.v123.js): Long max-age + immutable
  ```c
  Static opts = { .max_age = 31536000, .immutable = true };
  ```

- **Regular assets:** Moderate caching with ETag validation
  ```c
  Static opts = { .max_age = 3600, .etag = true };
  ```

### Security Features

#### Path Traversal Protection

Automatically blocks directory traversal attempts:

```
GET /../../../etc/passwd   -> 403 Forbidden
GET /files/../secret.txt   -> 403 Forbidden
GET /path//double/slash    -> 403 Forbidden
```

#### Dotfile Protection

By default, dotfiles are blocked to prevent exposing sensitive files:

```
GET /.env           -> 403 Forbidden
GET /.gitignore     -> 403 Forbidden
GET /.htaccess      -> 403 Forbidden
GET /config/.env    -> 403 Forbidden
```

> [!WARNING]
>
> Never enable dotfiles unless you have a specific reason and understand the security implications!

#### Safe Path Validation

The module validates all paths for:
- Path traversal sequences (`..`)
- Null bytes
- Double slashes
- Windows drive letters
- Other malicious patterns

### Index Files

Automatically serve index files for directory requests:

```c
serve_static("/", "./public", NULL);

// GET /           -> ./public/index.html
// GET /docs/      -> ./public/docs/index.html
// GET /about/     -> ./public/about/index.html
```

**Custom index file:**

```c
Static opts = {
    .index = "home.html"
};
serve_static("/", "./public", &opts);

// GET /           -> ./public/home.html
// GET /docs/      -> ./public/docs/home.html
```

## Statistics & Monitoring

Track static file serving performance:

```c
typedef struct {
    int mounted_paths;           // Number of mounted directories
    uint64_t total_requests;     // Total requests served
    uint64_t cache_hits;         // 304 Not Modified responses
    uint64_t not_found;          // 404 responses
    uint64_t forbidden;          // 403 responses
    uint64_t total_bytes_served; // Total bytes served
} static_stats_t;
```

**Get statistics:**

```c
void stats_handler(Req *req, Res *res) {
    static_stats_t stats;
    static_get_stats(&stats);
    
    char *json = arena_sprintf(req->arena,
        "{"
        "\"mounted_paths\":%d,"
        "\"total_requests\":%llu,"
        "\"cache_hits\":%llu,"
        "\"not_found\":%llu,"
        "\"forbidden\":%llu,"
        "\"total_bytes_served\":%llu,"
        "\"cache_hit_rate\":%.2f"
        "}",
        stats.mounted_paths,
        (unsigned long long)stats.total_requests,
        (unsigned long long)stats.cache_hits,
        (unsigned long long)stats.not_found,
        (unsigned long long)stats.forbidden,
        (unsigned long long)stats.total_bytes_served,
        stats.total_requests > 0 
            ? (double)stats.cache_hits / stats.total_requests * 100.0 
            : 0.0
    );
    
    send_json(res, 200, json);
}
```

**Reset statistics:**

```c
static_reset_stats();
```

## Advanced Examples

### Multiple Static Directories

```c
int main(void) {
    server_init();
    static_init();  // Automatically initializes ecewo-fs
    
    // Serve main site
    if (serve_static("/", "./public", NULL) != 0) {
        fprintf(stderr, "Failed to mount main site\n");
        return 1;
    }
    
    // Serve assets with long-term caching
    Static asset_opts = {
        .max_age = 31536000,
        .immutable = true,
        .etag = true
    };
    if (serve_static("/assets", "./dist", &asset_opts) != 0) {
        fprintf(stderr, "Failed to mount assets\n");
        return 1;
    }
    
    // Serve docs with moderate caching
    Static docs_opts = {
        .max_age = 3600,
        .etag = true
    };
    if (serve_static("/docs", "./documentation", &docs_opts) != 0) {
        fprintf(stderr, "Failed to mount docs\n");
        return 1;
    }
    
    server_atexit(static_cleanup);  // Also cleans up fs module
    server_listen(3000);
    server_run();
    return 0;
}
```

### API + Static Files

> [!WARNING]
>
> Define API routes **before** static file serving to ensure proper routing priority.

```c
void api_users(Req *req, Res *res) {
    send_json(res, 200, "{\"users\":[]}");
}

void api_products(Req *req, Res *res) {
    send_json(res, 200, "{\"products\":[]}");
}

int main(void) {
    server_init();
    static_init();  // Automatically initializes ecewo-fs
    
    // Define API routes FIRST
    get("/api/users", api_users);
    get("/api/products", api_products);
    
    // Static files LAST (fallback)
    if (serve_static("/", "./public", NULL) != 0) {
        fprintf(stderr, "Failed to mount static files\n");
        return 1;
    }
    
    /*
     * Request handling priority:
     * 1. GET /api/users     -> api_users() handler
     * 2. GET /api/products  -> api_products() handler
     * 3. GET /*             -> Static files
     */
    
    server_atexit(static_cleanup);  // Also cleans up fs module
    server_listen(3000);
    server_run();
    return 0;
}
```

### SPA (Single Page Application) Support

For SPAs like React, Vue, or Angular, serve `index.html` for all non-file routes:

```c
void spa_handler(Req *req, Res *res) {
    // Serve index.html for all routes
    SendFile opts = {
        .max_age = 0,           // Don't cache HTML
        .cache_control = false
    };
    send_file(res, "./public/index.html", &opts);
}

int main(void) {
    server_init();
    static_init();  // Automatically initializes ecewo-fs
    
    // API routes
    get("/api/*", api_handler);
    
    // Static assets with caching
    Static opts = {
        .max_age = 31536000,
        .immutable = true
    };
    if (serve_static("/static", "./public/static", &opts) != 0) {
        fprintf(stderr, "Failed to mount static assets\n");
        return 1;
    }
    
    // SPA fallback - serve index.html for all other routes
    get("/*", spa_handler);
    
    server_atexit(static_cleanup);  // Also cleans up fs module
    server_listen(3000);
    server_run();
    return 0;
}
```

### Conditional File Serving

```c
void protected_file_handler(Req *req, Res *res) {
    // Check authentication
    const char *token = get_header(req, "Authorization");
    if (!token || !validate_token(token)) {
        send_text(res, 401, "Unauthorized");
        return;
    }
    
    // Serve protected file
    const char *file = get_param(req, "file");
    char *filepath = arena_sprintf(req->arena, "protected/%s", file);
    
    SendFile opts = {
        .root = NULL,
        .max_age = 0,           // Don't cache protected files
        .dotfiles = false
    };
    
    send_file(res, filepath, &opts);
}
```

### Custom 404 Handler

```c
void custom_404(Req *req, Res *res) {
    SendFile opts = {
        .max_age = 3600,
        .content_type = "text/html; charset=utf-8"
    };
    send_file(res, "./public/404.html", &opts);
}

int main(void) {
    server_init();
    static_init();  // Automatically initializes ecewo-fs
    
    // Static files
    if (serve_static("/", "./public", NULL) != 0) {
        fprintf(stderr, "Failed to mount static files\n");
        return 1;
    }
    
    // Custom 404 (must be last)
    get("/*", custom_404);
    
    server_atexit(static_cleanup);  // Also cleans up fs module
    server_listen(3000);
    server_run();
    return 0;
}
```

## Limitations

- Maximum file size: 100 MB (configurable via `ECEWO_FS_MAX_FILE_SIZE`)
- No built-in compression (use reverse proxy like nginx)
- No range request support yet (planned for future)
- No directory listing (use a frontend framework or custom handler)
