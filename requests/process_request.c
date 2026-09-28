#include "./parse_request.h"
#include "headers.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static enum http_method
method_from_token(Slice method)
{
	if (0 == method.length) {
		return HTTP_METHOD_UNKNOWN;
	}

	if (memcmp(method.start, "GET", method.length) == 0) {
	    return HTTP_GET;
	}
	if (memcmp(method.start, "POST", method.length) == 0) {
	    return HTTP_POST;
	}
	if (memcmp(method.start, "HEAD", method.length) == 0) {
	    return HTTP_HEAD;
	}
	if (memcmp(method.start, "PUT", method.length) == 0) {
	    return HTTP_PUT;
	}
	if (memcmp(method.start, "DELETE", method.length) == 0) {
	    return HTTP_DELETE;
	}


	return HTTP_METHOD_UNKNOWN; // use for junk/incorrect
};

static enum http_version
version_from_token(Slice version){
    if (0 == version.length) {
		return HTTP_VERSION_UNKNOWN;
	}

    if (memcmp(version.start, "HTTP/1.0", version.length) == 0){
        return HTTP_1_0;
    }
    if (memcmp(version.start, "HTTP/1.1", version.length) == 0){
        return HTTP_1_1;
    }
    if (memcmp(version.start, "HTTP/0.9", version.length) == 0){
        return HTTP_0_9;
    }
    if (memcmp(version.start, "HTTP/2.0", version.length) == 0){
        return HTTP_2;
    }
    if (memcmp(version.start, "HTTP/3.0", version.length) == 0){
        return HTTP_3;
    }

    return HTTP_VERSION_UNKNOWN; // use for junk/incorrect
}

static int
path_has_traversal(Slice path)
{
	return strnstr(path.start, "../", path.length) ? 1 : 0;
}

static size_t slice_to_size_t(Slice s, char **endptr){

    size_t num = 0;

    (*endptr) = s.start;

    for (size_t i = 0; i < s.length; i++){
        if (!isdigit(s.start[i])){
            fprintf(stderr, "slice to sizet: slice isn't valid\n");
            (*endptr) = s.start;
            return 0;
        }

        num += num * 10 + (s.start[i] - '0') ;

        (*endptr)++;
    }

    return num;
}

int process_request(Client *client) {
    Client *c = client;

    if (c == NULL){
        return 1;
    }

    c->http_method = method_from_token(c->headers.request_fields.method);

    c->http_version = version_from_token(c->headers.request_fields.version);

    if (c->http_method == HTTP_POST || c->http_method == HTTP_PUT){
        const char* start = c->headers.content_length.start;
        char *endptr = NULL;

        /*
            skip whitespaces and then check for a leading negative to prevent interer wrap around
        */
        while((*start == ' ' || *start == '\t') && start <= c->headers.content_length.start + c->headers.content_length.length){
            start++;
        }
        if (*start != '-'){
            c->content_length = slice_to_size_t(c->headers.content_length, &endptr);

            //check for malformed string...
            if (endptr == c->headers.content_length.start){
                fprintf(stderr, "invalid content-length string.\n");
            }
        } else {
            fprintf(stderr, "content length is a negative number\n");
            c->content_length = 0;
        }


    }

    return 0;
}
