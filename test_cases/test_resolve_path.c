/*
 * test_resolve_path.c — Criterion suite for resolve_path().
 *
 * Build (FreeBSD, after `pkg install criterion`):
 *   cc -std=c11 -Wall -Wextra -g \
 *      test_cases/test_resolve_path.c requests/resolve_path.c \
 *      requests/parse_request.c \
 *      -I requests -I /usr/local/include \
 *      -L /usr/local/lib -lcriterion -lmagic \
 *      -o test_cases/test_resolve_path
 *
 * Add to Makefile:
 *   RESOLVE_TEST_SRC  = test_cases/test_resolve_path.c
 *   RESOLVE_TEST_UNIT = requests/resolve_path.c requests/parse_request.c
 *   RESOLVE_BINARY    = test_cases/test_resolve_path
 *
 *   .PHONY: test-resolve
 *   test-resolve: $(RESOLVE_BINARY)
 *       @./$(RESOLVE_BINARY) -j1 --quiet
 *
 *   $(RESOLVE_BINARY): $(RESOLVE_TEST_SRC) $(RESOLVE_TEST_UNIT)
 *       $(CC) $(TEST_CFLAGS) -o $@ \
 *           $(RESOLVE_TEST_SRC) $(RESOLVE_TEST_UNIT) \
 *           $(TEST_LDFLAGS) -lmagic
 *
 * Contract under test:
 *   int resolve_path(Client *c, const Request *req, ResolvedPath *rp,
 *                    magic_t magic);
 *
 *   Returns  0  success. rp fully populated. c->resp_val untouched
 *               (build_response owns the 200 decision).
 *   Returns -1  failure. c->resp_val set to specific error. rp contents
 *               unspecified except file_ptr (always NULL on -1 path so
 *               close_resolve_path_ptr is safe to call unconditionally).
 *
 * Fixture layout (created in suite_setup, destroyed in suite_teardown):
 *   test_cases/fixtures/
 *   ├── index.html              root / -> serves this
 *   ├── testdir/
 *   │   ├── file                regular readable file -> 200
 *   │   ├── file2               regular readable file -> 200
 *   │   ├── file space          filename with literal space -> 200
 *   │   ├── notallowed          chmod 000 -> 403
 *   │   ├── dir/                directory WITH index.html -> 200
 *   │   │   └── index.html
 *   │   └── dir2/               directory WITHOUT index -> listing
 *   └── cgi-bin/
 *       ├── env.cgi             executable (for C_FLAG tests)
 *       └── post.cgi            executable
 *
 * DECISION PINs are marked throughout — spots where the test asserts a
 * specific contract you chose. If you change a decision, change the assert,
 * but change it consciously.
 */

#include <criterion/criterion.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../requests/parse_request.h" /* Request, enums, client_response */
#include "../requests/resolve_path.h" /* ResolvedPath, resolve_path,
                                          close_resolve_path_ptr */
#include "../flags/flags.h" /* C_FLAG */

/* ------------------------------------------------------------------ *
 *  app_flags — the extern resolve_path reads for C_FLAG.
 *  Defined here so the test binary links without the full server.
 *  Reset to 0 in each test's setup so CGI tests don't bleed.
 * ------------------------------------------------------------------ */
uint32_t app_flags = 0;
FILE *log_ptr = NULL;
/* ------------------------------------------------------------------ *
 *  Fixture root — resolve_path uses BASEURL "./" so we chdir() into
 *  the fixture directory before each test and restore afterwards.
 * ------------------------------------------------------------------ */
#define FIXTURE_DIR "test_cases/fixtures"

static magic_t test_magic;
static char orig_dir[PATH_MAX];

/* ------------------------------------------------------------------ *
 *  Helpers
 * ------------------------------------------------------------------ */

static Client
make_client(void)
{
	Client c;
	memset(&c, 0, sizeof c);
	c.resp_val = NUM_CLIENT_RESP;
	c.fd = -1;
	return c;
}

static Request
make_request(const char *path)
{
	Request req;
	memset(&req, 0, sizeof req);
	req.method = HTTP_GET;
	req.version = HTTP_1_0;
	strlcpy(req.path, path, sizeof req.path);
	return req;
}

