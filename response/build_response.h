#include <magic.h>
#include "../client_conn/connections.h"
#include "../requests/resolve_path.h"
#include "../requests/parse_request.h"

/**
 * @brief Build a 200 OK response and stage it in the client's output buffer.
 *
 * Dispatches to one of three internal builders based on the resolved resource:
 *   - CGI:           build_response_cgi()   (not yet implemented; exits)
 *   - Directory:     build_response_dir()   (generates an HTML listing)
 *   - Regular file:  build_response_file()  (serves file contents)
 *
 * For HEAD requests, only the response headers are written into c->out_buf
 * (no body).  For GET requests, both headers and body are written.
 *
 * On success, c->out_buf is heap-allocated, c->output_length is set to the
 * total bytes to send, and 0 is returned.
 *
 * @param c    Client whose out_buf will be populated
 * @param rp   Resolved path describing the resource to serve
 * @param req  Parsed request (used for method, timestamps)
 * @return     0 on success, -1 on error (c->out_buf may be NULL)
 */
int build_okay_response(Client *c, ResolvedPath *rp, Request *req);

/**
 * @brief Build an HTTP error response and stage it in the client's output
 * buffer.
 *
 * Looks up c->resp_val in the static error_responses table to obtain the
 * status line, MIME type, and optional HTML body, then formats a complete
 * HTTP/1.0 response into a newly allocated c->out_buf.
 *
 * For HEAD requests, the body is omitted from c->out_buf (headers only).
 * For all other methods the body is appended.
 *
 * c->resp_val must be in the range [RESP_400, NUM_CLIENT_RESP).  Values
 * outside that range (including RESP_200) are rejected and -1 is returned.
 *
 * On success, c->out_buf is heap-allocated, c->header_len and
 * c->output_length are set, and 0 is returned.
 *
 * @param c    Client whose out_buf will be populated; resp_val must be set
 * @param req  Parsed request (used for method and timestamp)
 * @return     0 on success, -1 on invalid arguments or allocation failure
 */
int build_error_response(Client *c, Request *req);
