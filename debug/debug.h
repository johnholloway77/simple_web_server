#ifdef DEBUG

#define DBG(...) printf("\033[33;1;4m\tDEBUG \033[0m" __VA_ARGS__)
#define DBG_DEC(decl) decl
#define DBG_DO(stmt)                                                           \
	do {                                                                   \
		stmt;                                                          \
	} while (0)
#else
#define DBG(...) ((void)0)
#define DBG_DEC(decl)
#define DBG_DO(stmt) ((void)0)
#endif