static void
write_fixture(const char *relpath, const char *content)
{
	char full[PATH_MAX];
	snprintf(full, sizeof full, "%s/%s", FIXTURE_DIR, relpath);
	FILE *f = fopen(full, "w");
	cr_assert_not_null(f, "fixture setup: could not create %s", full);
	fputs(content, f);
	fclose(f);
}

/* ------------------------------------------------------------------ *
 *  Fixture lifecycle.
 *
 *  This Criterion version (2.4.3) has no per-suite .init_suite/.fini_suite
 *  TestSuite fields — only per-test .init/.fini. So the fixture tree is
 *  built ONCE per test process, guarded by a static flag in test_setup,
 *  and torn down via atexit().
 *
 *  Criterion forks each test into its own process, so the static guard
 *  resets per fork — each test rebuilds the tiny tree in its own process.
 *  That's fine and is in fact why a crash in one test can't corrupt
 *  another's fixtures. The atexit handler cleans up each fork's copy.
 * ------------------------------------------------------------------ */

static void
build_fixtures(void)
{
	mkdir(FIXTURE_DIR, 0755);
	mkdir(FIXTURE_DIR "/testdir", 0755);
	mkdir(FIXTURE_DIR "/testdir/dir", 0755);
	mkdir(FIXTURE_DIR "/testdir/dir2", 0755);
	mkdir(FIXTURE_DIR "/cgi-bin", 0755);

	write_fixture("index.html", "<html>root</html>");
	write_fixture("testdir/file", "hello");
	write_fixture("testdir/file2", "world");
	write_fixture("testdir/file space", "space in name");
	write_fixture("testdir/notallowed", "forbidden");
	write_fixture("testdir/dir/index.html", "<html>dir index</html>");
	/* testdir/dir2 intentionally has no index.html */

	write_fixture("cgi-bin/env.cgi", "#!/bin/sh\nenv\n");
	write_fixture("cgi-bin/post.cgi", "#!/bin/sh\necho ok\n");

	char path[PATH_MAX];
	snprintf(path, sizeof path, "%s/cgi-bin/env.cgi", FIXTURE_DIR);
	chmod(path, 0755);
	snprintf(path, sizeof path, "%s/cgi-bin/post.cgi", FIXTURE_DIR);
	chmod(path, 0755);
	snprintf(path, sizeof path, "%s/testdir/notallowed", FIXTURE_DIR);
	chmod(path, 0000);
}

static void
destroy_fixtures(void)
{
	/* Restore permissions so rm -rf can remove the file. */
	char path[PATH_MAX];
	snprintf(path, sizeof path, "%s/testdir/notallowed", FIXTURE_DIR);
	chmod(path, 0644);

	char cmd[PATH_MAX + 32];
	snprintf(cmd, sizeof cmd, "rm -rf %s", FIXTURE_DIR);
	system(cmd);
}

/* Per-test setup: build fixtures + init magic once per test process,
 * then chdir into the fixture dir and reset app_flags every test. */
void
test_setup(void)
{
	static int initialised = 0;

	if (!initialised) {
		cr_assert_not_null(getcwd(orig_dir, sizeof orig_dir),
		    "getcwd failed in test_setup");
		build_fixtures();
		atexit(destroy_fixtures);

		test_magic = magic_open(MAGIC_MIME_TYPE);
		cr_assert_not_null(test_magic, "magic_open failed");
		magic_load(test_magic, NULL);

		initialised = 1;
	}

	app_flags = 0;
	cr_assert_eq(chdir(FIXTURE_DIR),
	    0,
	    "chdir into fixtures failed: %s",
	    strerror(errno));
}

void
test_teardown(void)
{
	chdir(orig_dir);
}

/* ------------------------------------------------------------------ *
 *  Attach per-test setup/teardown to every suite.
 * ------------------------------------------------------------------ */
TestSuite(resolve_root, .init = test_setup, .fini = test_teardown);
TestSuite(resolve_file, .init = test_setup, .fini = test_teardown);
TestSuite(resolve_dir, .init = test_setup, .fini = test_teardown);
TestSuite(resolve_error, .init = test_setup, .fini = test_teardown);
TestSuite(resolve_cgi, .init = test_setup, .fini = test_teardown);
TestSuite(resolve_query, .init = test_setup, .fini = test_teardown);
TestSuite(cs631_resolve, .init = test_setup, .fini = test_teardown);

