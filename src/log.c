#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include "log.h"

static enum log_level log_level = LL_INFO;
static int use_syslog = 0;

static void log_vprintf(enum log_level level, const char *fmt, va_list args) {
	const char *level_str;

	if (level > log_level) {
		return;
	}

	switch (level) {
	case LL_DEBUG:
		level_str = "DEBUG";
		break;
	case LL_INFO:
		level_str = "INFO";
		break;
	case LL_WARNING:
		level_str = "WARNING";
		break;
	case LL_ERROR:
		level_str = "ERROR";
		break;
	case LL_FATAL:
		level_str = "FATAL";
		break;
	default:
		level_str = "UNKNOWN";
		break;
	}
	fprintf(stderr, "[%s] ", level_str);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");

	if (use_syslog) {
		vsyslog(LOG_INFO, fmt, args);
	}
}

void log_set_level(enum log_level level) {
	log_level = level;
}

void log_use_syslog(int use) {
	use_syslog = use;
	openlog("node-whisperer", 0, LOG_USER);
}

void log_error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	log_vprintf(LL_ERROR, fmt, args);
	va_end(args);
}

void log_warning(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	log_vprintf(LL_WARNING, fmt, args);
	va_end(args);
}

void log_info(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	log_vprintf(LL_INFO, fmt, args);
	va_end(args);
}

void log_debug(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	log_vprintf(LL_DEBUG, fmt, args);
	va_end(args);
}
