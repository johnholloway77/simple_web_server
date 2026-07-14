/*
 * test_build_okay_response.c — Criterion suite for build_okay_response().
 *
 * Two layers of coverage, deliberately:
 *
 *   1. UNIT LAYER (build_okay suite): calls the REAL resolve_path() to populate
 *      a ResolvedPath, then calls the REAL build_okay_response() on it.
 *      This is NOT mocked — it deliberately exercises the resolve_path ->
 *      build_okay_response handoff, because that handoff is exactly where
 *      a real bug lived (rp->file_ptr vs c->file_ptr confusion). A test
 *      that only unit-tests build_okay_response in isolation with a
 *      hand-built ResolvedPath would NOT have caught that class of bug —
 *      the bug was in how the two functions agree on which struct owns
 *      what. Testing them together, through the real resolve_path, closes
 *      that gap.
 *
 *   2. INTEGRATION LAYER (build_okay_integration suite): forks and execs the
 *      REAL server binary, waits for it to start listening, fires real
 *      HTTP requests at it over a real socket (via curl), then sends
 *      SIGTERM and asserts a clean, graceful exit. This is the only place
 *      in the test suite that exercises the full accept -> do_read ->
 *      resolve_path -> build_okay_response -> do_write pipeline as a real
 *      running process — the unit tests above call functions directly and
 *      never open a listening socket at all.
 *
 * Running under a memory checker:
 *   - Unit layer: build this test binary itself with ASan
 *     (test-build-okay-asan) or run the whole binary under Valgrind.
 *     Since the unit layer runs in-process, whatever tool wraps the test
 *     binary covers it directly.
 *   - Integration layer: the CHILD PROCESS is a separate binary. For that
 *     child to be checked for memory errors, point SIMPLE_SERVER_BIN at a
 *     binary built for the tool you want:
 *       SIMPLE_SERVER_BIN=../simple_server_debug    (child runs under its
 *                                                     own compiled-in ASan)
 *       SIMPLE_SERVER_BIN=../simple_server_valgrind, and wrap THIS TEST
 *         BINARY's invocation with valgrind --trace-children=yes so
 *         Valgrind follows into the forked/exec'd child.
 *     Default (no env var): ../simple_server_valgrind (no sanitizer, safe
 *     under Valgrind, matches the Makefile's memcheck binary).
 *
 * Fixture layout (built fresh, torn down after):
 *   test_cases/fixtures_okay/
 *   ├── plain.txt                   text file  -> text/plain
 *   ├── page.html                   html file  -> text/html
 *   ├── withindex/
 *   │   └── index.html              directory WITH index -> served
 * transparently └── nolisting/ ├── afile.txt               directory WITHOUT
 * index, has a file └── subdir/                 ...and a subdirectory └──
 * deep.txt
 */

#include <criterion/criterion.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <magic.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../requests/parse_request.h"
#include "../requests/resolve_path.h"
#include "../response/build_response.h" /* build_okay_response */
#include "../flags/flags.h"

/* app_flags: resolve_path reads this extern for C_FLAG. Not exercised by
   these tests (no CGI fixtures here — CGI has its own suite), but must be
   defined for the link to succeed. */
uint32_t app_flags = 0;
FILE *log_ptr = NULL;

#define FIXTURE_DIR "test_cases/fixtures_okay"

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
make_request(const char *path, enum http_method method)
{
	Request req;
	memset(&req, 0, sizeof req);
	req.method = method;
	req.version = HTTP_1_0;
	strlcpy(req.path, path, sizeof req.path);
	return req;
}

/*
 * out_buf is a RAW BYTE BUFFER whose real length is c->output_length —
 * exactly like in_buf on the read side. do_write() sends output_length
 * bytes via send(); nothing guarantees out_buf is null-terminated, and a
 * full fread() of a file's exact byte count can leave no terminator at
 * all. strstr()/strlen() on out_buf directly is therefore UNSAFE and can
 * walk past the allocation hunting for a '\0' that isn't there — exactly
 * what ASan caught in the first version of this test file. Every search
 * against out_buf must be bounded by output_length, never by an assumed
 * terminator. Same principle as parse_request using strnstr on in_buf
 * instead of strstr — output side instead of input side.
 */