/* ------------------------------------------------------------------ *
 *  Macros: one-liner setup and cleanup for the common case.
 *  rp is zero-initialised so close_resolve_path_ptr is always safe.
 *
 *  RESOLVE_SETUP works for paths WITH query strings too — make_request
 *  copies path_str verbatim into req.path, so
 *  RESOLVE_SETUP("/testdir/file?key=value") is valid.
 *  test_teardown() restores cwd, so no manual chdir needed.
 * ------------------------------------------------------------------ */
#define RESOLVE_SETUP(path_str)                                                \
	Client c = make_client();                                              \
	Request req = make_request(path_str);                                  \
	ResolvedPath rp;                                                       \
	memset(&rp, 0, sizeof rp);                                             \
	int rc = resolve_path(&c, &req, &rp, test_magic);                      \
	(void)rc

#define RESOLVE_CLEANUP() close_resolve_path_ptr(&rp)

/* ================================================================== *
 *  GROUP 1 — root: / -> index.html
 * ================================================================== */

Test(resolve_root, root_serves_index_html)
{
	RESOLVE_SETUP("/");
	cr_assert_eq(rc, 0, "/ should resolve successfully");
	cr_assert_eq(c.resp_val,
	    NUM_CLIENT_RESP,
	    "success must not touch resp_val");
	cr_assert_not_null(rp.file_ptr, "/ must open index.html");
	cr_assert_gt(rp.file_size, 0);
	cr_assert_not_null(rp.mime_type);
	cr_assert_eq(rp.is_dir_listing, 0);
	cr_assert_eq(rp.is_cgi_bin, 0);
	RESOLVE_CLEANUP();
}

Test(resolve_root, root_mime_is_html)
{
	RESOLVE_SETUP("/");
	cr_assert_eq(rc, 0);
	cr_assert(strstr(rp.mime_type, "text/html") != NULL,
	    "/ must have text/html MIME, got: %s",
	    rp.mime_type);
	RESOLVE_CLEANUP();
}

Test(resolve_root, root_query_string_empty)
{
	RESOLVE_SETUP("/");
	cr_assert_eq(rc, 0);
	cr_assert_str_eq(rp.query_string, "");
	RESOLVE_CLEANUP();
}

/* ================================================================== *
 *  GROUP 2 — regular files
 * ================================================================== */

Test(resolve_file, existing_file_succeeds)
{
	RESOLVE_SETUP("/testdir/file");
	cr_assert_eq(rc, 0);
	cr_assert_not_null(rp.file_ptr);
	cr_assert_gt(rp.file_size, 0);
	cr_assert_eq(rp.is_dir_listing, 0);
	cr_assert_eq(rp.is_cgi_bin, 0);
	RESOLVE_CLEANUP();
}

Test(resolve_file, existing_file2_succeeds)
{
	RESOLVE_SETUP("/testdir/file2");
	cr_assert_eq(rc, 0);
	cr_assert_not_null(rp.file_ptr);
	RESOLVE_CLEANUP();
}

/* file_size from fstat must match the actual byte count we wrote. */
Test(resolve_file, file_size_matches_content)
{
	RESOLVE_SETUP("/testdir/file");
	cr_assert_eq(rc, 0);
	cr_assert_eq(rp.file_size,
	    (off_t)5, /* "hello" = 5 bytes */
	    "file_size must equal fstat st_size");
	RESOLVE_CLEANUP();
}

Test(resolve_file, nonexistent_file_returns_404)
{
	RESOLVE_SETUP("/nowhere");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_404);
	cr_assert_null(rp.file_ptr);
	RESOLVE_CLEANUP();
}

Test(resolve_file, deeply_nested_nonexistent_returns_404)
{
	RESOLVE_SETUP("/nested/1/2/3/no/such/file");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_404);
	RESOLVE_CLEANUP();
}

/* DECISION PIN: literal space in filename. No percent-decoding —
   req->path is taken verbatim. File exists -> 200. */
Test(resolve_file, filename_with_literal_space)
{
	RESOLVE_SETUP("/testdir/file space");
	cr_assert_eq(rc,
	    0,
	    "literal space in filename should serve if file exists");
	cr_assert_not_null(rp.file_ptr);
	RESOLVE_CLEANUP();
}

