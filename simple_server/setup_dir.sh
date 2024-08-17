#!/bin/sh

# Create the cgi-bin directory if it doesn't exist
mkdir -p ./cgi-bin

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

# Compile the C program into an executable called helloWorld inside the cgi-bin directory
cc cgi-bin/helloWorld.c -o cgi-bin/helloWorld

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

</body>
</html>
EOF
