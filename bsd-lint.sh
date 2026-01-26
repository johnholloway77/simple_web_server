#!/usr/bin/env bash

# BSD-friendly cpplint wrapper for FreeBSD KNF style
# Filters out Google C++ style warnings that conflict with BSD conventions

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}BSD KNF Style Linter${NC}"
echo "Filtering out Google C++ style conflicts..."
echo

# If no arguments provided, find all .c and .h files
if [ $# -eq 0 ]; then
    echo "Scanning all .c and .h files in current directory and subdirectories..."
    echo
    FILES=$(find . -name "*.c" -o -name "*.h" | grep -v "./simple_server/" | sort)
    if [ -z "$FILES" ]; then
        echo -e "${RED}No .c or .h files found${NC}"
        exit 1
    fi
    set -- $FILES
fi

# Run cpplint with BSD-appropriate filters
# Note: whitespace/comments is included because BSD uses tab-alignment, not 2+ spaces
cpplint \
  --filter=-whitespace/braces,-whitespace/newline,-readability/braces,-build/header_guard,-legal/copyright,-build/include_subdir,-whitespace/tab,-readability/casting \
  --linelength=80 \
  --counting=detailed \
  "$@"

exit_code=$?

echo
if [ $exit_code -eq 0 ]; then
    echo -e "${GREEN}✓ No BSD-relevant style issues found!${NC}"
else
    echo -e "${RED}⚠ Found style issues that should be addressed for BSD compliance${NC}"
    echo
    echo "Focus on these issue types:"
    echo -e "  • ${YELLOW}runtime/threadsafe_fn${NC} - Use reentrant functions (strtok_r, gmtime_r)"
    echo -e "  • ${YELLOW}whitespace/comments${NC} - Improve comment spacing (BSD uses tab alignment)"
    echo -e "  • ${YELLOW}whitespace/line_length${NC} - Keep lines ≤ 80 characters"
    echo -e "  • ${YELLOW}whitespace/semicolon${NC} - Use {} instead of ; for empty statements"
    echo -e "  • ${YELLOW}build/include${NC} - Include appropriate headers"
    echo
    echo "Ignore these (correct for BSD KNF):"
    echo "  • Brace placement warnings (functions should have braces on new lines)"
    echo "  • else/newline warnings (BSD allows else on separate lines)"
    echo "  • Header guard format (BSD uses different style)"
    echo
    echo "Note: BSD comment spacing uses tabs/alignment, not just 2+ spaces."
    echo "See struct examples in style(9) for proper comment alignment."
fi

exit $exit_code