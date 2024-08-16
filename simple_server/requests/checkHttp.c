#include <string.h>

int checkHttp(const char *http_str)
{

    if(strcmp(http_str, "HTTP/1.0") == 0) return 1;

    if(strcmp(http_str, "HTTP/1.1") == 0) return 1;

    return 0;

}
