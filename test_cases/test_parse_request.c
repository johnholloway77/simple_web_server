/*
 * test_parse_request.c — Criterion suite for the pure request-line parser.
 *
 * Build (FreeBSD, after `pkg install criterion`):
 *   cc -std=c11 -Wall -Wextra -g \
 *      test_parse_request.c parse_request.c \
 *      -lcriterion -o test_parse_request
 *   ./test_parse_request -j1 --verbose
 *
 * Contract under test (see parse_request.h for the full statement):
 *   int parse_request(const char *buf, size_t len, Request *out,
 *                     enum client_response *resp);
 *     0  -> success; *out populated; *resp UNTOUCHED (resolve owns 200/404)
 *    -1  -> failure; *resp set to the specific error; out->path still a
 *           valid C string.
 *   buf is NOT null-terminated; never read at/past buf[len].
 *
 * Groups:
 *   1  happy path                       -> 0
 *   2  malformed request line           -> 400
 *   3  unsupported method               -> 501
 *   4  path traversal                   -> 403
 *   5  bounds / buffer safety
 *   6  regression hooks (add as you go)
 *   7  CS631 conformance cases (ported from the grader's shell suite)
 */

#include <criterion/criterion.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "../requests/parse_request.h"

/* ------------------------------------------------------------------ *
 *  ONE place to change if your signature differs, and ONE place to pin
 *  the over-long-path response code.
 *
 *  OVERLONG_PATH_RESP: you have no RESP_414 in the enum today, so this
 *  defaults to RESP_400. If you add RESP_414 to enum client_response,
 *  change this single line and the relevant tests follow.
 * ------------------------------------------------------------------ */
#define OVERLONG_PATH_RESP RESP_414

#define SETUP()                                                                \
	Request out;                                                           \
	enum client_response resp = NUM_CLIENT_RESP;                           \
	memset(&out, 0, sizeof out)

/* sizeof(lit)-1 strips the implicit NUL so len reflects real wire bytes. */
#define PARSE(literal)                                                         \
	parse_request((literal), sizeof(literal) - 1, &out, &resp)

/* ================================================================== *
 *  GROUP 1 — happy path: well-formed GET requests succeed
 * ================================================================== */

Test(parse_valid, get_root_http11)
{
	SETUP();
	int rc = PARSE("GET / HTTP/1.1\r\n\r\n");
	cr_assert_eq(rc, 0, "valid GET should return 0, got %d", rc);
	cr_assert_eq(out.method, HTTP_GET);
	cr_assert_eq(out.version, HTTP_1_1);
	cr_assert_str_eq(out.path, "/");
	cr_assert_eq(resp,
	    NUM_CLIENT_RESP,
	    "success must NOT touch resp (resolve owns 200)");
}