/* ================================================================== *
 *  GROUP 3 — directories
 * ================================================================== */

/* Directory with index.html -> open it, is_dir_listing stays 0. */
Test(resolve_dir, dir_with_index_serves_index)
{
	RESOLVE_SETUP("/testdir/dir");
	cr_assert_eq(rc, 0);
	cr_assert_not_null(rp.file_ptr, "dir with index.html must open it");
	cr_assert_eq(rp.is_dir_listing, 0);
	RESOLVE_CLEANUP();
}

Test(resolve_dir, dir_trailing_slash_with_index)
{
	RESOLVE_SETUP("/testdir/dir/");
	cr_assert_eq(rc, 0);
	cr_assert_not_null(rp.file_ptr);
	cr_assert_eq(rp.is_dir_listing, 0);
	RESOLVE_CLEANUP();
}

/* DECISION PIN: /testdir/dir/. — single-dot segment.
   Parser allows it (not traversal). Resolve must normalise "."
   and still find the directory's index.html. */
Test(resolve_dir, dir_dot_segment_finds_index)
{
	RESOLVE_SETUP("/testdir/dir/.");
	cr_assert_eq(rc, 0, "/testdir/dir/. should resolve like /testdir/dir/");
	cr_assert_not_null(rp.file_ptr);
	RESOLVE_CLEANUP();
}

/* Directory without index.html -> listing: rc=0, file_ptr=NULL,
   is_dir_listing=1. build_response will generate the HTML. */
Test(resolve_dir, dir_without_index_triggers_listing)
{
	RESOLVE_SETUP("/testdir/dir2/");
	cr_assert_eq(rc, 0);
	cr_assert_null(rp.file_ptr,
	    "no file to stream when listing a directory");
	cr_assert_eq(rp.is_dir_listing, 1);
	RESOLVE_CLEANUP();
}

Test(resolve_dir, dir_without_index_no_trailing_slash)
{
	RESOLVE_SETUP("/testdir/dir2");
	cr_assert_eq(rc, 0);
	cr_assert_eq(rp.is_dir_listing, 1);
	RESOLVE_CLEANUP();
}

/* ================================================================== *
 *  GROUP 4 — permission denied
 * ================================================================== */

Test(resolve_error, permission_denied_returns_403)
{
	if (getuid() == 0)
		cr_skip("permission test skipped: running as root");

	RESOLVE_SETUP("/testdir/notallowed");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_403);
	cr_assert_null(rp.file_ptr);
	RESOLVE_CLEANUP();
}

/* ================================================================== *
 *  GROUP 5 — CGI-bin routing
 * ================================================================== */

/* C_FLAG unset: 501, is_cgi_bin stays 0, filesystem not touched. */
Test(resolve_cgi, cgi_flag_unset_returns_501)
{
	app_flags = 0;
	RESOLVE_SETUP("/cgi-bin/env.cgi");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_501);
	cr_assert_null(rp.file_ptr);
	cr_assert_eq(rp.is_cgi_bin, 0);
	RESOLVE_CLEANUP();
}

Test(resolve_cgi, cgi_post_flag_unset_returns_501)
{
	app_flags = 0;
	RESOLVE_SETUP("/cgi-bin/post.cgi");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_501);
	RESOLVE_CLEANUP();
}

/* C_FLAG set: rc=0, is_cgi_bin=1. resolve does NOT exec — that's
   build_response's job when it sees the flag. */
Test(resolve_cgi, cgi_flag_set_routes_to_cgi)
{
	app_flags = C_FLAG;
	RESOLVE_SETUP("/cgi-bin/env.cgi");
	cr_assert_eq(rc, 0);
	cr_assert_eq(rp.is_cgi_bin, 1);
	cr_assert_eq(c.resp_val,
	    NUM_CLIENT_RESP,
	    "resp_val must be untouched on success");
	RESOLVE_CLEANUP();
}

/* DECISION PIN: empty script name (/cgi-bin/ with nothing after).
   C_FLAG unset -> 501 (hits the not-enabled check first).
   If you add script-name validation before the flag check, flip to 400. */
Test(resolve_cgi, cgi_empty_script_name_no_flag)
{
	app_flags = 0;
	RESOLVE_SETUP("/cgi-bin/");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_501);
	RESOLVE_CLEANUP();
}

