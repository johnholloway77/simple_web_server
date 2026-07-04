/*
 * test_build_error.c — Criterion unit tests for build_error_response().
 *
 * Links against response/build_response.c (the unit under test) and whatever
 * production files it needs. Does NOT link main.c.
 *
 * Coverage:
 *   - each canned error response builds successfully
 *   - the response is well-formed HTTP/1.0 (status line, CRLF headers, blank
 *     line, body)
 *   - Content-Length in the header matches the ACTUAL body byte count
 *   - output_length matches the real response length (what the sender writes)
 *   - the required headers are present (Content-Type, Content-Length,
 *     Connection: close)
 *   - edge cases: NULL client, out-of-range resp_val (OK range and >= sentinel)
 *   - out_buf is left NULL / untouched on the error paths that should not
 *     allocate
 */

#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../client_conn/connections.h"

int build_error_response(Client *c);

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/*
 * Zero a Client and set a response value. Returns a heap Client the caller
 * must free() (along with out_buf if allocated). Using a fresh Client per
 * test keeps state from leaking between cases.
 */
static Client *
make_client(int resp_val)
{
	Client *c = calloc(1, sizeof(*c));
	cr_assert_not_null(c, "test setup: calloc failed");
	c->resp_val = resp_val;
	c->out_buf = NULL;
	c->output_length = 0;
	return (c);
}

static void
free_client(Client *c)
{
	if (c == NULL)
		return;
	free(c->out_buf);
	free(c);
}

/*
 * Extract the numeric value of the Content-Length header from a response
 * buffer. Returns -1 if not found. Simple, test-only parser.
 */
static long
parse_content_length(const char *resp)
{
	const char *p = strstr(resp, "Content-Length:");
	if (p == NULL)
		return (-1);
	p += strlen("Content-Length:");
	return (strtol(p, NULL, 10));
}

/*
 * Return a pointer to the start of the body (first byte after the
 * "\r\n\r\n" header terminator), or NULL if the terminator is absent.
 */
static const char *
body_start(const char *resp)
{
	const char *sep = strstr(resp, "\r\n\r\n");
	if (sep == NULL)
		return (NULL);
	return (sep + 4);
}

/*
 * Shared assertions every well-formed response must satisfy. `expect_status`
 * is the substring expected in the status line, e.g. "404 Not Found".
 */
static void
assert_well_formed(Client *c, int rc, const char *expect_status)
{
	cr_assert_eq(rc, 0, "build_error_response should succeed");
	cr_assert_not_null(c->out_buf, "out_buf must be allocated on success");

	/* Status line: starts with HTTP/1.0 and contains the expected code. */
	cr_assert(strncmp(c->out_buf, "HTTP/1.0 ", 9) == 0,
	    "response must start with HTTP/1.0");
	cr_assert_not_null(strstr(c->out_buf, expect_status),
	    "status line must contain '%s'",
	    expect_status);

	/* Required headers present. */
	cr_assert_not_null(strstr(c->out_buf, "Content-Type:"),
	    "Content-Type header required");
	cr_assert_not_null(strstr(c->out_buf, "Content-Length:"),
	    "Content-Length header required");
	cr_assert_not_null(strstr(c->out_buf, "Connection: close"),
	    "Connection: close header required");

	/* Header/body separator present. */
	const char *body = body_start(c->out_buf);
	cr_assert_not_null(body, "response must contain CRLF CRLF separator");

	/* Content-Length must equal the actual body length. This is the
	 * correctness property: a lying Content-Length hangs clients. */
	long declared = parse_content_length(c->out_buf);
	cr_assert_geq(declared, 0, "Content-Length must be parseable");
	cr_assert_eq((size_t)declared,
	    strlen(body),
	    "Content-Length (%ld) must equal actual body length (%zu)",
	    declared,
	    strlen(body));

	/* output_length must equal the real response length the sender writes.
	 * strlen is valid here because canned responses contain no NUL bytes.
	 */
	cr_assert_eq((size_t)c->output_length,
	    strlen(c->out_buf),
	    "output_length (%zu) must match response byte length (%zu)",
	    c->output_length,
	    strlen(c->out_buf));
}

