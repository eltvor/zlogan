#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#ifdef LOG_SYSLOG
#include <syslog.h>
#endif

#define L_FATAL 0
#define L_ERR   1
#define L_WARN  2
#define L_NOTE  3
#define L_INFO  4
#define L_DEBUG 5

void log_level(unsigned level);
void log_wr_begin(unsigned level);
void log_wr_frag(unsigned level, const char *fmt, ...);
void log_wr_end(unsigned level);
void log_wr(unsigned level, const char *fmt, ...);
void log_std_error(unsigned level, char *s);
int log_stream_add(FILE *f);
int log_stream_delete(FILE *f);
FILE *log_open(char *name, char *date_fmt, char *suffix_fmt,
	       unsigned timeout_sec);
#ifdef LOG_SYSLOG
void log_syslog_add(const char *ident, int option, int facility);
void log_syslog_delete();
#endif


#endif /* LOG_H */
