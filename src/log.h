#pragma once

enum log_level {
    LL_DEBUG = 5,
    LL_INFO = 4,
    LL_WARNING = 3,
    LL_ERROR = 2,
    LL_FATAL = 1,
};

void log_set_level(enum log_level level);
void log_use_syslog(int use);

void log_error(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_debug(const char *fmt, ...);
