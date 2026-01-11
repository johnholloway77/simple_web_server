#include <string.h>

int checkMethod(const char *meth_str){

    if(strcmp(meth_str, "GET") == 0) return 1;

    //I don't think I'll end up using this method, but it's in the spec
    if(strcmp(meth_str, "HEAD") == 0) return 1;

    if(strcmp(meth_str, "POST") == 0) return 1;

    if(strcmp(meth_str, "DELETE") == 0) return 1;

    //invalid method
    return 0;

}
