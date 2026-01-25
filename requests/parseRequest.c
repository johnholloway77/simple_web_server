#include <limits.h>
#include <magic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syslimits.h>

#include "../cgi/cgi.h"
#include "../flags/flags.h"
#include "../response/dirResponse.h"
#include "../response/response.h"
#include "./requests.h"

#define BASEURL "./\0"

extern uint32_t app_flags;

const char *
get_mime_type_by_ext(const char *filename, magic_t magic, int file_des)
{
  const char *ext = strrchr(filename, '.');
  if(!ext)
    {
      return magic_descriptor(magic, file_des);
    }

  // Wowzers a lookup table!
  if(strcasecmp(ext, ".htm") == 0)
    return "text/html";
  if(strcasecmp(ext, ".txt") == 0)
    return "text/plain; charset=utf-8";
  if(strcasecmp(ext, ".csv") == 0)
    return "text/csv; charset=utf-8";
  if(strcasecmp(ext, ".md") == 0)
    return "text/markdown; charset=utf-8";
  if(strcasecmp(ext, ".xml") == 0)
    return "application/xml";
  if(strcasecmp(ext, ".json") == 0)
    return "application/json";
  if(strcasecmp(ext, ".map") == 0)
    return "application/json"; // sourcemaps
  if(strcasecmp(ext, ".wasm") == 0)
    return "application/wasm";

  // CSS/JS variants
  if(strcasecmp(ext, ".mjs") == 0)
    return "text/javascript";
  if(strcasecmp(ext, ".cjs") == 0)
    return "text/javascript";

  // Images
  if(strcasecmp(ext, ".jpeg") == 0)
    return "image/jpeg";
  if(strcasecmp(ext, ".svg") == 0)
    return "image/svg+xml";
  if(strcasecmp(ext, ".webp") == 0)
    return "image/webp";
  if(strcasecmp(ext, ".ico") == 0)
    return "image/x-icon";

  // Fonts
  if(strcasecmp(ext, ".woff") == 0)
    return "font/woff";
  if(strcasecmp(ext, ".woff2") == 0)
    return "font/woff2";
  if(strcasecmp(ext, ".ttf") == 0)
    return "font/ttf";
  if(strcasecmp(ext, ".otf") == 0)
    return "font/otf";

  // Audio / video (common)
  if(strcasecmp(ext, ".mp3") == 0)
    return "audio/mpeg";
  if(strcasecmp(ext, ".wav") == 0)
    return "audio/wav";
  if(strcasecmp(ext, ".mp4") == 0)
    return "video/mp4";
  if(strcasecmp(ext, ".webm") == 0)
    return "video/webm";

  // Documents
  if(strcasecmp(ext, ".pdf") == 0)
    return "application/pdf";

  // Archives / binaries
  if(strcasecmp(ext, ".zip") == 0)
    return "application/zip";
  if(strcasecmp(ext, ".gz") == 0)
    return "application/gzip";
  if(strcasecmp(ext, ".tgz") == 0)
    return "application/gzip"; // tar+gzip
  if(strcasecmp(ext, ".tar") == 0)
    return "application/x-tar";

  return magic_descriptor(magic, file_des);
}

