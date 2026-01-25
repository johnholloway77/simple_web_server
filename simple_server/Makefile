#Compiler
CC = clang

#Compiler flags
CFLAGS = -Wall -Wextra
DEBUG_CFLAGS = -Wall -Wextra -g -DDEBUG -fsanitize=address

# Source files
SOURCES = main.c \
    flags/setFlags.c \
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

#debug build with debug symbols and AddressSanitizer
.PHONY: debug
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(BINARY)

.PHONY:  clean
clean: 
	rm -rf $(BINARY) $(OBJECTS)

#clean only the object files
.PHONY: clean-obj
clean-obj:
	rm -rf $(OBJECTS)
