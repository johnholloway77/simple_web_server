# Simple Web Server
This program was designed for and designed on FreeBSD 14.1. It is the group project from the class CS631 Advanced Programming in the Unix Enviroment as taught by Jan Schaumann.
The objective of this assignemnt is to write a simple web server that speaks a limited version of HTTP/1.0 as defined in [RFC1945](https://www.rfc-editor.org/rfc/rfc1945.html). For more inforamtion on the specific of the assignment, please visit the [assigment page](https://stevens.netmeister.org/631/f23-group-project.html).

This program is made simply for me to learn more about interprocess communication and sockets.

## Assignment Goals
The objective of this assignment is to write a simple web server that speaks a limited version of HTTP/1.0 as defined in RFC1945.
Your program should behave as one would expect from a regular system daemon. That is, it should detach from the controlling terminal and run in the background, accept multiple simultaneous connections, not generate any messages on stdout unless in debugging mode etc.

**Important:** This is not a production server. It has limited functionality, probable bugs and security issues. It is purely made for educational purposes. It is **not** recommended that you use it exposed to the internet.

## Building the Program
To build the program, run cmake in the main directory of the program. Ensure you have cmake installed on your system.

To build the program, follow these steps:

1. Navigate to the main directory of the program.
2. Run the following commands:
```sh
$ ./setup.sh
```

The setup file will run the Makefile, as well as create several files and subdirectories. The files will be a mix of HTML files, CGI scripts, and text files.
```
.
├── cgi-bin
│   ├── guestBook.c
│   ├── guestBook.cgi
│   ├── helloWorld.c
│   └── helloWorld.cgi
├── cgi-data
├── index.html
├── simple_server
├── subNoIndex
│   ├── test1.txt
│   └── test2.txt
└── subWithIndex
    └── index.html
```


Should you wish to only build the server instead run the command:
```sh
make
```
To remove the only object files after compiling the binary run the following command
```sh
make clean-obj
```

To remove all files created during the make process including the binary run the following
```sh
make clean
```

If the application is built on another *nix machine it is recommended that the code be reviewed for any library or system calls that may need to be refactored.

## Running the Program

```sh
./simple_server [option] [path] ...
```

Replace [options] with any applicable flags and [path] with the directories or files you want to analyze. The program will list the contents of the specified directory, formatted according to the provided options.

**−c *dir*** Allow execution of CGIs from the given directory. 

**−d** Enter debugging mode. That is, do not daemonize, only accept one connection at a time
and enable logging to stdout.

**−i *address*** Bind to the given IPv4 or IPv6 address. If not provided, sws will listen on all IPv4 and
IPv6 addresses on this host.

**−l *file*** Log all requests to the given file. See LOGGING for details.

**−p *port*** Listen on the given port. If not provided, sws will listen on port 8080.

Once the server is running it can be accessed in a browser with http://localhost:[portnumber]. If using the setup script, the server will load the index.html file in the directory.

## Closing the application

Simple_server is designed to run as a daemon and will be inaccessable from the terminal unless the *-d* flag has been enabled.

To close the application while running as a daemon you will need to kill the process using the process ID. The process ID can be found using the sockstat command and grepping the respective port number.
```
$ ./simple_server 
$ sockstat | grep 8080
jholloway simple_server 31863 3   tcp4   *:8080                *:*
jholloway simple_server 31863 4   tcp6   *:8080                *:*
$ kill 31863
```

## Memory Management
This program has been carefully developed to handle memory management correctly, ensuring no memory leaks. Valgrind was used extensively to check for and fix any memory issues.


