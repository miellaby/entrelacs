/** @file log.h
 Entrelacs log internal log sytem.
 */

#ifndef __LOG_H__
#define __LOG_H__

#define LOG_OFF		  0
#define LOG_FATAL         1
#define LOG_ERROR	  2
#define LOG_WARN          3
#define LOG_INFO          4
#define LOG_TRACE	  5
#define LOG_DEBUG	  6

enum _log_facility
{
  LOG_GENERAL=0,
  LOG_MEM0,
  LOG_MEM,
  LOG_SPACE,
  LOG_MACHINE,
  LOG_SESSION,
  LOG_SERVER,
  LOG_RAM,
  LOG_BUFFER,
  LOG_FACILITY_COUNT
};
#ifndef LOG_CURRENT
#define LOG_CURRENT LOG_GENERAL
#endif

#ifdef __LOG_C__

char *facility_name[] = {
    "general",
    "mem0",
    "mem",
    "space",
    "machine",
    "session",
    "server",
    "ram",
    "buffer",
    0
};

char *level_name[] = {
    "off",
    "fatal",
    "error",
    "warn",
    "info",
    "trace",
    "debug",
    0
};

#endif

extern int log_level[LOG_FACILITY_COUNT];
extern int log_init(const char *filename, const char *debug_str);
extern void log_msg(int level, enum _log_facility facility, char *fname, int lineno, char *fmt, ...);

#define LOGPRINTF(level, fmt, arg...) { log_msg(level, LOG_CURRENT, __FILE__, __LINE__, fmt, ##arg); }


#ifdef DEBUG
  #define ONDEBUG(w) w
#else
  #define ONDEBUG(w)
#endif


#ifdef PRODUCTION
#define FATALPRINTF(format, arg...) (void)(0)
#define ERRORPRINTF(format, arg...) (void)(0)
#define WARNPRINTF(format, arg...) (void)(0)
#define INFOPRINTF(format, arg...) (void)(0)
#define TRACEPRINTF(format, arg...) (void)(0)
#define DEBUGPRINTF(format, arg...) (void)(0)
#else
#define FATALPRINTF(format, arg...) LOGPRINTF(LOG_FATAL, format, ##arg)
#define ERRORPRINTF(format, arg...) LOGPRINTF(LOG_ERROR, format, ##arg)
#define WARNPRINTF(format, arg...) LOGPRINTF(LOG_WARN, format, ##arg)
#define INFOPRINTF(format, arg...) LOGPRINTF(LOG_INFO, format, ##arg)
#ifdef DEBUG
#define TRACEPRINTF(format, arg...) LOGPRINTF(LOG_TRACE, format, ##arg)
#define DEBUGPRINTF(format, arg...) LOGPRINTF(LOG_DEBUG, format, ##arg)
#else
#define TRACEPRINTF(format, arg...) LOGPRINTF(LOG_TRACE, format, ##arg)
#define DEBUGPRINTF(format, arg...) (void)(0)
#endif
#endif

#define dputs(format, arg...) DEBUGPRINTF(format, ##arg)

#endif /* __LOG_H__ */
