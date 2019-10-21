#ifndef ERROR_REPORTING_H
#define ERROR_REPORTING_H

#ifdef NO_USE_SYSLOG
#include <err.h>
# define LOG_PRINT_DEBUG(txt, ...)   warnx("DEBUG: "     txt " (at %s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)
# define LOG_PRINT_NOTICE(txt, ...)  warnx("NOTICE"      txt " (at %s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)
# define LOG_PRINT_INFO(txt, ...)    warnx("INFO: "      txt " (at %s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)
# define LOG_PRINT_WARN(txt, ...)    warnx("WARNING: "   txt " (at %s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)
# define LOG_PRINT_ERROR(txt, ...)   warnx("ERROR: "     txt " (at %s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)
# define LOG_PRINT_CRIT(txt, ...)    warnx("CRITICAL: "  txt " (at %s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)
#else
# include <syslog.h>
# define LOG_PRINT_DEBUG(...)   syslog(LOG_DEBUG,    __VA_ARGS__)
# define LOG_PRINT_NOTICE(...)  syslog(LOG_NOTICE,   __VA_ARGS__)
# define LOG_PRINT_INFO(...)    syslog(LOG_INFO,     __VA_ARGS__)
# define LOG_PRINT_WARN(...)    syslog(LOG_WARNING,  __VA_ARGS__)
# define LOG_PRINT_ERROR(...)   syslog(LOG_ERR,      __VA_ARGS__)
# define LOG_PRINT_CRIT(...)    syslog(LOG_CRIT,     __VA_ARGS__)
#endif

#endif
