#!/bin/sh

# Create the cgi-bin directory if it doesn't exist
mkdir -p ./cgi-bin ./cgi-data

# Write the C code into a file called helloWorld.c inside the cgi-bin directory
cat <<EOF > cgi-bin/helloWorld.c
#include <stdio.h>

int main(int argc, char **argv) {
    printf("HTTP/1.0 200 OK\r\n"
           "Content-Type: text/html\r\n"
           "Connection: close\r\n"
           "\r\n"
           "<html><body><h1>Hello World!</h1><p>This page was generated from the hello world C file</p></body></html>"
           );

    return 0;
}
EOF

cat <<EOF> cgi-bin/guestBook.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {

  FILE *fp;
  const char *filename = "cgi-data/guestbook.txt";
  char name_buf[256];

  char *name = getenv("NAME");

  if(name == NULL ){
      printf("HTTP/1.0 400 Bad Request\r\n"
             "Content-Type: text/html\r\n"
             "Connection: close\r\n"
             "\r\n"
             "<html><body><h1>Bad Request</h1><p>Name not provided</p></body></html>");
      return 0;
  }

if(*name != '\0') {
  fp = fopen(filename, "a");
  if (fp == NULL) {
    fp = fopen(filename, "w");
    if (fp == NULL) {
      printf("HTTP/1.0 500 Server Error\r\n"
             "Content-Type: text/html\r\n"
             "Connection: close\r\n"
             "\r\n"
             "<http><body><h1>Internal Server Error</h1><p>Could not get "
             "guestbook</p></body></html>");
      return 0;
    }


  }

  fprintf(fp, "%s\n", name);

  fclose(fp);
}


  fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("HTTP/1.0 500 Server Error\r\n"
           "Content-Type: text/html\r\n"
           "Connection: close\r\n"
           "\r\n"
           "<http><body><h1>Internal Server Error</h1><p>Could not get "
           "guestbook</p></body></html>");
    return 0;
  }

  printf("HTTP/1.0 200 OK\r\n"
         "Content-Type: text/html\r\n"
         "Connection: close\r\n"
         "\r\n"
         "<html><body><h1>Guest Book</h1><p>The following people have signed "
         "the guest book:</p>");

  while (fgets(name_buf, sizeof(name_buf), fp) != NULL) {
    printf("<p>Visitor: %s</p>", name_buf);
  }

  printf("</body></html>");

  fclose(fp);

  return 0;
}

EOF


# Compile the C program into an executable called helloWorld inside the cgi-bin directory
cc cgi-bin/helloWorld.c -o cgi-bin/helloWorld.cgi
cc cgi-bin/guestBook.c -o cgi-bin/guestBook.cgi

# Make an index.html page for the root directory
cat <<EOF > index.html
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

</body>
</html>
EOF
