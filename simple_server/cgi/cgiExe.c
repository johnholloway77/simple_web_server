

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fcntl.h>

#include "../flags/flags.h"
#include "../response/response.h"

#define BUFFER 1024
#define CGI_BIN_DIR "./cgi-bin"
#define RES_PIPE_NAME "RESPONSE_PIPE"



extern uint32_t app_flags;
extern char* cgi_addr;

// Helper function to decode URL-encoded strings
void url_decode(char *dst, const char *src) {
  char a, b;
  while (*src) {
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
        (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';
}

// char *cgiExe2(char *cgiURI, int cgi_argc, char *cgi_argv[]);
char *cgiExe(char *file, int cgi_argc, char *cgi_argv[], int *resp_status) {

  char *file_name = strtok(file, "?");
  //printf("\tfile name: %s\n", file_name);

  if (file == NULL || (strcmp(file, "") == 0)) {
    *resp_status = 400;
    return RESPONSE_400;
  }

  char *file_name2 = strdup(file_name);
  // printf("\tfile name2: %s\n", file_name2);

  char *param_string = strtok(NULL, "?");
  // printf("\tparam_string = %s\n", param_string);

  char *buffer;
  char *name;
  char *val;

  int pipe_stdin[2];
  int pipe_stdout[2];
  int pipe_response[2];
  pid_t pid;

  if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1 || pipe(pipe_response)) {
    free(file_name2);

      *resp_status = 500;
    return RESPONSE_500;
  }



  pid = fork();
  if (pid == -1) {
      free(file_name2);

      *resp_status = 500;
    return RESPONSE_500;
  }

  // Child process
  if (pid == 0) {
    dup2(pipe_stdout[1], STDOUT_FILENO); // Redirect stdout to pipe

    close(pipe_stdout[0]);
    close(pipe_stdin[1]);
    close(pipe_response[0]);

    // Set environment variables for response pipe
    char res_pipe_fd_str[4];
    snprintf(res_pipe_fd_str, sizeof(res_pipe_fd_str), "%d", pipe_response[1]);
    setenv(RES_PIPE_NAME, res_pipe_fd_str, 1);

    //set environment variable for request parameters
    while ((buffer = strtok(param_string, "&")) != NULL) {
      param_string = NULL; // Continue tokenizing the original string

      name = strtok(buffer, "=");
      val = strtok(NULL, "=");

      if (name && val) {
        char decoded_name[256];
        char decoded_val[256];
        url_decode(decoded_name, name);
        url_decode(decoded_val, val);

        // printf("\tname: %s value: %s\n", decoded_name, decoded_val);

        char *s = decoded_name;
        while (*s) {
          *s = toupper(*s);
          s++;
        }

        setenv(decoded_name, decoded_val, 1);
      }
    }

    char path[PATH_MAX];

    /*
     * cgi_addr should be set in function setFlags() in setFlags.c
     * This function should have been called at program start in main.c
     */
    if(cgi_addr == NULL){
        free(file_name2);
        *resp_status = 500;
        return RESPONSE_500;
    }

    //branching logic here only exists so that CGI will load directory CGI when C_FLAGS isn't set.
    //Will change to only the first branch once directory listing is no longer done through CGI.
    if(app_flags & C_FLAG){
        snprintf(path, PATH_MAX, "%s/%s", cgi_addr, file_name2);
    } else{
        snprintf(path, PATH_MAX, "%s%s", CGI_BIN_DIR, file_name2);

    }
            printf("path: %s\n", path);

    char *exec_args[cgi_argc + 2];
    exec_args[0] = path;

    for (int i = 0; i < cgi_argc; i++) {
      exec_args[i + 1] = strdup(cgi_argv[i]);
    }
    exec_args[cgi_argc + 1] = NULL;

    if (execvp(path, exec_args) == -1) {
      perror("execvp failed");
        free(file_name2);

      *resp_status = 500;
      return RESPONSE_500;
    }

    exit(EXIT_SUCCESS); // Not reached if execvp is successful
  }

  // Parent process
  else {
    close(pipe_stdout[1]); // Close unused write end
    close(pipe_stdin[0]);  // Close unused read end
    close(pipe_stdin[1]);  // Close unused write end
    close(pipe_response[1]);

    size_t nread;
    char buffer[BUFFER];
    char *response = malloc(BUFFER);
    size_t total_read = 0;

    if (response == NULL) {
        free(file_name2);
        *resp_status = 500;
      return RESPONSE_500;
    }


    if(read(pipe_response[0], resp_status, sizeof(int)) < 0){

        free(file_name2);
        return RESPONSE_500;
    }


    while ((nread = read(pipe_stdout[0], buffer, BUFFER)) > 0) {
      response = realloc(response, total_read + nread + 1);
      if (response == NULL) {
          free(file_name2);
          *resp_status = 500;
        return RESPONSE_500;
      }

      memcpy(response + total_read, buffer, nread);
      total_read += nread;
    }

    if (nread == 0) {
        free(file_name2);
      free(response);
        *resp_status = 500;
      return RESPONSE_500;
    }

    response[total_read] = '\0'; // Null-terminate the response
    close(pipe_stdout[0]);       // Close the read end of the pipe
    close(pipe_response[0]);


    free(file_name2);
    return response;
  }
}
