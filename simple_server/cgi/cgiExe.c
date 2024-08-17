
// #include <limits.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <sys/types.h>
// #include <sys/wait.h>

// #define BUFFER 1024
// #define CGI_BIN_DIR "./cgi-bin/"

// char *cgiExe(char *cgiURI, char **args) {

//     char *file_name = strtok(cgiURI, "?");

//     printf("\tfile name: %s\n", file_name);

//     char *param_string = strtok(NULL, "?");

//     printf("\tparam_string = %s\n", param_string);

//     char *buffer;
//     char *name;
//     char *val;




//     int pipe_stdin[2];
//     int pipe_stdout[2];
//     pid_t pid;

//     if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
//         return "HTTP/1.0 500 Internal Error\r\n"
//                "Content-Type: text/plain\r\n"
//                "Connection: close\r\n\r\n"
//                "Error: creating pipes";
//     }

//     pid = fork();
//     if (pid == -1) {
//         return "HTTP/1.0 500 Internal Error\r\n"
//                "Content-Type: text/plain\r\n"
//                "Connection: close\r\n\r\n"
//                "Error: fork()";
//     }

//     // Child process
//     if (pid == 0) {
//         //dup2(pipe_stdin[0], STDIN_FILENO);
//         dup2(pipe_stdout[1], STDOUT_FILENO);  // Redirect stdout to pipe

//         close(pipe_stdout[0]);  // Close unused read end
//         close(pipe_stdin[1]);   // Close unused write end

//         //set environment variables
//         while((buffer = strtok(param_string, "&")) != NULL){
//             param_string = NULL;

//             name = strtok(buffer, "=");
//             val = strtok(NULL, "=");

//             printf("\tname %s value %s\n", name, val);

//             setenv(name, val, 1);
//         }


//         char path[PATH_MAX];
//         snprintf(path, PATH_MAX, "%s%s", CGI_BIN_DIR, file_name);

//         printf("\tpathname: %s\n", path);
//         char *exec_args[] = {path, NULL};



//         if (execvp(path, exec_args) == -1) {
//             perror("execvp failed");
//             exit(EXIT_FAILURE);  // Exit on execvp failure
//         }

//         exit(EXIT_SUCCESS);  // Not reached if execvp is successful
//     }

//     // Parent process
//     else {
//         close(pipe_stdout[1]);  // keep both stdin and stdout open to cgi process

//         close(pipe_stdin[0]);   // Close unused read end
//         close(pipe_stdin[1]);   // Close unused write end

//         size_t nread;
//         char buffer[BUFFER];
//         char *response = malloc(BUFFER);
//         size_t total_read = 0;

//         if (response == NULL) {
//             return "HTTP/1.0 500 Internal Error\r\n"
//                    "Content-Type: text/plain\r\n"
//                    "Connection: close\r\n\r\n"
//                    "Error: malloc()";
//         }

//         while ((nread = read(pipe_stdout[0], buffer, BUFFER)) > 0) {
//             response = realloc(response, total_read + nread + 1);
//             if (response == NULL) {
//                 return "HTTP/1.0 500 Internal Error\r\n"
//                        "Content-Type: text/plain\r\n"
//                        "Connection: close\r\n\r\n"
//                        "Error: realloc()";
//             }

//             memcpy(response + total_read, buffer, nread);
//             total_read += nread;
//         }

//         if (nread == -1) {
//             free(response);
//             return "HTTP/1.0 500 Internal Error\r\n"
//                    "Content-Type: text/plain\r\n"
//                    "Connection: close\r\n\r\n"
//                    "Error: read() failed";
//         }

//         response[total_read] = '\0';  // Null-terminate the response
//         close(pipe_stdout[0]);  // Close the read end of the pipe

//         int status;
//         waitpid(pid, &status, 0);

//         // Check if the child process terminated successfully
//         if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
//             free(response);
//             return "HTTP/1.0 404 Not Found\r\n"
//                    "Content-Type: text/html\r\n"
//                    "Connection: close\r\n\r\n"
//                    "<html><body><h1>404 Not Found</h1>"
//                    "<p>The requested CGI script was not found or failed to execute.</p></body></html>\r\n";
//         }

