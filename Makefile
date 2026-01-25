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

# Code quality and formatting targets
.PHONY: format
format:
	@echo "Formatting C source files..."
	@for file in $(SOURCES) $(wildcard */*.h); do \
		echo "Formatting $$file"; \
		clang-format19 -i $$file; \
	done

.PHONY: format-check
format-check:
	@if command -v clang-format19 >/dev/null 2>&1; then \
		echo "Checking code formatting..."; \
		failed=0; \
		for file in $(SOURCES) $(wildcard */*.h); do \
			if ! clang-format19 --dry-run --Werror $$file > /dev/null 2>&1; then \
				echo "❌ $$file needs formatting"; \
				failed=1; \
			else \
				echo "✅ $$file"; \
			fi; \
		done; \
		if [ $$failed -eq 1 ]; then \
			echo ""; \
			echo "💡 Run 'make format' to fix formatting issues"; \
			exit 1; \
		else \
			echo "🎉 All files are properly formatted!"; \
		fi; \
	else \
		echo "⚠️  clang-format not found - skipping format check"; \
		echo "📦 Install with: pkg install llvm"; \
	fi

.PHONY: lint
lint:
	@echo "Running cppcheck static analysis..."
	@cppcheck --enable=all --suppress=missingIncludeSystem --suppress=unusedFunction \
		--error-exitcode=1 $(SOURCES)
	@echo "Running cpplint style check..."
	@cpplint --filter=-whitespace/line_length,-build/include_subdir \
		--linelength=80 $(SOURCES) $(wildcard */*.h)

.PHONY: check
check: format-check lint
	@echo "🎉 All code quality checks passed!"

.PHONY: fix
fix: format
	@echo "✅ Code formatting applied. Run 'make check' to verify."