/* CS631: /cgi-binaajsodijaoisjd looks like /cgi-bin but lacks the
   trailing slash. Must NOT be routed as CGI — treat as a normal path.
   DECISION PIN: 404 (file doesn't exist in fixtures). */
Test(resolve_cgi, cgi_bin_false_prefix_not_routed)
{
	app_flags = C_FLAG;
	RESOLVE_SETUP("/cgi-binaajsodijaoisjd");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val,
	    RESP_404,
	    "/cgi-bin<garbage> must not match the /cgi-bin/ prefix");
	cr_assert_eq(rp.is_cgi_bin, 0);
	RESOLVE_CLEANUP();
}

/* ================================================================== *
 *  GROUP 6 — query string splitting
 *
 *  RESOLVE_SETUP accepts paths with query strings verbatim since
 *  make_request uses strlcpy. test_teardown restores cwd, so no
 *  manual chdir needed — these tests use the standard macros.
 * ================================================================== */

/* DECISION PIN: resolve_path splits req->path on '?'.
   query_string gets everything after '?'.
   The file lookup uses only the path part (no '?'). */
Test(resolve_query, query_string_extracted)
{
	RESOLVE_SETUP("/testdir/file?key=value");
	cr_assert_eq(rc, 0);
	cr_assert_str_eq(rp.query_string, "key=value");
	cr_assert_not_null(rp.file_ptr,
	    "file must open — query string doesn't affect lookup");
	RESOLVE_CLEANUP();
}

Test(resolve_query, multiple_params_extracted)
{
	RESOLVE_SETUP("/testdir/file?q=foo&food=bacon");
	cr_assert_eq(rc, 0);
	cr_assert_str_eq(rp.query_string, "q=foo&food=bacon");
	RESOLVE_CLEANUP();
}

Test(resolve_query, no_query_string_leaves_field_empty)
{
	RESOLVE_SETUP("/testdir/file");
	cr_assert_eq(rc, 0);
	cr_assert_str_eq(rp.query_string, "");
	RESOLVE_CLEANUP();
}

/* CGI with query string: query preserved, is_cgi_bin=1. */
Test(resolve_query, cgi_query_string_preserved)
{
	app_flags = C_FLAG;
	RESOLVE_SETUP("/cgi-bin/env.cgi?q=foo&food=bacon");
	cr_assert_eq(rc, 0);
	cr_assert_eq(rp.is_cgi_bin, 1);
	cr_assert_str_eq(rp.query_string, "q=foo&food=bacon");
	RESOLVE_CLEANUP();
}

/* ================================================================== *
 *  GROUP 7 — CS631 conformance at the resolve layer
 * ================================================================== */

Test(cs631_resolve, passwd_not_found)
{
	RESOLVE_SETUP("/passwd");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_404);
	RESOLVE_CLEANUP();
}

Test(cs631_resolve, nowhere_not_found)
{
	RESOLVE_SETUP("/nowhere");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_404);
	RESOLVE_CLEANUP();
}

Test(cs631_resolve, testdir_ls_not_found)
{
	RESOLVE_SETUP("/testdir/ls");
	cr_assert_eq(rc, -1);
	cr_assert_eq(c.resp_val, RESP_404);
	RESOLVE_CLEANUP();
}

/* Safety guarantee: close_resolve_path_ptr on a zero-initialised
   ResolvedPath (parse failed, resolve never ran) must not crash.
   This is what makes the unconditional cleanup in do_read safe. */
Test(cs631_resolve, cleanup_on_uninitialised_rp_is_safe)
{
	ResolvedPath rp;
	memset(&rp, 0, sizeof rp);
	close_resolve_path_ptr(&rp); /* must not crash */
}

/*
 * The following CS631 cases belong at this layer but require features
 * not yet built. Add the test when the feature lands:
 *
 *   ~user expansion (/~jschauma/, /~nobody)        out of scope for now
 *   Percent-decoding (/testdir/file%20space)       DECISION: decode or literal?
 *   CGI execution   (/cgi-bin/env.cgi + C_FLAG)    deferred (async pipe)
 *   PATH_INFO       (/cgi-bin/env.cgi/yes/we/...)  deferred with CGI
 */
