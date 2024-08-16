#ifndef REQUESTS_H
#define REQUESTS_H

#include <stdio.h>

char *parseRequest(const char *req_str, FILE **file_ptr);

int checkMethod(const char *meth_str);

int checkHttp(const char *http_str);

char *getURI(const char *URI);

#endif // REQUESTS_H