/* ------------------------------------------------------------------ */
/* One test per canned response                                       */
/* ------------------------------------------------------------------ */

Test(build_error, resp_400)
{
	Client *c = make_client(RESP_400);
	int rc = build_error_response(c);
	assert_well_formed(c, rc, "400 Bad Request");
	free_client(c);
}

Test(build_error, resp_403)
{
	Client *c = make_client(RESP_403);
	int rc = build_error_response(c);
	assert_well_formed(c, rc, "403 Forbidden");
	free_client(c);
}

Test(build_error, resp_404)
{
	Client *c = make_client(RESP_404);
	int rc = build_error_response(c);
	assert_well_formed(c, rc, "404 Not Found");
	/* 404 has an HTML body — verify it actually arrived. */
	cr_assert_not_null(strstr(c->out_buf, "<html>"),
	    "404 body should contain HTML");
	cr_assert_not_null(strstr(c->out_buf, "text/html"),
	    "404 should be text/html");
	free_client(c);
}

Test(build_error, resp_414)
{
	Client *c = make_client(RESP_414);
	int rc = build_error_response(c);
	assert_well_formed(c, rc, "414 URI Too Long");
	free_client(c);
}

Test(build_error, resp_500)
{
	Client *c = make_client(RESP_500);
	int rc = build_error_response(c);
	assert_well_formed(c, rc, "500 Internal Server Error");
	free_client(c);
}

Test(build_error, resp_501)
{
	Client *c = make_client(RESP_501);
	int rc = build_error_response(c);
	assert_well_formed(c, rc, "501 Not Implemented");
	cr_assert_not_null(strstr(c->out_buf, "<html>"),
	    "501 body should contain HTML");
	cr_assert_not_null(strstr(c->out_buf, "cgi-bin"),
	    "501 body should mention cgi-bin");
	free_client(c);
}

Test(build_error, resp_505)
{
	Client *c = make_client(RESP_505);
	int rc = build_error_response(c);
	assert_well_formed(c, rc, "505 HTTP Version Not Supported");
	free_client(c);
}

/* ------------------------------------------------------------------ */
/* Edge cases / failure paths                                         */
/* ------------------------------------------------------------------ */

/* NULL client must return -1, not crash, not exit. */
Test(build_error, null_client_returns_error)
{
	int rc = build_error_response(NULL);
	cr_assert_eq(rc, -1, "NULL client must return -1");
}

/* resp_val in the OK range (< RESP_400) must be rejected. */
Test(build_error, ok_range_rejected)
{
	Client *c = make_client(RESP_200);
	int rc = build_error_response(c);
	cr_assert_eq(rc, -1, "RESP_200 (OK range) must return -1");
	cr_assert_null(c->out_buf,
	    "out_buf must not be allocated on the reject path");
	free_client(c);
}

/* resp_val == sentinel (NUM_CLIENT_RESP) must be rejected as out of range. */
Test(build_error, sentinel_rejected)
{
	Client *c = make_client(NUM_CLIENT_RESP);
	int rc = build_error_response(c);
	cr_assert_eq(rc, -1, "NUM_CLIENT_RESP sentinel must return -1");
	cr_assert_null(c->out_buf, "out_buf must not be allocated");
	free_client(c);
}

/* resp_val beyond the sentinel (garbage) must be rejected. */
Test(build_error, above_sentinel_rejected)
{
	Client *c = make_client(NUM_CLIENT_RESP + 5);
	int rc = build_error_response(c);
	cr_assert_eq(rc, -1, "resp_val past sentinel must return -1");
	cr_assert_null(c->out_buf, "out_buf must not be allocated");
	free_client(c);
}

/* Negative resp_val must be rejected (also exercises the < RESP_400 bound). */
Test(build_error, negative_resp_val_rejected)
{
	Client *c = make_client(-1);
	int rc = build_error_response(c);
	cr_assert_eq(rc, -1, "negative resp_val must return -1");
	cr_assert_null(c->out_buf, "out_buf must not be allocated");
	free_client(c);
}
