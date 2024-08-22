#ifndef REQUESTS_H
#define REQUESTS_H

#include <stdio.h>

#define HEADER_BUF_SIZE 256
#define DIR_LIST_CGI_PATH "directoryList.cgi"

char *parseRequest(const char *req_str, FILE **file_ptr, int *resp_status);

int checkMethod(const char *meth_str);

int checkHttp(const char *http_str);

char *getURI(const char *URI);

#endif // REQUESTS_H
