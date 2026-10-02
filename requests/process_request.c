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

	if (method.length == 3 && (memcmp(method.start, "GET", method.length) == 0)) {
	    return HTTP_GET;
	}
	if (method.length == 4 && (memcmp(method.start, "POST", method.length) == 0)) {
	    return HTTP_POST;
	}
	if (method.length == 4 && (memcmp(method.start, "HEAD", method.length) == 0)) {
	    return HTTP_HEAD;
	}
	if (method.length == 3 && (memcmp(method.start, "PUT", method.length) == 0)) {
	    return HTTP_PUT;
	}
	if (method.length == 6 && (memcmp(method.start, "DELETE", method.length) == 0)) {
	    return HTTP_DELETE;
	}


	return HTTP_METHOD_UNKNOWN; // use for junk/incorrect
};

static enum http_version
version_from_token(Slice version){
    if (0 == version.length) {
		return HTTP_VERSION_UNKNOWN;
	}

    if (version.length == 8 && (memcmp(version.start, "HTTP/1.0", version.length) == 0)){
        return HTTP_1_0;
    }
    if (version.length == 8 && (memcmp(version.start, "HTTP/1.1", version.length) == 0)){
        return HTTP_1_1;
    }
    if (version.length == 8 && (memcmp(version.start, "HTTP/0.9", version.length) == 0)){
        return HTTP_0_9;
    }
    if (version.length == 8 && (memcmp(version.start, "HTTP/2.0", version.length) == 0)){
        return HTTP_2;
    }
    if (version.length == 8 && (memcmp(version.start, "HTTP/3.0", version.length) == 0)){
        return HTTP_3;
    }

    return HTTP_VERSION_UNKNOWN; // use for junk/incorrect
}

static int
path_has_traversal(Slice path)
{
	return strnstr(path.start, "../", path.length) ? 1 : 0;
}

static size_t slice_to_size_t(Slice s, const char **endptr){


    size_t num = 0;

    (*endptr) = s.start;

    for (size_t i = 0; i < s.length; i++){
        if (!isdigit((unsigned char)s.start[i])){
            fprintf(stderr, "slice to sizet: slice isn't valid\n");
            (*endptr) = s.start;
            return 0;
        }

        num = num * 10 + (s.start[i] - '0') ;

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
        Slice s = c->headers.content_length;
        const char *endptr = NULL;

        if (s.start != NULL) {/*
            skip whitespaces and then check for a leading negative to prevent interer wrap around
        */
        while((s.length) && (*s.start == ' ' || *s.start == '\t') ){
            s.start++;
            s.length--;
        }

        if (s.length == 0){
            fprintf(stderr, "content lenght string is all whitespace\n");
            c->content_length = 0;
        }else if (*s.start != '-'){
            c->content_length = slice_to_size_t(s, &endptr);

            //check for malformed string...
            if (endptr == s.start){
                fprintf(stderr, "invalid content-length string.\n");
            }
        } else {
            fprintf(stderr, "content length is a negative number\n");
            c->content_length = 0;
        }

        } else {
            fprintf(stderr, "content length slice is null\n");
            c->content_length = 0;
        }
    }

    return 0;
}
