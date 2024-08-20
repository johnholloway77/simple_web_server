#include <limits.h>
#include <magic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../cgi/cgi.h"
#include "./requests.h"

#define HEADER_BUF_SIZE 256
#define DIR_LIST_CGI_PATH "directoryList.cgi"

char *parseRequest(const char *req_str, FILE **file_ptr) {
  char *str;
  str = strdup(req_str);

  if (!str) {
    return "HTTP/1.0 500 Internal Error\r\n"
           "Content-Length: 0\r\n"
           "Connection: close\r\n\r\n";
  }

  char *method = strtok(str, " ");
  char *URI = strtok(NULL, " ");
  char *http = strtok(NULL, " ");

  // check that the request header was properly parsed
  if (method && URI && http) {
    printf("Method: '%s'\n", method);
    printf("URI: '%s'\n", URI);
    printf("http: '%s'\n", http);
  } else {
    free(str);
    printf("invalid request");
    return "HTTP/1.0 400 Bad Request\r\n"
           "Content-Length: 0\r\n"
           "Connection: close\r\n\r\n";
  }

  /*
   * check if it's a valid method
   * We're only doing GET requests for this simple project
   */
  if (checkMethod(method) == 0) {
    free(str);
    return "HTTP/1.0 400 Bad Request\r\n"
           "Content-Length: 0\r\n"
           "Connection: close\r\n\r\n";
  }

  /*
   * Check that it's either HTML1.0 or 1.1
   */
  if (checkHttp(http) == 0) {
    free(str);
    return "HTTP/1.0 400 Bad Request\r\n"
           "Content-Length: 0\r\n"
           "Connection: close\r\n\r\n";
  }

  // should now get index by default
  if (strcmp(URI, "/") == 0) {
    *file_ptr = fopen("index.html", "r");
  } else if (strncmp(URI, "/cgi-bin/", 9) == 0) {
    char *cgi_URI =
        strdup(URI + 9); // get the first part of /cgi-bin/someExeFile
    cgi_URI = strtok(cgi_URI, "/"); // get the exec name;

    free(cgi_URI);

    char *cgi_argv[] = {URI + 1};

    return cgiExe(cgi_URI, 1, cgi_argv);
  } else {
    // //check if file points to a directory
    // //if directory, call cgi script
    struct stat stat1;

    if (lstat(URI + 1, &stat1) != 0) {
      return "HTTP/1.0 500 Internal Error\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n\r\n";
    }

    if (S_ISDIR(stat1.st_mode)) {
      printf("%s file is a directory!\n", URI);

      char index_path[PATH_MAX];
      snprintf(index_path, PATH_MAX, "%sindex.html", URI + 1);

      *file_ptr = fopen(index_path, "r");

      if (*file_ptr == NULL) {
        // index.html doesn't exist in directory
        char *cgi_argv[] = {URI + 1};
        return cgiExe(DIR_LIST_CGI_PATH, 1, cgi_argv);
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
      return "HTTP/1.0 500 Internal Error\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n\r\n";
    }

    const char *file_type = magic_descriptor(magic, file_des);
    printf("\tfile type is: %s\n", file_type);

    char *header_buf = (char *)malloc(HEADER_BUF_SIZE);

    snprintf(header_buf, HEADER_BUF_SIZE,
             "HTTP/1.0 200 Ok\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",
             file_type);
    printf("\t%s\n", header_buf);

    return header_buf;

  } else {
    // if nothing found
    return "HTTP/1.0 404 Not Found\r\n"
           "Content-Type: text/html\r\n"
           "Connection: close\r\n\r\n"
           "\r\n"
           "<html><body><h1>404 Not Found</h1>"
           "<h3>Sorry, the file you are looking for doesn't exist</h3>"
           "<br><br><p>Or does it?!?!</p><br><br><br><br>"
           "<p>No... it doesn't</p></body></html>\r\n";
  }
}
