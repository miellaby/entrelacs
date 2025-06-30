#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define __LOG_C__
#include "log/log.h"

static FILE *log_fp = NULL;
#ifndef PRODUCTION
   static int default_log_level = LOG_INFO;
#else
   static int default_log_level = LOG_WARN;
#endif
int log_level[LOG_FACILITY_COUNT];

int log_verbose()
{
    for (int i=0; i<LOG_FACILITY_COUNT; i++)
        log_level[i] = LOG_DEBUG;
}

int log_init(const char *filename, const char *debug_str)
{
    int i;
    FILE *fp;

    for (i=0; i<LOG_FACILITY_COUNT; i++)
        log_level[i] = default_log_level;

    if (debug_str) {
        char *rhs, *lhs, *p;
        int n;
        int level, facility;
        rhs = lhs = (char*) debug_str;
        while (rhs && (rhs = strchr(rhs, '='))) {
            rhs++;

            p = strchr(rhs, ',');
            n = p ? p - rhs : strlen(rhs);
            for (level=0; level_name[level]; level++) {
                if (!(strncasecmp(level_name[level], rhs, n)))
                    break;
            }
            rhs = p;
            if (!level_name[level]) {
                // unknown level
                continue;
            }

            do {
                if (*lhs==',') lhs++;
                p = strpbrk(lhs, ",=");
                n = p ? p - lhs : strlen(lhs);
                for (facility=0; facility_name[facility]; facility++) {
                    if (!(strncasecmp(facility_name[facility], lhs, n)))
                        break;
                }
                if (facility_name[facility]) {
                    log_level[facility] = level;
                }
                lhs = p;
            } while (*lhs && *lhs==',');
            lhs = rhs;
        }
    }

    if (!filename) // use default i.e. stdout
        return 0;

    if (!(fp = fopen(filename, "a")))
        return 1;
    log_fp = fp;
    return 0;
}

void log_msg(int level, enum _log_facility facility, char *fname, int lineno, char *fmt, ...)
{
    char* errbuf;
    va_list ap;
    time_t t;
    struct tm *tm;
    if (level && level>log_level[facility] && level>LOG_FATAL)
        return;

    if (!log_fp)
        log_fp = stdout;

    // user log
    va_start(ap, fmt);
    //vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    if (vasprintf(&errbuf, fmt, ap) == -1) {
        va_end(ap);
        return;
    }
    va_end(ap);

    // timestamp
    t = time(NULL);
    tm = localtime(&t);
    fprintf(log_fp, "[%04d/%02d/%02d %02d:%02d:%02d] ",
            tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);

    if (level)
        fprintf(log_fp, "%s:%d: %s: %s\n", fname, lineno, level_name[level], errbuf);
    else
        fprintf(log_fp, "%s:%d: %s\n", fname, lineno, errbuf);
    fflush(log_fp);
    free(errbuf);

    if (level==LOG_FATAL)
        exit(-1);

    return;
}
