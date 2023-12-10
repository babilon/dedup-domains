#ifndef LOG_DIAGNOSTICS_H
#define LOG_DIAGNOSTICS_H

extern void open_globalStdLog();
FILE *get_globalStdLog();
extern void close_globalStdLog();
extern void free_globalStdLog();

#ifdef COLLECT_DIAGNOSTICS
#define BORROW_SPACES \
	char *spaces_str = NULL; \
	do { \
		const char *tmp1 = __FILE__; \
		const char *tmp2 = __FUNCTION__; \
		size_t spaces = strlen(tmp1) + strlen(tmp2); \
		spaces_str = calloc(spaces + 1, sizeof(char)); \
		memset(spaces_str, ' ', spaces); \
		spaces_str[spaces] = '\0'; \
	} while(0)
#define RETURN_SPACES do { free(spaces_str); } while(0)
#endif

#define LOG_DIAG(clsname, ptr, fmt, ...) do { \
BORROW_SPACES; \
open_globalStdLog(); \
fprintf(get_globalStdLog(), "[%s:%s] %s %p\n" \
		" %s   " fmt, \
		__FILE__, __FUNCTION__, clsname, ptr, \
		spaces_str, __VA_ARGS__); \
close_globalStdLog(); \
RETURN_SPACES; \
} while(0)

#define LOG_DIAG_CONT(fmt, ...) do { \
BORROW_SPACES; \
open_globalStdLog(); \
fprintf(get_globalStdLog(), " %s   " fmt, \
		spaces_str, __VA_ARGS__); \
close_globalStdLog(); \
RETURN_SPACES; \
} while(0)

#define LOG_STR(fmt, ...) do { \
open_globalStdLog(); \
fprintf(get_globalStdLog(), "[%s:%s] " \
		fmt, \
		__FILE__, __FUNCTION__, \
		__VA_ARGS__); \
close_globalStdLog(); \
} while(0)

#endif
