#include "ecewo.h"
#include "ecewo-mock.h"
#include "ecewo-fs.h"
#include "ecewo-static.h"
#include "tester.h"
#include "uv.h"
#include <string.h>
#include <stdio.h>

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

// ========================================================================
void setup_static_routes(void) {
  uv_fs_t req;

  // Create test directory
  int r = uv_fs_mkdir(NULL, &req, "test_public", 0755, NULL);
  uv_fs_req_cleanup(&req);

  if (r != 0 && r != UV_EEXIST) {
    fprintf(stderr, "Failed to create test_public: %s\n", uv_strerror(r));
    return;
  }

  // Create index.html
  const char *index_content = "<html><body>Hello from static server</body></html>";
  uv_file file = uv_fs_open(NULL, &req, "test_public/index.html",
                            UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_TRUNC,
                            0644, NULL);
  uv_fs_req_cleanup(&req);

  if (file >= 0) {
    uv_buf_t buf = uv_buf_init((char *)index_content, strlen(index_content));
    uv_fs_write(NULL, &req, file, &buf, 1, -1, NULL);
    uv_fs_req_cleanup(&req);

    uv_fs_close(NULL, &req, file, NULL);
    uv_fs_req_cleanup(&req);
  }

  // Create .env (dotfile to test blocking)
  const char *env_content = "SECRET=super_secret_value";
  file = uv_fs_open(NULL, &req, "test_public/.env",
                    UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_TRUNC,
                    0644, NULL);
  uv_fs_req_cleanup(&req);

  if (file >= 0) {
    uv_buf_t buf = uv_buf_init((char *)env_content, strlen(env_content));
    uv_fs_write(NULL, &req, file, &buf, 1, -1, NULL);
    uv_fs_req_cleanup(&req);

    uv_fs_close(NULL, &req, file, NULL);
    uv_fs_req_cleanup(&req);
  }

  // Mount static directory
  if (serve_static("/", "./test_public", NULL) != 0)
    fprintf(stderr, "Failed to mount static directory\n");

  // Verify files exist (debug)
  uv_fs_t stat_req;
  int stat_result = uv_fs_stat(NULL, &stat_req, "test_public/index.html", NULL);
  printf("DEBUG: index.html exists? %s (result=%d)\n",
         stat_result == 0 ? "YES" : "NO", stat_result);
  if (stat_result == 0) {
    printf("DEBUG: File size: %lld bytes\n", (long long)stat_req.statbuf.st_size);
  }
  uv_fs_req_cleanup(&stat_req);
}

void cleanup_static(void) {
  uv_fs_t req;

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

  uv_fs_rmdir(NULL, &req, "test_public", NULL);
  uv_fs_req_cleanup(&req);
}

int main(void) {
  if (static_init() != 0) {
    printf("ERROR: Failed to initialize static module\n");
    return 1;
  }

  // Create test directory and files
  uv_fs_t req;
  int r = uv_fs_mkdir(NULL, &req, "test_public", 0755, NULL);
  uv_fs_req_cleanup(&req);

  if (r != 0 && r != UV_EEXIST) {
    fprintf(stderr, "Failed to create test_public: %s\n", uv_strerror(r));
  }

  if (mock_init(setup_static_routes) != 0) {
    printf("ERROR: Failed to initialize mock server\n");
    cleanup_static();
    static_cleanup();
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

  printf("\nAll tests passed!\n");

  mock_cleanup();
  cleanup_static();
  static_cleanup();

  return 0;
}
