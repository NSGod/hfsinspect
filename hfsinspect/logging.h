//
//  logging.h
//  hfsinspect
//
//  Created by Adam Knight on 5/30/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#ifndef hfsinspect_logging_h
#define hfsinspect_logging_h

#include "debug.h"      //print_trace
#include <stdio.h>      //FILE*
#include <stdbool.h>    //bool
#include <signal.h>     //raise

#define print(...)        PrintLine(stderr, L_STANDARD, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define critical(...)   { PrintLine(stderr, L_CRITICAL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__); if (DEBUG) { print_trace(NULL); raise(SIGTRAP); } exit(1); }
#define error(...)      { PrintLine(stderr, L_ERROR, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__); if (DEBUG) { print_trace(NULL); raise(SIGTRAP); } exit(1); }
#define warning(...)    { PrintLine(stderr, L_WARNING, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__); if (DEBUG) { print_trace(NULL); } }
#define info(...)       if (DEBUG) { PrintLine(stderr, L_INFO, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__); }
#define debug(...)      if (DEBUG) { PrintLine(stderr, L_DEBUG, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__); }

#define fatal(...)      { fprintf(stderr, __VA_ARGS__); fputc('\n', stdout); usage(1); }

extern bool DEBUG;

enum LogLevel {
    L_CRITICAL = 0,
    L_ERROR,
    L_WARNING,
    L_STANDARD,
    L_INFO,
    L_DEBUG
};

void _print_reset(FILE* f);
void _print_color(FILE* f, uint8_t red, uint8_t green, uint8_t blue, bool background);

void PrintLine(FILE* f, enum LogLevel level, const char* file, const char* function, unsigned int line, const char* format, ...);

#endif
