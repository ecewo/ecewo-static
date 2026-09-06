#include "ecewo.h"
#include "ecewo-mock.h"
#include "ecewo-fs.h"
#include "ecewo-static.h"
#include "tester.h"
#include "uv.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Handlers used by tests
// ============================================================================

static void send_file_test_handler(ecewo_request_t *req, ecewo_response_t *res) {
  ecewo_send_file(req, res, "./test_public/index.html", NULL);
}

// root is configured, so an absolute filepath must NOT escape it.
static void send_file_absolute_handler(ecewo_request_t *req, ecewo_response_t *res) {
  ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
  if (!opts) {
    ecewo_send_text(res, 500, "no opts");
    return;
  }
  ecewo_send_file_options_set_root(opts, "./test_public");
  ecewo_send_file(req, res, "/etc/passwd", opts);
  ecewo_send_file_options_free(opts);
}

// root is configured, so ".." must NOT climb above it.
static void send_file_traversal_handler(ecewo_request_t *req, ecewo_response_t *res) {
  ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
  if (!opts) {
    ecewo_send_text(res, 500, "no opts");
    return;
  }
  ecewo_send_file_options_set_root(opts, "./test_public");
  ecewo_send_file(req, res, "../test_outside/secret.txt", opts);
  ecewo_send_file_options_free(opts);
}

// A relative path under root still has to work.
static void send_file_rooted_ok_handler(ecewo_request_t *req, ecewo_response_t *res) {
  ecewo_send_file_options_t *opts = ecewo_send_file_options_new();
  if (!opts) {
    ecewo_send_text(res, 500, "no opts");
    return;
  }
  ecewo_send_file_options_set_root(opts, "./test_public");
  ecewo_send_file(req, res, "index.html", opts);
  ecewo_send_file_options_free(opts);
}

// ============================================================================
// Tests
// ============================================================================

int test_static_serve_html(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/index.html",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(strstr(res.body, "<html>"));

  free_request(&res);
  RETURN_OK();
}

int test_static_serve_index(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(strstr(res.body, "<html>"));

  free_request(&res);
  RETURN_OK();
}

int test_static_not_found(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/nonexistent.html",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(404, res.status_code);

  free_request(&res);
  RETURN_OK();
}

int test_static_dotfile_blocked(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/.env",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(403, res.status_code);

  free_request(&res);
  RETURN_OK();
}

int test_static_path_traversal_blocked(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/../../../etc/passwd",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_TRUE(res.status_code == 403 || res.status_code == 404);

  free_request(&res);
  RETURN_OK();
}

int test_static_mime_type_html(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/index.html",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  const char *content_type = mock_get_header(&res, "Content-Type");
  ASSERT_NOT_NULL(content_type);
  ASSERT_NOT_NULL(strstr(content_type, "text/html"));

  free_request(&res);
  RETURN_OK();
}

int test_static_etag_present(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/index.html",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  const char *etag = mock_get_header(&res, "ETag");
  ASSERT_NOT_NULL(etag);

  free_request(&res);
  RETURN_OK();
}

int test_static_etag_304(void) {
  // First request - get the ETag
  MockParams params1 = {
    .method = MOCK_GET,
    .path = "/index.html",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };
  MockResponse res1 = request(&params1);
  ASSERT_EQ(200, res1.status_code);
  const char *etag = mock_get_header(&res1, "ETag");
  ASSERT_NOT_NULL(etag);

  char etag_copy[256];
  strncpy(etag_copy, etag, sizeof(etag_copy) - 1);
  etag_copy[sizeof(etag_copy) - 1] = '\0';
  free_request(&res1);

  // Second request with If-None-Match - expect 304
  MockHeaders headers[] = { { "If-None-Match", etag_copy } };
  MockParams params2 = {
    .method = MOCK_GET,
    .path = "/index.html",
    .body = NULL,
    .headers = headers,
    .header_count = 1
  };
  MockResponse res2 = request(&params2);
  ASSERT_EQ(304, res2.status_code);

  free_request(&res2);
  RETURN_OK();
}

int test_static_redirect(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/subdir",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(301, res.status_code);
  const char *location = mock_get_header(&res, "Location");
  ASSERT_NOT_NULL(location);
  ASSERT_NOT_NULL(strstr(location, "/subdir/"));

  free_request(&res);
  RETURN_OK();
}

