#ifndef COMMON_H
#define COMMON_H

#define mssleep(ms) usleep(ms * 1000)
#define TRUE 1
#define FALSE 0

#ifdef DEBUG
#define DBGLN(...) printf("%s:%d ",  __FILE__, __LINE__);  printf(__VA_ARGS__)
#define DBG(...) printf(__VA_ARGS__)
#else
#define DBGLN(...) 
#define DBG(...) 
#endif

extern unsigned char opt_verbose;
#define VERBOSE(...) if (opt_verbose) printf(__VA_ARGS__)

#define MAX(a, b) ((a) > (b)) ? (a): (b)
#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#define ABS(a) ((a) > 0.0) ? (a) : (0.0 - (a))

#endif