static const char *
bounded_find(const char *haystack, size_t haystack_len, const char *needle)
{
	size_t needle_len = strlen(needle);
	if (needle_len == 0 || needle_len > haystack_len)
		return NULL;
	for (size_t i = 0; i + needle_len <= haystack_len; i++) {
		if (memcmp(haystack + i, needle, needle_len) == 0)
			return haystack + i;
	}
	return NULL;
}

/* Parse the Content-Length value bounded by out_buf's real length, by
 * copying just the digit run into a small NUL-terminated local buffer
 * before calling strtol — never handing strtol a pointer into
 * potentially-unterminated out_buf directly. */
static long
bounded_content_length(const char *out_buf, size_t output_length)
{
	const char *cl =
	    bounded_find(out_buf, output_length, "Content-Length:");
	if (!cl)
		return -1;
	cl += strlen("Content-Length:");
	size_t remaining = output_length - (size_t)(cl - out_buf);

	char digits[32] = {0};
	size_t n = 0;
	while (n < remaining && n < sizeof digits - 1 &&
	       (cl[n] == ' ' || (cl[n] >= '0' && cl[n] <= '9'))) {
		digits[n] = cl[n];
		n++;
	}
	return strtol(digits, NULL, 10);
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

static void
build_fixtures(void)
{
	mkdir(FIXTURE_DIR, 0755);
	mkdir(FIXTURE_DIR "/withindex", 0755);
	mkdir(FIXTURE_DIR "/nolisting", 0755);
	mkdir(FIXTURE_DIR "/nolisting/subdir", 0755);

	write_fixture("plain.txt", "hello from a plain text file\n");
	write_fixture("page.html",
	    "<html><body><h1>a real page</h1></body></html>");
	write_fixture("withindex/index.html",
	    "<html><body>index served transparently</body></html>");
	write_fixture("nolisting/afile.txt",
	    "a file inside a directory with no index\n");
	write_fixture("nolisting/subdir/deep.txt",
	    "nested file, one level down\n");
}

static void
destroy_fixtures(void)
{
	char cmd[PATH_MAX + 32];
	snprintf(cmd, sizeof cmd, "rm -rf %s", FIXTURE_DIR);
	system(cmd);
}

void
okay_test_setup(void)
{
	static int initialised = 0;
	if (!initialised) {
		cr_assert_not_null(getcwd(orig_dir, sizeof orig_dir),
		    "getcwd failed in test setup");
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
okay_test_teardown(void)
{
	chdir(orig_dir);
}

TestSuite(build_okay, .init = okay_test_setup, .fini = okay_test_teardown);
TestSuite(build_okay_integration,
    .init = okay_test_setup,
    .fini = okay_test_teardown);

/* Resolve + build in one step, mirroring the real do_read call chain.
   rp is zero-initialised so close_resolve_path_ptr is always safe. */
#define RESOLVE_AND_BUILD(path_str, method)                                    \
	Client c = make_client();                                              \
	Request req = make_request(path_str, method);                          \
	ResolvedPath rp;                                                       \
	memset(&rp, 0, sizeof rp);                                             \
	int resolve_rc = resolve_path(&c, &req, &rp, test_magic);              \
	cr_assert_eq(resolve_rc,                                               \
	    0,                                                                 \
	    "resolve_path must succeed for this fixture");                     \
	int rc = build_okay_response(&c, &rp, &req);                           \
	(void)rc

#define OKAY_CLEANUP() close_resolve_path_ptr(&rp)

/* ================================================================== *
 *  UNIT LAYER — plain files
 * ================================================================== */

Test(build_okay, serves_plain_text_file)
{
	RESOLVE_AND_BUILD("/plain.txt", HTTP_GET);
	cr_assert_eq(rc, 0);
	cr_assert_not_null(c.out_buf);
	cr_assert(c.output_length >= 12 &&
		      memcmp(c.out_buf, "HTTP/1.0 200", 12) == 0,
	    "must be a 200 response");
	cr_assert_not_null(bounded_find(c.out_buf,
			       c.output_length,
			       "text/plain"),
	    ".txt must resolve to text/plain");
	cr_assert_not_null(bounded_find(c.out_buf,
			       c.output_length,
			       "hello from a plain text file"),
	    "response must contain the actual file content");
	free(c.out_buf);
	OKAY_CLEANUP();
}

Test(build_okay, serves_html_file)
{
	RESOLVE_AND_BUILD("/page.html", HTTP_GET);
	cr_assert_eq(rc, 0);
	cr_assert_not_null(bounded_find(c.out_buf,
			       c.output_length,
			       "text/html"),
	    ".html must resolve to text/html");
	cr_assert_not_null(bounded_find(c.out_buf,
			       c.output_length,
			       "a real page"),
	    "response must contain the real page content");
	free(c.out_buf);
	OKAY_CLEANUP();
}

/* Content-Length must equal the ACTUAL bytes sent, not a guess. */
Test(build_okay, file_content_length_matches_body)
{
	RESOLVE_AND_BUILD("/plain.txt", HTTP_GET);
	cr_assert_eq(rc, 0);

	long declared = bounded_content_length(c.out_buf, c.output_length);
	cr_assert_geq(declared, 0, "Content-Length header must be present");

	const char *body = bounded_find(c.out_buf, c.output_length, "\r\n\r\n");
	cr_assert_not_null(body);
	body += 4;

	/* output_length includes the header; body length = output_length
	   minus everything up to and including the header terminator. */
	size_t header_len = (size_t)(body - c.out_buf);
	size_t actual_body_len = c.output_length - header_len;

	cr_assert_eq((size_t)declared,
	    actual_body_len,
	    "Content-Length (%ld) must equal actual body bytes (%zu)",
	    declared,
	    actual_body_len);
	free(c.out_buf);
	OKAY_CLEANUP();
}

/* HEAD on a real file: header present, body absent, Content-Length still
   reflects what GET's body would have been. */
Test(build_okay, head_request_file_has_no_body)
{
	RESOLVE_AND_BUILD("/plain.txt", HTTP_HEAD);
	cr_assert_eq(rc, 0);

	const char *body = bounded_find(c.out_buf, c.output_length, "\r\n\r\n");
	cr_assert_not_null(body);
	size_t header_len = (size_t)(body + 4 - c.out_buf);

	cr_assert_eq(c.output_length,
	    header_len,
	    "HEAD response must contain ONLY the header, no body bytes");

	long declared = bounded_content_length(c.out_buf, c.output_length);
	cr_assert_gt(declared,
	    0,
	    "HEAD must still report the real Content-Length");
	free(c.out_buf);
	OKAY_CLEANUP();
}

/* ================================================================== *
 *  UNIT LAYER — directories
 * ================================================================== */

/* Directory with index.html: served transparently as if it were the
   requested resource, NOT as a directory listing. */
Test(build_okay, serves_directory_with_index_transparently)
{
	RESOLVE_AND_BUILD("/withindex", HTTP_GET);
	cr_assert_eq(rc, 0);
	cr_assert_eq(rp.is_dir_listing,
	    0,
	    "a directory with an index.html is NOT a listing");
	cr_assert_not_null(bounded_find(c.out_buf,
			       c.output_length,
			       "index served transparently"),
	    "must serve withindex/index.html's actual content");
	free(c.out_buf);
	OKAY_CLEANUP();
}

/* Directory WITHOUT index.html containing a file AND a subdirectory:
   both entries must appear in the generated listing. */
Test(build_okay, lists_directory_with_file_and_subdirectory)
{
	RESOLVE_AND_BUILD("/nolisting", HTTP_GET);
	cr_assert_eq(rc, 0);
	cr_assert_eq(rp.is_dir_listing,
	    1,
	    "no index.html present -> must be a listing");
	cr_assert_not_null(bounded_find(c.out_buf,
			       c.output_length,
			       "text/HTML"),
	    "listing response must be HTML");

	cr_assert_not_null(bounded_find(c.out_buf,
			       c.output_length,
			       "afile.txt"),
	    "listing must include the file inside the directory");
	cr_assert_not_null(bounded_find(c.out_buf, c.output_length, "subdir"),
	    "listing must include the subdirectory entry");

	/* Dotfiles are excluded — no entry should be exactly "." or "..". */
	cr_assert_null(bounded_find(c.out_buf, c.output_length, ">..<"),
	    "listing must not include the .. parent-dir entry");
	free(c.out_buf);
	OKAY_CLEANUP();
}

/* Listing links must be absolute paths from root, so browser navigation
   works regardless of the current URL (a relative './' link resolves
   incorrectly one level up when clicked from within the browser). */
Test(build_okay, listing_links_are_absolute_paths)
{
	RESOLVE_AND_BUILD("/nolisting", HTTP_GET);
	cr_assert_eq(rc, 0);
	cr_assert_not_null(bounded_find(c.out_buf,
			       c.output_length,
			       "href=\"/nolisting/afile.txt\""),
	    "listing links must be absolute (/nolisting/afile.txt), "
	    "not relative (./afile.txt), so browser navigation resolves "
	    "correctly regardless of current URL depth");
	free(c.out_buf);
	OKAY_CLEANUP();
}

/* HEAD on a directory listing: header only, no generated HTML body, and
   no leak of the internally-generated listing buffer (regression pin —
   this exact case leaked response_body before the fix). */
Test(build_okay, head_request_directory_listing_no_body_no_leak)
{
	RESOLVE_AND_BUILD("/nolisting", HTTP_HEAD);
	cr_assert_eq(rc, 0);

	const char *body = bounded_find(c.out_buf, c.output_length, "\r\n\r\n");
	cr_assert_not_null(body);
	size_t header_len = (size_t)(body + 4 - c.out_buf);
	cr_assert_eq(c.output_length,
	    header_len,
	    "HEAD on a directory must return header only, no listing body");
	/* No explicit leak assertion possible from inside the process, but
	   running this test under ASan/Valgrind is what proves the internal
	   response_body buffer was freed on this path. */
	free(c.out_buf);
	OKAY_CLEANUP();
}

/* ================================================================== *
 *  INTEGRATION LAYER — real spawned process, real sockets, real signal
 * ================================================================== */

/* Path to the server binary under test. Override to point at a specific
   sanitizer build:
     SIMPLE_SERVER_BIN=../simple_server_debug     (compiled-in ASan)
     SIMPLE_SERVER_BIN=../simple_server_valgrind   (plain -g, for use
						     under external valgrind
						     --trace-children=yes)
   Default matches the Makefile's Valgrind-safe binary. */
static const char *
server_binary_path(void)
{
	const char *env = getenv("SIMPLE_SERVER_BIN");
	return env ? env : "../simple_server_valgrind";
}

#define INTEGRATION_PORT "8199"
#define INTEGRATION_STARTUP_RETRIES 20
#define INTEGRATION_STARTUP_DELAY_US 100000 /* 100ms */

/* Poll until the server accepts a connection, or give up. Returns 0 if
   the server came up, -1 on timeout. Avoids a fixed sleep racing against
   however long this particular binary+platform takes to bind & listen. */
static int
wait_for_server_ready(void)
{
	for (int i = 0; i < INTEGRATION_STARTUP_RETRIES; i++) {
		char cmd[256];
		snprintf(cmd,
		    sizeof cmd,
		    "curl -s -o /dev/null --max-time 1 "
		    "http://127.0.0.1:%s/plain.txt",
		    INTEGRATION_PORT);
		if (system(cmd) == 0)
			return 0;
		usleep(INTEGRATION_STARTUP_DELAY_US);
	}
	return -1;
}

/* Fork + exec the real server binary against the fixture directory (the
   caller must already have chdir'd into it via okay_test_setup). Returns
   the child pid, or -1 on fork failure. */
static pid_t
spawn_server(void)
{
	pid_t pid = fork();
	cr_assert(pid >= 0, "fork() failed: %s", strerror(errno));

	if (pid == 0) {
		/* child: exec the real server, verbose (no daemonize), on a
		   dedicated port so this never collides with a dev server
		   someone may already have running on 8080. */
		execl(server_binary_path(),
		    server_binary_path(),
		    "-v",
		    "-p",
		    INTEGRATION_PORT,
		    (char *)NULL);
		/* execl only returns on failure */
		_exit(127);
	}
	return pid;
}

/* SIGTERM, then wait (bounded) for a clean exit. Asserts the process
   actually terminated rather than being left as a zombie or requiring
   SIGKILL — a graceful shutdown that hangs is itself a bug. */
static void
stop_server_gracefully(pid_t pid)
{
	cr_assert_eq(kill(pid, SIGTERM),
	    0,
	    "SIGTERM to server pid %d failed: %s",
	    pid,
	    strerror(errno));

	int status;
	int elapsed_ms = 0;
	const int timeout_ms = 3000;
	pid_t result;

	do {
		result = waitpid(pid, &status, WNOHANG);
		if (result == 0) {
			usleep(50000);
			elapsed_ms += 50;
		}
	} while (result == 0 && elapsed_ms < timeout_ms);

	cr_assert_eq(result,
	    pid,
	    "server did not exit within %dms of SIGTERM "
	    "(graceful shutdown hung or was ignored)",
	    timeout_ms);
	cr_assert(WIFEXITED(status) || WIFSIGNALED(status),
	    "server child left in an unexpected wait status");
}

/*
 * Spawns the real server, requests a plain file, an html file, a
 * transparently-served index directory, and a generated directory
 * listing with a subdirectory — all over a real socket via curl against
 * the real running process — then sends SIGTERM and requires a clean
 * exit within the timeout.
 *
 * This is the test to run under `valgrind --trace-children=yes` (wrapping
 * THIS test binary) or with SIMPLE_SERVER_BIN pointed at simple_server_debug
 * (ASan) to get real memory-safety coverage of the full request pipeline,
 * not just the isolated functions the unit tests above call directly.
 */
Test(build_okay_integration,
    real_server_serves_all_fixture_types,
    .timeout = 15)
{
	pid_t pid = spawn_server();

	int ready = wait_for_server_ready();
	if (ready != 0) {
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
		cr_assert_fail(
		    "server never became ready on port %s "
		    "(binary: %s) — check it exists and is executable",
		    INTEGRATION_PORT,
		    server_binary_path());
	}

	char cmd[512];
	int code;

	/* plain text file */
	snprintf(cmd,
	    sizeof cmd,
	    "test \"$(curl -s -o /dev/null -w '%%{http_code}' "
	    "http://127.0.0.1:%s/plain.txt)\" = 200",
	    INTEGRATION_PORT);
	code = system(cmd);
	cr_assert_eq(code, 0, "GET /plain.txt did not return 200");

	/* html file */
	snprintf(cmd,
	    sizeof cmd,
	    "test \"$(curl -s -o /dev/null -w '%%{http_code}' "
	    "http://127.0.0.1:%s/page.html)\" = 200",
	    INTEGRATION_PORT);
	code = system(cmd);
	cr_assert_eq(code, 0, "GET /page.html did not return 200");

	/* directory with index, served transparently */
	snprintf(cmd,
	    sizeof cmd,
	    "curl -s http://127.0.0.1:%s/withindex | grep -q "
	    "'index served transparently'",
	    INTEGRATION_PORT);
	code = system(cmd);
	cr_assert_eq(code,
	    0,
	    "directory with index.html was not served transparently");

	/* directory without index: generated listing includes file + subdir */
	snprintf(cmd,
	    sizeof cmd,
	    "curl -s http://127.0.0.1:%s/nolisting | grep -q 'afile.txt'",
	    INTEGRATION_PORT);
	code = system(cmd);
	cr_assert_eq(code, 0, "directory listing missing the file entry");

	snprintf(cmd,
	    sizeof cmd,
	    "curl -s http://127.0.0.1:%s/nolisting | grep -q 'subdir'",
	    INTEGRATION_PORT);
	code = system(cmd);
	cr_assert_eq(code,
	    0,
	    "directory listing missing the subdirectory entry");

	/* nested file one level down */
	snprintf(cmd,
	    sizeof cmd,
	    "test \"$(curl -s -o /dev/null -w '%%{http_code}' "
	    "http://127.0.0.1:%s/nolisting/subdir/deep.txt)\" = 200",
	    INTEGRATION_PORT);
	code = system(cmd);
	cr_assert_eq(code,
	    0,
	    "GET on nested subdirectory file did not return 200");

	/* 404 for something that doesn't exist — proves the server is still
	   alive and coherent after the requests above, not just responding
	   to the first one before quietly dying. */
	snprintf(cmd,
	    sizeof cmd,
	    "test \"$(curl -s -o /dev/null -w '%%{http_code}' "
	    "http://127.0.0.1:%s/does-not-exist)\" = 404",
	    INTEGRATION_PORT);
	code = system(cmd);
	cr_assert_eq(code, 0, "GET on missing file did not return 404");

	stop_server_gracefully(pid);
}
