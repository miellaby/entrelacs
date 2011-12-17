#ifndef __LOG_H__
#define __LOG_H__

#define LOG_OFF		  0
#define LOG_FATAL         1
#define LOG_ERROR	  2
#define LOG_WARN          3
#define LOG_INFO          4
#define LOG_DEBUG	  5

enum _log_facility
{
  LOG_GENERAL=0,
  LOG_MEM0,
  LOG_MEM,
  LOG_SPACE,
  LOG_MACHINE,
  LOG_FACILITY_COUNT
};

#ifdef __LOG_C__
char *level_name[] = {
        "off",
        "fatal",
        "error",
        "warn",
        "info",
        "debug",
        0
};

char *facility_name[] = {
        "general",
        "mem0",
        "mem",
        "space",
        "machine",
        0
};
#endif

extern int log_level[LOG_FACILITY_COUNT];
extern int log_init(const char *fname, const char *debug);
extern void log_msg(int level, enum _log_facility facility, char *fname, int lineno, char *fmt, ...);

#define DPRINTF(level, facility, fmt, arg...) { log_msg(level, facility, __FILE__, __LINE__, fmt, ##arg); }


#ifndef PRODUCTION
  #define ONDEBUG(w) w
#else
  #define ONDEBUG(w)
#endif

#define DEBUGPRINTF(fmt, arg...) ONDEBUG(DPRINTF(LOG_DEBUG, LOG_GENERAL, fmt, ##arg))


#endif /* __LOG_H__ */
