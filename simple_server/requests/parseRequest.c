#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./requests.h"
#include "../cgi/cgi.h"

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

  if (checkMethod(method) == 0) {
    free(str);
    return "HTTP/1.0 400 Bad Request\r\n"
           "Content-Length: 0\r\n"
           "Connection: close\r\n\r\n";
  }

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
      char *cgi_URI = strdup(URI + 9); //get the first part of /cgi-bin/someExeFile
      cgi_URI = strtok(cgi_URI, "/"); //get the exec name;

      free(cgi_URI);

      return cgiExe(cgi_URI, NULL);
  } else {
    *file_ptr = fopen(URI + 1, "r");
  }

  free(str);

  if (*file_ptr) {
    return "HTTP/1.0 200\r\n"
           "Content-Type: text/html\r\n"
           "Connection: close\r\n\r\n";
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
