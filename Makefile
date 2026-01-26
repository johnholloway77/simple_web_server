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

# Documentation generation target
.PHONY: docs
docs:
	@echo "🔍 Checking for Doxygen..."
	@if command -v doxygen >/dev/null 2>&1; then \
		echo "✅ Found doxygen: $$(which doxygen)"; \
		doxygen_cmd=doxygen; \
	elif [ -x /usr/local/bin/doxygen ]; then \
		echo "✅ Found doxygen: /usr/local/bin/doxygen"; \
		doxygen_cmd=/usr/local/bin/doxygen; \
	elif [ -x /usr/bin/doxygen ]; then \
		echo "✅ Found doxygen: /usr/bin/doxygen"; \
		doxygen_cmd=/usr/bin/doxygen; \
	else \
		echo "❌ Doxygen not found!"; \
		echo "📦 Please install Doxygen:"; \
		echo "   FreeBSD: pkg install doxygen"; \
		echo "   Linux:   apt install doxygen (Debian/Ubuntu)"; \
		echo "           dnf install doxygen (Fedora/RHEL)"; \
		echo "   macOS:   brew install doxygen"; \
		exit 1; \
	fi; \
	echo ""; \
	echo "🔍 Checking for Doxygen configuration..."; \
	if [ ! -d docs ]; then \
		echo "📁 Creating docs directory..."; \
		mkdir -p docs; \
	fi; \
	if [ -f docs/Doxyfile ]; then \
		echo "✅ Found Doxygen config: docs/Doxyfile"; \
		echo ""; \
		echo "📚 Generating documentation..."; \
		cd docs && $$doxygen_cmd Doxyfile; \
		if [ $$? -eq 0 ]; then \
			echo "🎉 Documentation generated successfully!"; \
			echo "📖 Open docs/html/index.html to view documentation"; \
		else \
			echo "❌ Documentation generation failed!"; \
			exit 1; \
		fi; \
	elif [ -f docs/doxyfile ]; then \
		echo "✅ Found Doxygen config: docs/doxyfile"; \
		echo ""; \
		echo "📚 Generating documentation..."; \
		cd docs && $$doxygen_cmd doxyfile; \
		if [ $$? -eq 0 ]; then \
			echo "🎉 Documentation generated successfully!"; \
			echo "📖 Open docs/html/index.html to view documentation"; \
		else \
			echo "❌ Documentation generation failed!"; \
			exit 1; \
		fi; \
	else \
		echo "❌ Doxygen configuration file not found!"; \
		echo "🔧 Please create a Doxygen configuration:"; \
		echo "   1. cd docs"; \
		echo "   2. doxygen -g Doxyfile"; \
		echo "   3. Edit Doxyfile to configure your project"; \
		echo "   4. Run 'make docs' again"; \
		echo ""; \
		echo "💡 Suggested Doxyfile settings for this project:"; \
		echo "   PROJECT_NAME = \"FreeBSD Web Server\""; \
		echo "   INPUT = ../"; \
		echo "   RECURSIVE = YES"; \
		echo "   FILE_PATTERNS = *.c *.h"; \
		echo "   EXCLUDE_PATTERNS = */build/* */.*"; \
		echo "   GENERATE_HTML = YES"; \
		echo "   GENERATE_LATEX = NO"; \
		exit 1; \
	fi

.PHONY: docs-init
docs-init:
	@echo "🔍 Checking for Doxygen..."
	@if command -v doxygen >/dev/null 2>&1; then \
		echo "✅ Found doxygen: $$(which doxygen)"; \
		doxygen_cmd=doxygen; \
	elif [ -x /usr/local/bin/doxygen ]; then \
		echo "✅ Found doxygen: /usr/local/bin/doxygen"; \
		doxygen_cmd=/usr/local/bin/doxygen; \
	elif [ -x /usr/bin/doxygen ]; then \
		echo "✅ Found doxygen: /usr/bin/doxygen"; \
		doxygen_cmd=/usr/bin/doxygen; \
	else \
		echo "❌ Doxygen not found!"; \
		echo "📦 Please install Doxygen first"; \
		exit 1; \
	fi; \
	echo "📁 Creating docs directory..."; \
	mkdir -p docs; \
	echo "🔧 Generating default Doxygen configuration..."; \
	cd docs && $$doxygen_cmd -g Doxyfile; \
	echo "✅ Created docs/Doxyfile"; \
	echo ""; \
	echo "🔧 Applying recommended settings..."; \
	sed -i.bak \
		-e 's/^PROJECT_NAME[[:space:]]*=.*/PROJECT_NAME = "FreeBSD Web Server"/' \
		-e 's/^INPUT[[:space:]]*=.*/INPUT = ..\//' \
		-e 's/^RECURSIVE[[:space:]]*=.*/RECURSIVE = YES/' \
		-e 's/^FILE_PATTERNS[[:space:]]*=.*/FILE_PATTERNS = *.c *.h/' \
		-e 's/^EXCLUDE_PATTERNS[[:space:]]*=.*/EXCLUDE_PATTERNS = *\/build\/* *\/.*/' \
		-e 's/^GENERATE_LATEX[[:space:]]*=.*/GENERATE_LATEX = NO/' \
		-e 's/^EXTRACT_ALL[[:space:]]*=.*/EXTRACT_ALL = YES/' \
		-e 's/^EXTRACT_PRIVATE[[:space:]]*=.*/EXTRACT_PRIVATE = YES/' \
		-e 's/^EXTRACT_STATIC[[:space:]]*=.*/EXTRACT_STATIC = YES/' \
		docs/Doxyfile; \
	rm -f docs/Doxyfile.bak; \
	echo "🎉 Doxygen configuration initialized!"; \
	echo "📝 Edit docs/Doxyfile to customize further"; \
	echo "📚 Run 'make docs' to generate documentation"

.PHONY: docs-clean
docs-clean:
	@echo "🧹 Cleaning generated documentation..."
	@rm -rf docs/html docs/latex docs/man docs/xml
	@echo "✅ Documentation cleaned"
