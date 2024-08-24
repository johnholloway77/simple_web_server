#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "./response.h"

char *dirResponse(char *uri, int *resp_status) {
    DIR *dp;
    struct dirent *dirp;
    char *response;


    if ((dp = opendir(uri)) == NULL) {
        *resp_status = 500;
        response = (char *)malloc(strlen(RESPONSE_500) + 1);
        if (response != NULL) {
            snprintf(response, strlen(RESPONSE_500) + 1, "%s", RESPONSE_500);
        }
        return response;
    }

    *resp_status = 200;

    const char *header = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n<html><body><h1>Directory: </h1><ul>";
    int resp_len = strlen(header);
    response = (char *)malloc(resp_len + 1);
    if (response == NULL) {
        *resp_status = 500;
        closedir(dp);
        return NULL;
    }
    snprintf(response, resp_len + 1, "%s", header);

    int line_length;
    char buffer[PATH_MAX + 256];
    while ((dirp = readdir(dp)) != NULL) {
        if (dirp->d_name[0] == '.') {
            continue;
        }

        memset(buffer, 0, sizeof(buffer));

        if(uri[strlen(uri) - 1] == '/'){

            line_length = sprintf(buffer, "<li><a href=\"%s\">%s</a></li>",  dirp->d_name, dirp->d_name);
        } else{
            line_length = sprintf(buffer, "<li><a href=\"%s/%s\">%s</a></li>", uri, dirp->d_name, dirp->d_name);
        }


        char *new_response = realloc(response, resp_len + line_length + 1);
        if (new_response == NULL) {
            *resp_status = 500;
            free(response);
            closedir(dp);
            return NULL;
        }
        response = new_response;
        memcpy(response + resp_len, buffer, line_length);

        resp_len += line_length;

    }

    const char *footer = "</ul></body></html>";


    int footer_len = strlen(footer);
    response = realloc(response, resp_len + footer_len + 1);
    if (response == NULL) {
        *resp_status = 500;
        closedir(dp);
        return RESPONSE_500;
    }


    memcpy(response + resp_len, footer, strlen(footer));
    response[resp_len + footer_len] = '\0';

    closedir(dp);
    return response;
}
