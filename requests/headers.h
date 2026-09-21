#pragma  once

#include <stdlib.h>

typedef enum Header_type {
    H_HOST,
    H_USER_AGENT,
    H_ACCEPT,
    H_ACCEPT_LANG,
    H_ACCEPT_ENC,
    H_CONTENT_LEN,
    H_CONTENT_TYPE,
    H_CONNECTION,
    H_ERROR
} Header_type;

typedef struct Slice{
    const char * start;
    size_t length;
} Slice;