char *
parseRequest(const char *req_str, FILE **file_ptr, int *resp_status,
             magic_t magic)
{
  char *str;

  str = strdup(req_str);

  if(!str)
    {
      *resp_status = 500;
      RETURN_RESP(RESPONSE_500)
    }

  char *method = strtok(str, " ");
  char *URI = strtok(NULL, " ");
  char *http = strtok(NULL, " ");

  char URI_relative[PATH_MAX];
  char *baseUrl = BASEURL;
  size_t baseLength = strlen(baseUrl);

  strlcpy(URI_relative, baseUrl, PATH_MAX);
  strlcpy(URI_relative + baseLength, URI, PATH_MAX);

  // check that the request header was properly parsed
  if(method && URI && http)
    {
    }
  else
    {
      free(str);
      *resp_status = 500;
      RETURN_RESP(RESPONSE_500)
    }

  if((strstr(URI, "../")) || (strstr(URI, "/..")))
    {
      free(str);
      *resp_status = 403;
      RETURN_RESP(RESPONSE_403)
    }

  /*
   * check if it's a valid method
   * We're only doing GET requests for this simple project
   */
  if(checkMethod(method) == 0)
    {
      free(str);
      *resp_status = 400;
      RETURN_RESP(RESPONSE_400)
    }

  /*
   * Check that it's either HTML1.0 or 1.1
   */
  if(checkHttp(http) == 0)
    {
      free(str);
      *resp_status = 400;
      RETURN_RESP(RESPONSE_400)
    }

  // should now get index by default
  if(strcmp(URI, "/") == 0)
    {
      *file_ptr = fopen("index.html", "r");
    }
  else if(strncmp(URI, "/cgi-bin/", 9) == 0)
    {
      if(app_flags & C_FLAG)
        {
          char *cgi_URI =
              strdup(URI + 9); // get the first part of /cgi-bin/someExeFile
          cgi_URI = strtok(cgi_URI, "/"); // get the exec name;

          if(cgi_URI == NULL || strcmp(cgi_URI, "") == 0)
            {
              free(cgi_URI); // Free allocated memory before returning error
                             // response

              free(str);
              *resp_status = 400;
              RETURN_RESP(RESPONSE_400)
            }

          char *cgi_argv[] = { URI + 1 }; // pass directory path to

          char *response = cgiExe(cgi_URI, 1, cgi_argv, resp_status);
          free(cgi_URI);
          free(str);

          return response;
        }
      else
        {
          free(str);

          *resp_status = 501;
          RETURN_RESP(RESPONSE_501)
        }
    }
  else
    {
      // //check if file points to a directory
      // //if directory, call cgi script
      struct stat stat1;

      if(lstat(URI + 1, &stat1) != 0)
        {
          /*
           * will actuall work to check if file exists
           * returns 404 if not
           */
          free(str);

          *resp_status = 404;
          RETURN_RESP(RESPONSE_404)
        }

      if(S_ISDIR(stat1.st_mode))
        {
          char index_path[PATH_MAX];
          memset(index_path, 0, sizeof(index_path));

          if(URI_relative[strlen(URI_relative) - 1] == '/')
            {
              snprintf(index_path, PATH_MAX, "%sindex.html", URI_relative);
            }
          else
            {
              snprintf(index_path, PATH_MAX, "%s/index.html", URI_relative);
            }

          *file_ptr = fopen(index_path, "r");

          if(*file_ptr == NULL)
            {
              char *response = dirResponse(URI_relative, resp_status);
              free(str);
              return response;
            }
        }
      else
        {
          // file is regular file
          *file_ptr = fopen(URI_relative, "r");
        }
    }

  if(*file_ptr)
    {
      int file_des = fileno(*file_ptr);

      // Save filename to stack before freeing str to avoid use-after-free
      char fileName[PATH_MAX];

      if(strcmp(URI, "/") == 0)
        {
          fileName[0] = '\0'; // Empty string for root
        }
      else
        {
          strlcpy(fileName, URI + 1, sizeof(fileName));
        }

      // Now safe to free str
      free(str);

      const char *file_type;
      if(strcmp(URI, "/") == 0)
        {
          file_type = "text/html";
        }
      else
        {
          file_type = get_mime_type_by_ext(fileName, magic, file_des);
        }

      char *header_buf = (char *) malloc(HEADER_BUF_SIZE);
      if(header_buf == NULL)
        {
          *resp_status = 500;
          return RESPONSE_500;
        }

      snprintf(
          header_buf, HEADER_BUF_SIZE,
          "HTTP/1.0 200 Ok\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",
          file_type);

      *resp_status = 200;
      return header_buf;
    }
  else
    {
      // this code is not reached. could delete it if need be...
      // if nothing found

      free(str);
      *resp_status = 404;
      RETURN_RESP(RESPONSE_404)
    }
}
