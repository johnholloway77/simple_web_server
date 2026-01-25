#!/bin/sh

make
make clean-obj

# Create the cgi-bin directory if it doesn't exist
mkdir -p ./cgi-bin ./cgi-data ./subWithIndex ./subNoIndex

# Write the C code into a file called helloWorld.c inside the cgi-bin directory
cat <<EOF > ./cgi-bin/helloWorld.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define RES_PIPE_NAME "RESPONSE_PIPE"

int main(int argc, char **argv){

  int response_code;
  int resp_pipe_fd;
 char *resp_pipe_fd_str = getenv(RES_PIPE_NAME);
 resp_pipe_fd = atoi(resp_pipe_fd_str);

                response_code = 200;
        write(resp_pipe_fd, &response_code, sizeof(int));
                printf("HTTP/1.0 %d OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "<html><body><h1>Hello World!</h1><p>This page was generated from the hello world C file</p></body></html>", response_code
                        );

        close(resp_pipe_fd);
        return 0;
}
EOF

#Lets write the CGI for the guest book found on index.html
cat <<EOF> ./cgi-bin/guestBook.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RES_PIPE_NAME "RESPONSE_PIPE"

int main(void) {

  FILE *fp;
  const char *filename = "cgi-data/guestbook.txt";
  char name_buf[256];

  char *name = getenv("NAME");

  int response_code = 200;
  int resp_pipe_fd;
  char *resp_pipe_fd_str = getenv(RES_PIPE_NAME);
  resp_pipe_fd = atoi(resp_pipe_fd_str);


  if( name){
      if (*name != '\0') {
          fp = fopen(filename, "a");
          if (fp == NULL) {
              fp = fopen(filename, "w");
              if (fp == NULL) {

                  response_code = 500;
                  write(resp_pipe_fd, &response_code, sizeof(int));
                  printf("HTTP/1.0 %d Server Error\r\n"
                         "Content-Type: text/html\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "<http><body><h1>Internal Server Error</h1><p>Could not get "
                         "guestbook</p></body></html>",
                         response_code);

                  fclose(fp);
                  close(resp_pipe_fd);
                  return 0;
              }
          }

          fprintf(fp, "%s\n", name);

          fclose(fp);
      }
  }

  fp = fopen(filename, "r");
  if (fp == NULL) {

    response_code = 500;
    write(resp_pipe_fd, &response_code, sizeof(int));
    printf("HTTP/1.0 %d Server Error\r\n"
           "Content-Type: text/html\r\n"
           "Connection: close\r\n"
           "\r\n"
           "<http><body><h1>Internal Server Error</h1><p>Could not get "
           "guestbook</p></body></html>",
           response_code);

    fclose(fp);
    close(resp_pipe_fd);
    return 0;
  }



  response_code = 200;

  ssize_t bytes_written = write(resp_pipe_fd, &response_code, sizeof(int));
  if (bytes_written == -1) {
      perror("Error writing to pipe");
  } else if (bytes_written != sizeof(int)) {
      fprintf(stderr, "Incomplete write to pipe\n");
  }


  printf("HTTP/1.0 %d OK\r\n"
         "Content-Type: text/html\r\n"
         "Connection: close\r\n"
         "\r\n"
         "<html><body><h1>Guest Book</h1><p>The following people have signed "
         "the guest book:</p>",
         response_code);


  while (fgets(name_buf, sizeof(name_buf), fp) != NULL) {
    printf("<p>Visitor: %s</p>", name_buf);
  }

  printf("</body></html>");




  fclose(fp);
  close(resp_pipe_fd);

  return 0;
}



EOF




# Compile the C program into an executable called helloWorld inside the cgi-bin directory
cc ./cgi-bin/helloWorld.c -o ./cgi-bin/helloWorld.cgi
cc ./cgi-bin/guestBook.c -o ./cgi-bin/guestBook.cgi

# Make an index.html page for the root directory
cat <<EOF > ./index.html
<html>
<body>
<h1>Hello World!</h1>
<p>This page has been served to you by the simple server</p>
<br>
<p>This program was designed for and designed on FreeBSD 14.1. It is the group project from the class <a href="https://stevens.netmeister.org/631/">
CS631 Advanced Programming in the Unix Enviroment as taught by Jan Schaumann</a>. The objective of this assignemnt is to write a simple web server
that speaks a limited version of HTTP/1.0 as defined in RFC1945. For more information on the specific of the assignment, please visit the
<a href="https://stevens.netmeister.org/631/f23-group-project.html">assignment page.</a></p>

<p>This program is made simply for me to learn more about interprocess communication and sockets.</p>

<br>
<a href="./cgi-bin/helloWorld.cgi">Click here for a CGI helloWorld page</a><br>
<br>
<h2>Sign the guestbook!</h2>
<form  action="./cgi-bin/guestBook.cgi">
<label for="name">Name: </label>
<input type="text" id="name" name="NAME">
<input type="submit" value="submit">
</form>

<a href="./subWithIndex">Sub directory with index.html</a><br>
<a href="./subNoIndex">Sub Directory without index.html</a><br>
<a href="./cgi-bin/helloWorld.cgi">View guestbook without signing</a>


</body>
</html>
EOF

#make an index.html page for subWithIndex
cat <<EOF> subWithIndex/index.html
<html>
<body>

<h1>Subdirectory index file!</h1>

<p> Put this file in random subdirectories as the default file. Use it to test if the server will automatically direct to index.html</p>

</body>
</html>
EOF

#make files to test subNoIndex
cat <<EOF> subNoIndex/test1.txt
Hello My Baby!
Hello My Honey!
Hello My Ragtime gal!
EOF

i=1
while [ $i -le 100 ]
do
    echo "All work and no play makes Jack a dull boy" >> subNoIndex/test2.txt
    i=$((i + 1))
done