Test(parse_valid, get_root_http10)
{
	SETUP();
	int rc = PARSE("GET / HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, 0);
	cr_assert_eq(out.version, HTTP_1_0);
}

Test(parse_valid, deep_path_preserved)
{
	SETUP();
	int rc = PARSE("GET /a/b/c/file.css HTTP/1.1\r\n\r\n");
	cr_assert_eq(rc, 0);
	cr_assert_str_eq(out.path, "/a/b/c/file.css");
}

/* Secretly a tokenizer-boundary test: if the scanner swallows the trailing
   \r into the version token, version_from_token sees "HTTP/1.1\r" (len 9)
   and wrongly fails. Green here means the token ends at \r, not after it. */
Test(parse_valid, version_token_excludes_cr)
{
	SETUP();
	int rc = PARSE("GET / HTTP/1.1\r\n\r\n");
	cr_assert_eq(rc, 0);
	cr_assert_eq(out.version, HTTP_1_1);
}

/* DECISION PIN: query string kept verbatim in path. If you strip it at
   parse time instead, change the expected path. The point is to pin it so
   resolve isn't surprised by a '?' it didn't expect. */
Test(parse_valid, query_string_kept_verbatim)
{
	SETUP();
	int rc = PARSE("GET /search?q=freebsd HTTP/1.1\r\n\r\n");
	cr_assert_eq(rc, 0);
	cr_assert_str_eq(out.path, "/search?q=freebsd");
}

/* ================================================================== *
 *  GROUP 2 — malformed request line -> 400 Bad Request
 *  (structural failure: wrong token count, bad version syntax, no CRLF)
 * ================================================================== */

Test(parse_400, empty_buffer)
{
	SETUP();
	int rc = parse_request("", 0, &out, &resp);
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

Test(parse_400, only_method)
{
	SETUP();
	int rc = PARSE("GET\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400, "one token is not a request line");
}

Test(parse_400, method_and_uri_no_version)
{
	SETUP();
	int rc = PARSE("GET /\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

Test(parse_400, four_tokens)
{
	SETUP();
	int rc = PARSE("GET / HTTP/1.1 extra\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400, "trailing token");
}

Test(parse_400, no_crlf_within_len)
{
	SETUP();
	int rc = PARSE("GET / HTTP/1.1");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

/* DECISION PIN: empty method via leading space. CS631 harness sends
   "  / HTTP/1.0". Empty method token is STRUCTURAL (400), distinct from a
   present-but-unsupported method (501 in Group 3). Keep them separate. */
Test(parse_400, empty_method_double_leading_space)
{
	SETUP();
	int rc = PARSE("  / HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

/* ================================================================== *
 *  GROUP 3 — unsupported method -> 501 Not Implemented
 *  Contract: GET is the ONLY method that proceeds. Everything else that
 *  occupies a well-formed method slot -> 501. POST may map to HTTP_POST
 *  in the enum for the future, but the externally visible behavior is 501,
 *  so we assert on `resp`, not on the enum.
 * ================================================================== */

Test(parse_501, post_not_implemented)
{
	SETUP();
	int rc = PARSE("POST / HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_501, "recognized but unimplemented -> 501");
}

Test(parse_501, delete_not_implemented)
{
	SETUP();
	int rc = PARSE("DELETE / HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_501);
}

/* DECISION PIN: BACON occupies the method slot, well-formed request line.
   Treated as unimplemented (501) rather than malformed (400). If the
   grader's golden output says 400, flip this — but pin it consciously. */
Test(parse_501, unknown_method_bacon)
{
	SETUP();
	int rc = PARSE("BACON / HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_501);
}

Test(parse_501, method_case_sensitive)
{
	SETUP();
	int rc = PARSE("get / HTTP/1.1\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_501);
}

/* ================================================================== *
 *  GROUP 4 — path traversal -> 403 Forbidden
 *  Contract (matches CS631): ANY ".." path segment is rejected outright.
 *  NO normalization — even a ".." that would resolve to a legal path is
 *  forbidden. Fast string-level reject; the real boundary is realpath
 *  containment at resolve time.
 * ================================================================== */

Test(parse_403, dotdot_leading)
{
	SETUP();
	int rc = PARSE("GET /../../etc/passwd HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_403);
}

Test(parse_403, dotdot_deep_traversal)
{
	SETUP();
	int rc = PARSE(
	    "GET /../../../../../../../../etc/passwd HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_403);
}

/* CS631 case: /dir/file/../../dir/file would NORMALIZE to a legal path, but
   the spec rejects any ".." outright. 403, no normalization. */
Test(parse_403, dotdot_that_would_normalize_legal)
{
	SETUP();
	int rc = PARSE("GET /dir/file/../../dir/file HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_403);
}

Test(parse_403, dotdot_in_user_dir)
{
	SETUP();
	int rc = PARSE("GET /~jschauma/../../../ HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_403);
}

/* Guards against an over-eager strstr(path, ".."). A single dot segment and
   ".." inside a filename are NOT traversals and must pass parse. */
Test(parse_valid, single_dot_segment_is_fine)
{
	SETUP();
	int rc = PARSE("GET /dir/./file HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, 0, "single-dot segment is not traversal");
}

Test(parse_valid, dotdot_inside_filename_is_fine)
{
	SETUP();
	int rc = PARSE("GET /my..file.txt HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc,
	    0,
	    "'..' inside a filename is not a traversal segment");
}

/* ================================================================== *
 *  GROUP 5 — bounds / buffer safety
 * ================================================================== */

/* DECISION PIN: path must begin with '/'. CS631 sends "no-leading-slash".
   Spec treats it as malformed -> 400. */
Test(parse_400, path_must_have_leading_slash)
{
	SETUP();
	int rc = PARSE("GET no-leading-slash HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

Test(parse_bounds, path_just_under_path_max)
{
	SETUP();
	char buf[PATH_MAX + 64];
	int prefix = snprintf(buf, sizeof buf, "GET /");
	int room = PATH_MAX - 2; /* '/' already written + NUL */
	memset(buf + prefix, 'a', room);
	int after = prefix + room;
	int tail =
	    snprintf(buf + after, sizeof buf - after, " HTTP/1.0\r\n\r\n");
	int rc = parse_request(buf, (size_t)(after + tail), &out, &resp);
	cr_assert_eq(rc, 0);
	cr_assert(strlen(out.path) < PATH_MAX, "path must fit with NUL");
}

Test(parse_bounds, path_overflows_path_max)
{
	SETUP();
	char buf[PATH_MAX * 2];
	int prefix = snprintf(buf, sizeof buf, "GET /");
	int huge = PATH_MAX + 100;
	memset(buf + prefix, 'a', huge);
	int after = prefix + huge;
	int tail =
	    snprintf(buf + after, sizeof buf - after, " HTTP/1.0\r\n\r\n");
	int rc = parse_request(buf, (size_t)(after + tail), &out, &resp);
	cr_assert_eq(rc, -1, "over-long path rejected, not silently truncated");
	cr_assert_eq(resp, OVERLONG_PATH_RESP);
	cr_assert(strnlen(out.path, PATH_MAX) < PATH_MAX,
	    "out.path must remain a valid C string even on the error path");
}

Test(parse_bounds, no_read_past_len)
{
	SETUP();
	const char full[] = "GET / HTTP/1.1\r\n\r\nGARBAGE";
	size_t lie = 10; /* "GET / HTTP" — truncated mid-version, no CRLF */
	int rc = parse_request(full, lie, &out, &resp);
	cr_assert_eq(rc, -1, "must not see bytes past len");
}

Test(parse_bounds, embedded_nul_before_crlf)
{
	SETUP();
	const char raw[] = "GET /a\0b HTTP/1.1\r\n\r\n";
	int rc = parse_request(raw, sizeof raw - 1, &out, &resp);
	/* A length-aware parser keeps going past the NUL; a strchr/strcmp-based
	   one stops early. Just require a definite reject and no crash. */
	cr_assert_eq(rc, -1);
}

/* ================================================================== *
 *  GROUP 6 — regression hooks
 *  TDD habit: every bug you fix gets a failing test here FIRST, then green.
 * ================================================================== */

/* Test(parse_regression, describe_the_bug) { ... } */

/* ================================================================== *
 *  GROUP 7 — CS631 conformance cases (from the grader's shell harness)
 *  These mirror the integration suite at the unit level so green-locally
 *  predicts green-on-the-grader, without needing a running server.
 * ================================================================== */

/* --- PROTOCOLS block: version classification --- */

Test(cs631_proto, http_0_9_unsupported)
{
	SETUP();
	int rc = PARSE("GET / HTTP/0.9\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_505);
}

Test(cs631_proto, http_2_0_unsupported)
{
	SETUP();
	int rc = PARSE("GET / HTTP/2.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_505);
}

/* The length-guard payoff: "HTTP/1.0000" is NOT "HTTP/1.0" + junk. A naive
   strncmp(tok,"HTTP/1.0",8) wrongly ACCEPTS it; exact length+bytes rejects. */
Test(cs631_proto, http_1_0000_trailing_junk_rejected)
{
	SETUP();
	int rc = PARSE("GET / HTTP/1.0000\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
	cr_assert_neq(out.version,
	    HTTP_1_0,
	    "must not match HTTP/1.0 by prefix");
}

Test(cs631_proto, bare_http_no_version)
{
	SETUP();
	int rc = PARSE("GET / HTTP\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

Test(cs631_proto, bacon_protocol)
{
	SETUP();
	int rc = PARSE("GET / BACON\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

Test(cs631_proto, bacon_versioned_protocol)
{
	SETUP();
	int rc = PARSE("GET / BACON/3.2\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

/* Absurdly long version token must reject cleanly, never overrun. */
Test(cs631_proto, version_token_absurdly_long)
{
	SETUP();
	char buf[256];
	int prefix = snprintf(buf, sizeof buf, "GET / HTTP/1.");
	int zeros = 200;
	memset(buf + prefix, '0', zeros);
	int after = prefix + zeros;
	int tail = snprintf(buf + after, sizeof buf - after, "\r\n\r\n");
	int rc = parse_request(buf, (size_t)(after + tail), &out, &resp);
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

/* --- REQUESTS block: method classification (mirrors Group 3) --- */

// Updated to reflect that it is now implemented
Test(cs631_method, head_via_harness)
{
	SETUP();
	int rc = PARSE("HEAD / HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, 0, "HEAD is now implemented");
	cr_assert_eq(out.method, HTTP_HEAD);
}

Test(cs631_method, delete_via_harness)
{
	SETUP();
	int rc = PARSE("DELETE / HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_501);
}

/* --- URIS block: the parser-layer subset (the rest are resolve cases) --- */

Test(cs631_uri, no_leading_slash)
{
	SETUP();
	int rc = PARSE("GET no-leading-slash HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_400);
}

Test(cs631_uri, etc_passwd_traversal)
{
	SETUP();
	int rc = PARSE(
	    "GET /../../../../../../../../../etc/passwd HTTP/1.0\r\n\r\n");
	cr_assert_eq(rc, -1);
	cr_assert_eq(resp, RESP_403);
}

/* The 512-deep nested path from the harness: a length stress on the path
   copy. Built programmatically to match the harness's /1/2/.../512/ shape.
   Whether this 404s (fits) or over-long-rejects depends on PATH_MAX vs the
   built length; we assert it does ONE of: parses cleanly OR rejects with the
   over-long code — never crashes, never silently truncates into a different
   valid path. */
Test(cs631_uri, deeply_nested_path_safe)
{
	SETUP();
	char buf[8192];
	int n = snprintf(buf, sizeof buf, "GET /");
	for (int seg = 1; seg <= 512 && n < (int)sizeof buf - 32; seg++)
		n += snprintf(buf + n, sizeof buf - n, "%d/", seg);
	int tail = snprintf(buf + n, sizeof buf - n, " HTTP/1.0\r\n\r\n");
	int rc = parse_request(buf, (size_t)(n + tail), &out, &resp);
	if (rc == 0) {
		cr_assert(strlen(out.path) < PATH_MAX);
		cr_assert_eq(out.path[0], '/');
	}
	else {
		cr_assert_eq(resp, OVERLONG_PATH_RESP);
		cr_assert(strnlen(out.path, PATH_MAX) < PATH_MAX);
	}
}

/*
 * NOTE — these CS631 URIs are RESOLVE-layer cases, NOT parse cases. They
 * test filesystem behavior (existence, dir vs file, ~user expansion,
 * %-decoding) and belong in a future test_resolve suite once build_response
 * exists. Listed here so they aren't forgotten:
 *
 *   /cgi-bin/env.cgi ...            -> CGI deferred -> 501 at resolve
 *   /testdir/dir, /dir/, /dir/.     -> directory handling (index then list)
 *   /testdir/file space            -> filename with space (exists? 200/404)
 *   /testdir/file%20space%3C...    -> %-decoding decision (CS631: likely
 * literal)
 *   /~jschauma/, /~nobody          -> ~user expansion feature (scope it!)
 *   /passwd, /nowhere              -> 404
 *
 * The parser passes all of these through as a valid path (they have a
 * leading slash and no ".." segment); the response code is resolve's call.
 */
