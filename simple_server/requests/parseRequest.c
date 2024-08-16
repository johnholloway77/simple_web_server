#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./requests.h"

char *parseRequest(const char *req_str, FILE **file_ptr) {
  char *str;
  str = strdup(req_str);
  char *body;

  char *method = strtok(str, " ");
  char *URI = strtok(NULL, " ");
  char *http = strtok(NULL, " ");

  free(str);

  if (method && URI && http) {
    printf("Method: '%s'\n", method);
    printf("URI: '%s'\n", URI);
    printf("http: '%s'\n", http);
  } else {
    printf("invalid request");
    return "HTTP/1.0 400 Bad Request\r\n"
           "Content-Length: 0\r\n\r\n";
  }

  if (checkMethod(method) == 0) {
    return "HTTP/1.0 400 Bad Request\r\n"
           "Content-Length: 0\r\n\r\n";
  }

  if (checkHttp(http) == 0) {
    return "HTTP/1.0 400 Bad Request\r\n"
           "Content-Length: 0\r\n\r\n";
  }


  //should now get index by default
  if(strcmp(URI, "/") == 0){
      *file_ptr = fopen("index.html", "r");
  } else {
      *file_ptr = fopen(++URI, "r");
  }



  if (*file_ptr) {
    return "HTTP/1.0 200\r\n"
           "Content-Type: text/html\r\n\r\n";
  } else {
    // if nothing found
    return "HTTP/1.0 404 Not Found\r\n"
           "Content-Type: text/html\r\n"
           "\r\n"
           "<html><body><h1>404 Not Found</h1>"
           "<p>Sorry, the file you are looking for doesn't exist</p>"
           "<br><br><p>Or does it?!?!</p><br><br><br><br>"
           "<p>No... it doesn't</p></body></html>\r\n";
  }
}
