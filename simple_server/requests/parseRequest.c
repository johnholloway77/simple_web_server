#include <limits.h>
#include <magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../flags/flags.h"
#include "../cgi/cgi.h"
#include "../response/dirResponse.h"
#include "../response/response.h"
#include "./requests.h"

extern uint32_t app_flags;

char *parseRequest(const char *req_str, FILE **file_ptr, int *resp_status) {
  char *str;

  str = strdup(req_str);

  if (!str) {
    *resp_status = 500;
    RETURN_RESP(RESPONSE_500)
  }

  char *method = strtok(str, " ");
  char *URI = strtok(NULL, " ");
  char *http = strtok(NULL, " ");

  // check that the request header was properly parsed
  if (method && URI && http) {

  } else {
    free(str);
    *resp_status = 400;
    RETURN_RESP(RESPONSE_400)
  }

  /*
   * check if it's a valid method
   * We're only doing GET requests for this simple project
   */
  if (checkMethod(method) == 0) {
    free(str);
    *resp_status = 400;
    RETURN_RESP(RESPONSE_400)
  }

  /*
   * Check that it's either HTML1.0 or 1.1
   */
  if (checkHttp(http) == 0) {
    free(str);
    *resp_status = 400;
    RETURN_RESP(RESPONSE_400)
  }

  // should now get index by default
  if (strcmp(URI, "/") == 0) {
    *file_ptr = fopen("index.html", "r");
  } else if (strncmp(URI, "/cgi-bin/", 9) == 0) {
      if(app_flags & C_FLAG){
          char *cgi_URI =
              strdup(URI + 9); // get the first part of /cgi-bin/someExeFile
          cgi_URI = strtok(cgi_URI, "/"); // get the exec name;

          if (cgi_URI == NULL || strcmp(cgi_URI, "") == 0) {
              free(cgi_URI); // Free allocated memory before returning error response

              free(str);
              *resp_status = 400;
              RETURN_RESP(RESPONSE_400)
          }

          char *cgi_argv[] = {URI + 1}; // pass directory path to

          char *response = cgiExe(cgi_URI, 1, cgi_argv, resp_status);
          free(cgi_URI);
          free(str);

          return response;
      }else{
          free(str);

          *resp_status = 404;
          RETURN_RESP(RESPONSE_404)
      }

  } else {
    // //check if file points to a directory
    // //if directory, call cgi script
    struct stat stat1;

    if (lstat(URI + 1, &stat1) != 0) {
      /*
       * will actuall work to check if file exists
       * returns 404 if not
       */
      free(str);

      *resp_status = 404;
      RETURN_RESP(RESPONSE_404)
    }

    if (S_ISDIR(stat1.st_mode)) {

      char index_path[PATH_MAX];
      snprintf(index_path, PATH_MAX, "%sindex.html", URI + 1);

      *file_ptr = fopen(index_path, "r");

      if (*file_ptr == NULL) {


       // printf("calling dirResponse(%s)\n", URI +1);

        char *response = dirResponse(URI +1, resp_status);
        free(str);
        return response;
      }

    } else {
      // file is regular file
      *file_ptr = fopen(URI + 1, "r");
    }
  }

  free(str);

  if (*file_ptr) {

    magic_t magic = magic_open(MAGIC_MIME_TYPE);
    int file_des = fileno(*file_ptr);

    if (magic_load(magic, NULL) != 0) {
      magic_close(magic);

      *resp_status = 500;
      RETURN_RESP(RESPONSE_500)
    }

    const char *file_type = magic_descriptor(magic, file_des);

    char *header_buf = (char *)malloc(HEADER_BUF_SIZE);

    snprintf(header_buf, HEADER_BUF_SIZE,
             "HTTP/1.0 200 Ok\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",
             file_type);
    // printf("\t%s\n", header_buf);

    magic_close(magic);

    *resp_status = 200;
    return header_buf;

  } else {
    // this code is not reached. could delete it if need be...
    // if nothing found
    // printf("File not found/n");

    *resp_status = 404;
    RETURN_RESP(RESPONSE_404)
  }
}
