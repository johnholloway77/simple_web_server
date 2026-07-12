/**
 * @file debug.h
 * @brief Compile-time debug output macros.
 *
 * All three macros compile away to nothing unless the translation unit is
 * built with -DDEBUG (i.e. `make DEBUG=1`).  This ensures zero runtime
 * overhead in release builds.
 *
 * Usage:
 * @code
 *   DBG("accepted fd %d from %s\n", newfd, addr);   // printf-style
 *   DBG_DEC(int tmp = expensive_debug_value());      // debug-only declaration
 *   DBG_DO(dump_state(&conn));                        // debug-only statement
 * @endcode
 */

#ifdef DEBUG

/** @brief Print a yellow-highlighted debug line to stdout (printf-style). */
#define DBG(...) printf("\033[33;1;4m\tDEBUG \033[0m" __VA_ARGS__)

/** @brief Declare a variable or expression only in debug builds. */
#define DBG_DEC(decl) decl

/** @brief Execute a statement only in debug builds (wrapped in do-while). */
#define DBG_DO(stmt)                                                           \
	do {                                                                   \
		stmt;                                                          \
	} while (0)
#else
#define DBG(...) ((void)0)
#define DBG_DEC(decl)
#define DBG_DO(stmt) ((void)0)
#endif