//         return response;
//     }
// }

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

#define BUFFER 1024
#define CGI_BIN_DIR "./cgi-bin/"

// Helper function to decode URL-encoded strings
void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
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

char *cgiExe(char *cgiURI, char **args) {

    char *file_name = strtok(cgiURI, "?");
   // printf("\tfile name: %s\n", file_name);

    char *file_name2 = strdup(file_name);
    //printf("\tfile name2: %s\n", file_name2);

    char *param_string = strtok(NULL, "?");
    //printf("\tparam_string = %s\n", param_string);

    char *buffer;
    char *name;
    char *val;

    int pipe_stdin[2];
    int pipe_stdout[2];
    pid_t pid;

    if (pipe(pipe_stdin) == -1 || pipe(pipe_stdout) == -1) {
        return "HTTP/1.0 500 Internal Error\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n\r\n"
               "Error: creating pipes";
    }

    pid = fork();
    if (pid == -1) {
        return "HTTP/1.0 500 Internal Error\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n\r\n"
               "Error: fork()";
    }

    // Child process
    if (pid == 0) {
        dup2(pipe_stdout[1], STDOUT_FILENO);  // Redirect stdout to pipe

        close(pipe_stdout[0]);  // Close unused read end
        close(pipe_stdin[1]);   // Close unused write end

        // Set environment variables
        while ((buffer = strtok(param_string, "&")) != NULL) {
            param_string = NULL;  // Continue tokenizing the original string

            name = strtok(buffer, "=");
            val = strtok(NULL, "=");

            if (name && val) {
                char decoded_name[256];
                char decoded_val[256];
                url_decode(decoded_name, name);
                url_decode(decoded_val, val);

               // printf("\tname: %s value: %s\n", decoded_name, decoded_val);

                char *s = decoded_name;
                while(*s){
                    *s = toupper(*s);
                    s++;
                }

                setenv(decoded_name, decoded_val, 1);
            }
        }

        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s%s", CGI_BIN_DIR, file_name2);
       // printf("\tfile_name: %s\n", file_name2);

        //printf("\tpathname: %s\n", path);
        char *exec_args[] = {path, NULL};

        if (execvp(path, exec_args) == -1) {
            perror("execvp failed");
            exit(EXIT_FAILURE);  // Exit on execvp failure
        }

        exit(EXIT_SUCCESS);  // Not reached if execvp is successful
    }

    // Parent process
    else {
        close(pipe_stdout[1]);  // Close unused write end
        close(pipe_stdin[0]);   // Close unused read end
        close(pipe_stdin[1]);   // Close unused write end

        size_t nread;
        char buffer[BUFFER];
        char *response = malloc(BUFFER);
        size_t total_read = 0;

        if (response == NULL) {
            return "HTTP/1.0 500 Internal Error\r\n"
                   "Content-Type: text/plain\r\n"
                   "Connection: close\r\n\r\n"
                   "Error: malloc()";
        }

        while ((nread = read(pipe_stdout[0], buffer, BUFFER)) > 0) {
            response = realloc(response, total_read + nread + 1);
            if (response == NULL) {
                return "HTTP/1.0 500 Internal Error\r\n"
                       "Content-Type: text/plain\r\n"
                       "Connection: close\r\n\r\n"
                       "Error: realloc()";
            }

            memcpy(response + total_read, buffer, nread);
            total_read += nread;
        }

        if (nread == -1) {
            free(response);
            return "HTTP/1.0 500 Internal Error\r\n"
                   "Content-Type: text/plain\r\n"
                   "Connection: close\r\n\r\n"
                   "Error: read() failed";
        }

        response[total_read] = '\0';  // Null-terminate the response
        close(pipe_stdout[0]);  // Close the read end of the pipe

        int status;
        waitpid(pid, &status, 0);

        // Check if the child process terminated successfully
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            free(response);
            return "HTTP/1.0 404 Not Found\r\n"
                   "Content-Type: text/html\r\n"
                   "Connection: close\r\n\r\n"
                   "<html><body><h1>404 Not Found</h1>"
                   "<p>The requested CGI script was not found or failed to execute.</p></body></html>\r\n";
        }

        return response;
    }
}
