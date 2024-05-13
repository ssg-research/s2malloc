/*  debug.h
 *  Adopted from SlimGuard
 *  Copyright (c) 2019, Beichen Liu, Virginia Tech
 *  All rights reserved
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#define RED     "\x1b[31m"
#define YELLOW  "\x1b[33m"
#define GREEN   "\x1b[32m"
#define END     "\x1b[0m"

#ifdef DEBUG // debug only
#define Debug(fmt, ...) \
  do { \
    fprintf(stderr, YELLOW "[ln %d] in %s " fmt END, \
             __LINE__, __func__,  __VA_ARGS__); \
  } while(0)
#else
#define Debug(...)
#endif // end debug

#ifdef DEBUG // debug only
#define Canary(fmt, ...) \
  do { \
    fprintf(stderr, GREEN "%s " fmt END, __func__, __VA_ARGS__); \
  } while(0) /* canary related print */
#else
#define Canary( ...)
#endif // end debug

#define Error(fmt, ...) \
  do{ \
    fprintf(stderr, RED "ERROR: %s #%d " fmt END, \
           __func__, __LINE__,  __VA_ARGS__); \
  } while(0)

#define Print(fmt, ...) \
  do { \
    frpintf(stderr, "[%d:] %s ", \
            __LINE__ , __func__, __VA_ARGS__); \
  } while(0)

#ifdef ASSERT
#include <assert.h>
#define Assert(truth, message)                                                                      \
	do {                                                                                                     \
		if (!(truth)) {                                                                                      \
            assert((truth) && message);                                                                      \
		}                                                                                                    \
	} while (0)
#else
#define Assert(truth, message) do {} while(0)
#endif

#endif /* DEBUG_H */