int test_static_extensions(void) {
  // GET /ext/page has no extension - the mount at /ext has extensions=["html"]
  // so it should resolve to test_public/page.html
  MockParams params = {
    .method = MOCK_GET,
    .path = "/ext/page",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(strstr(res.body, "<html>"));

  free_request(&res);
  RETURN_OK();
}

int test_send_file(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/sendfile",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(strstr(res.body, "<html>"));
  ASSERT_NOT_NULL(mock_get_header(&res, "ETag"));

  free_request(&res);
  RETURN_OK();
}

int test_send_file_etag_304(void) {
  // First request - get the ETag from send_file
  MockParams params1 = {
    .method = MOCK_GET,
    .path = "/sendfile",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };
  MockResponse res1 = request(&params1);
  ASSERT_EQ(200, res1.status_code);
  const char *etag = mock_get_header(&res1, "ETag");
  ASSERT_NOT_NULL(etag);

  char etag_copy[256];
  strncpy(etag_copy, etag, sizeof(etag_copy) - 1);
  etag_copy[sizeof(etag_copy) - 1] = '\0';
  free_request(&res1);

  // Second request with If-None-Match - expect 304
  MockHeaders headers[] = { { "If-None-Match", etag_copy } };
  MockParams params2 = {
    .method = MOCK_GET,
    .path = "/sendfile",
    .body = NULL,
    .headers = headers,
    .header_count = 1
  };
  MockResponse res2 = request(&params2);
  ASSERT_EQ(304, res2.status_code);

  free_request(&res2);
  RETURN_OK();
}

// ============================================================================
// Setup / cleanup
// ============================================================================

static void write_file(const char *path, const char *content) {
  uv_fs_t req;
  uv_file file = uv_fs_open(NULL, &req, path,
                            UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_TRUNC,
                            0644, NULL);
  uv_fs_req_cleanup(&req);
  if (file < 0)
    return;

  uv_buf_t buf = uv_buf_init((char *)content, strlen(content));
  uv_fs_write(NULL, &req, file, &buf, 1, -1, NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_close(NULL, &req, file, NULL);
  uv_fs_req_cleanup(&req);
}

static void mkdir_sync(const char *path) {
  uv_fs_t req;
  uv_fs_mkdir(NULL, &req, path, 0755, NULL);
  uv_fs_req_cleanup(&req);
}

void setup_static_routes(ecewo_app_t *app) {
  const char *index_content = "<html><body>Hello from static server</body></html>";
  const char *page_content = "<html><body>Page with extension fallback</body></html>";
  const char *sub_content = "<html><body>Subdir index</body></html>";
  const char *env_content = "SECRET=super_secret_value";

  mkdir_sync("test_public");
  mkdir_sync("test_public/subdir");

  write_file("test_public/index.html", index_content);
  write_file("test_public/page.html", page_content);
  write_file("test_public/subdir/index.html", sub_content);
  write_file("test_public/.env", env_content);

  // Main mount: redirect=true, no extensions
  if (ecewo_serve_static(app, "/", "./test_public", NULL) != 0)
    fprintf(stderr, "Failed to mount /\n");

  // Extension-fallback mount at /ext
  ecewo_static_options_t *ext_opts = ecewo_static_options_new();
  if (ext_opts) {
    const char *exts[] = { "html" };
    ecewo_static_options_set_extensions(ext_opts, exts, 1);
    if (ecewo_serve_static(app, "/ext", "./test_public", ext_opts) != 0)
      fprintf(stderr, "Failed to mount /ext\n");
    ecewo_static_options_free(ext_opts);
  }

  // A file outside the mount, plus a symlink inside it that points there.
  mkdir_sync("test_outside");
  write_file("test_outside/secret.txt", "OUTSIDE_SECRET");
#ifndef _WIN32
  {
    uv_fs_t req;
    uv_fs_unlink(NULL, &req, "test_public/escape.txt", NULL);
    uv_fs_req_cleanup(&req);
    uv_fs_symlink(NULL, &req, "../test_outside/secret.txt",
                  "test_public/escape.txt", 0, NULL);
    uv_fs_req_cleanup(&req);
  }
#endif

  // Route for send_file tests
  ECEWO_GET(app, "/sendfile", send_file_test_handler);
  ECEWO_GET(app, "/sendfile-absolute", send_file_absolute_handler);
  ECEWO_GET(app, "/sendfile-traversal", send_file_traversal_handler);
  ECEWO_GET(app, "/sendfile-rooted", send_file_rooted_ok_handler);
}

void cleanup_static(void) {
  uv_fs_t req;

  // Remove known subdirectory contents first
  uv_fs_unlink(NULL, &req, "test_public/subdir/index.html", NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_rmdir(NULL, &req, "test_public/subdir", NULL);
  uv_fs_req_cleanup(&req);

  // Remove top-level files
  uv_fs_scandir(NULL, &req, "test_public", 0, NULL);
  uv_dirent_t dent;
  while (uv_fs_scandir_next(&req, &dent) != UV_EOF) {
    char path[512];
    snprintf(path, sizeof(path), "test_public/%s", dent.name);
    uv_fs_t unlink_req;
    uv_fs_unlink(NULL, &unlink_req, path, NULL);
    uv_fs_req_cleanup(&unlink_req);
  }
  uv_fs_req_cleanup(&req);

  uv_fs_unlink(NULL, &req, "test_public/escape.txt", NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_rmdir(NULL, &req, "test_public", NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_unlink(NULL, &req, "test_outside/secret.txt", NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_rmdir(NULL, &req, "test_outside", NULL);
  uv_fs_req_cleanup(&req);
}

// A symlink inside the served directory that points outside it must not be
// followed: lexical checks cannot see it, only realpath containment can.
int test_symlink_escape_blocked(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/escape.txt",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_NE(200, res.status_code);
  if (res.body)
    ASSERT_NULL(strstr(res.body, "OUTSIDE_SECRET"));

  free_request(&res);
  RETURN_OK();
}

int test_send_file_absolute_path_blocked(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/sendfile-absolute",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_NE(200, res.status_code);
  if (res.body)
    ASSERT_NULL(strstr(res.body, "root:"));

  free_request(&res);
  RETURN_OK();
}

int test_send_file_traversal_blocked(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/sendfile-traversal",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_NE(200, res.status_code);
  if (res.body)
    ASSERT_NULL(strstr(res.body, "OUTSIDE_SECRET"));

  free_request(&res);
  RETURN_OK();
}

// The confinement must not break the legitimate rooted case.
int test_send_file_rooted_ok(void) {
  MockParams params = {
    .method = MOCK_GET,
    .path = "/sendfile-rooted",
    .body = NULL,
    .headers = NULL,
    .header_count = 0
  };

  MockResponse res = request(&params);

  ASSERT_EQ(200, res.status_code);
  ASSERT_NOT_NULL(strstr(res.body, "<html>"));

  free_request(&res);
  RETURN_OK();
}

int main(void) {
  if (ecewo_static_init() != 0) {
    printf("ERROR: Failed to initialize static module\n");
    return 1;
  }

  if (mock_init(setup_static_routes) != 0) {
    printf("ERROR: Failed to initialize mock server\n");
    cleanup_static();
    ecewo_static_cleanup();
    return 1;
  }

  printf("\n=== Running ecewo-static tests ===\n\n");

  RUN_TEST(test_static_serve_html);
  RUN_TEST(test_static_serve_index);
  RUN_TEST(test_static_not_found);
  RUN_TEST(test_static_dotfile_blocked);
  RUN_TEST(test_static_path_traversal_blocked);
  RUN_TEST(test_static_mime_type_html);
  RUN_TEST(test_static_etag_present);
  RUN_TEST(test_static_etag_304);
  RUN_TEST(test_static_redirect);
  RUN_TEST(test_static_extensions);
  RUN_TEST(test_send_file);
  RUN_TEST(test_send_file_etag_304);
  RUN_TEST(test_symlink_escape_blocked);
  RUN_TEST(test_send_file_absolute_path_blocked);
  RUN_TEST(test_send_file_traversal_blocked);
  RUN_TEST(test_send_file_rooted_ok);

  printf("\nAll tests passed!\n");

  mock_cleanup();
  cleanup_static();
  ecewo_static_cleanup();

  return 0;
}
