#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#include "log.h"

#define L_FATAL 0
#define L_ERR   1
#define L_WARN  2
#define L_NOTE  3
#define L_INFO  4
#define L_DEBUG 5

static const char * const log_label[] = {
  "fatal",
  "error",
  "warning",
  "notice",
  "info",
  "debug",
};

#ifdef LOG_SYSLOG
static const int syslog_lvl[] = {
  LOG_EMERG,
  LOG_ERR,
  LOG_WARNING,
  LOG_NOTICE,
  LOG_INFO,
  LOG_DEBUG,
};
static int syslog_active = 0;
#endif

static unsigned log_level_thr = 0;
static FILE **log_file = NULL;
static int n_log_files = 0;
static pthread_mutex_t log_file_lock = PTHREAD_MUTEX_INITIALIZER;

void check(void *ptr) {
  if (ptr)
    return;
  fprintf(stderr, "error: could not realloc(); logging disabled\n");
  n_log_files = 0;
}

void log_level(unsigned level) {
  log_level_thr = level;
}

void log_wr_begin(unsigned level) {
  struct timeval tv;
  struct tm *tm;
  int i;

  if (level > log_level_thr)
    return;
  gettimeofday(&tv, NULL);
  tm = localtime(&tv.tv_sec);
  pthread_mutex_lock(&log_file_lock);
  for (i = 0; i < n_log_files; i++)
    fprintf(log_file[i], "%04d-%02d-%02d %02d:%02d:%02d.%06d  %s: ",
            1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec, (int)tv.tv_usec,
            log_label[level]);
}

void log_wr_end(unsigned level) {
  int i;

  if (level > log_level_thr)
    return;
  for (i = 0; i < n_log_files; i++) {
    fputc('\n', log_file[i]);
    fflush(log_file[i]);
  }
  pthread_mutex_unlock(&log_file_lock);
}

void log_wr_frag(unsigned level, const char *fmt, ...) {
  va_list ap;
  int i;

  if (level > log_level_thr)
    return;
  for (i = 0; i < n_log_files; i++) {
    va_start(ap, fmt);
    vfprintf(log_file[i], fmt, ap);
    va_end(ap);
  }
#ifdef LOG_SYSLOG
  if (syslog_active) {
    va_start(ap, fmt);
    vsyslog(syslog_lvl[level], fmt, ap);
    va_end(ap);
  }
#endif
}

void log_wr(unsigned level, const char *fmt, ...) {
  va_list ap;
  int i;

  if (level > log_level_thr)
    return;
  log_wr_begin(level);
  for (i = 0; i < n_log_files; i++) {
    va_start(ap, fmt);
    vfprintf(log_file[i], fmt, ap);
    va_end(ap);
  }
  log_wr_end(level);
#ifdef LOG_SYSLOG
  if (syslog_active) {
    va_start(ap, fmt);
    vsyslog(syslog_lvl[level], fmt, ap);
    va_end(ap);
  }
#endif
}

void log_std_error(unsigned level, char *s) {
  char out[256];
  int max = sizeof(out);

  strncpy(out, s, max-3);
  strcat(out, ": ");
  max -= strlen(out);
  strncat(out, strerror(errno), max-1);
  log_wr(level, out);
}

#ifdef LOG_SYSLOG
void log_syslog_add(const char *ident, int option, int facility) {
  openlog(ident, option, facility);
  syslog_active = 1;
}

void log_syslog_delete() {
  closelog();
  syslog_active = 0;
}
#endif

int log_stream_add(FILE *f) {
  int i;
  if (!f)
    return -1;
  pthread_mutex_lock(&log_file_lock);
  for (i = 0; i < n_log_files; i++)
    if (log_file[i] == f)
      return -1;
  ++n_log_files;
  log_file = (FILE**)realloc(log_file, n_log_files*sizeof(FILE*));
  check(log_file);
  log_file[n_log_files-1] = f;
  pthread_mutex_unlock(&log_file_lock);
  return 0;
}

int log_stream_delete(FILE *f) {
  int i, rc = -1;
  if (!f)
    return -1;
  pthread_mutex_lock(&log_file_lock);
  for (i = 0; i < n_log_files; i++)
    if (log_file[i] == f) {
      for (i++ ; i < n_log_files; i++)
        log_file[i-1] = log_file[i];
      --n_log_files;
      log_file = (FILE**)realloc(log_file, n_log_files*sizeof(FILE*));
      check(log_file);
      rc = 0;
      break;
    }
  pthread_mutex_unlock(&log_file_lock);
  return rc;
}

/**
 * create or just open logfile
 * name is composed of name, followed possibly by a timestamp, followed
 * possibly by a numeric suffix, if exclusivity is required
 * @param name base file name
 */
FILE *log_open(char *name, char *date_fmt, char *suffix_fmt,
               unsigned timeout_sec)
{
  char s[256], *p;
  time_t t0;
  struct tm *tm;
  int fd, i, n, max = sizeof(s);

  time(&t0);
  s[sizeof(s)-1] = '\0';
  strncpy(s, name, max-1);
  n = strlen(s);
  p = s + n;
  max -= n;

  if (date_fmt && strlen(date_fmt)) {
    tm = localtime(&t0);
    n = strftime(p, max, date_fmt, tm);
    p += n;
    max -= n;
    p[0] = '\0';
  }

  if (suffix_fmt && strlen(suffix_fmt)) {
    i = 1;
    do {
      snprintf(p, max, suffix_fmt, i++);
      fd = open(s, O_CREAT | O_EXCL | O_WRONLY, 0644);
      if (fd >= 0) {
        fprintf(stderr, "Opening exclusive log file %s\n", s);
        FILE *f = fdopen(fd, "w");
        return f;
      }
    } while (difftime(time(NULL), t0) < timeout_sec);
    fprintf(stderr, "Failed to create unique log file within %ds\n",
            timeout_sec);
  }

  fprintf(stderr, "Opening log file %s\n", s);
  return fopen(s, "a");
}
