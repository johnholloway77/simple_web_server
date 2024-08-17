
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER 1024
#define CGI_BIN_DIR "./cgi-bin/"

char *cgiExe(const char *file, char **args) {
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
        //dup2(pipe_stdin[0], STDIN_FILENO);
        dup2(pipe_stdout[1], STDOUT_FILENO);  // Redirect stdout to pipe

        close(pipe_stdout[0]);  // Close unused read end
        close(pipe_stdin[1]);   // Close unused write end

        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s%s", CGI_BIN_DIR, file);

        char *exec_args[] = {path, NULL};

        if (execvp(path, exec_args) == -1) {
            perror("execvp failed");
            exit(EXIT_FAILURE);  // Exit on execvp failure
        }

        exit(EXIT_SUCCESS);  // Not reached if execvp is successful
    }

    // Parent process
    else {
        close(pipe_stdout[1]);  // keep both stdin and stdout open to cgi process

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
