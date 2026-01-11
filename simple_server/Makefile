#Compiler
CC = clang

#Compiler flags
CFLAGS = -Wall -Wextra

# Source files
SOURCES = main.c \
    flags/setFlags.c \
    sig_handlers/reap.c \
    sockets/createSocket_v4.c \
    sockets/createSocket_v6.c \
    sockets/handleSocket.c \
    sockets/handleConnection.c \
    requests/parseRequest.c \
    requests/checkMethod.c \
    requests/checkHttp.c \
    cgi/cgiExe.c \
    response/dirResponse.c

OBJECTS = $(SOURCES:.c=.o)

#libraries to link
LIBS = -lmagic

#output binary
BINARY=simple_server

#rule to link the binary
$(BINARY): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LIBS)

#rule to compile source files into object files
%.o:  %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:  clean
clean: 
	rm -rf $(BINARY) $(OBJECTS)

#clean only the object files
.PHONY: clean-obj
clean-obj:
	rm -rf $(OBJECTS)
