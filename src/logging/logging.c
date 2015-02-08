//
//  logging.c
//  hfsinspect
//
//  Created by Adam Knight on 5/30/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <libgen.h>
#include <signal.h>         //raise
#include <sys/param.h>      //MIN/MAX

#include <string.h>         // memcpy, strXXX, etc.
#if defined(__linux__)
    #include <bsd/string.h> // strlcpy, etc.
#endif

#include "logging.h"
#include "prefixstream.h"
#include "nullstream.h"
#include "memdmp/output.h"

bool DEBUG = false;

struct _colorState {
    struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t white;
    } foreground;
    struct {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t white;
    } background;
};
typedef struct _colorState colorState;


void _printColor(FILE* f, unsigned level)
{
    if (_useColor(f) == false) return;

    // Critical:    always white on red
    colorState critical = { { .white = 23 }, { .red = 5 } };

    // Error:       always red
    colorState error    = { { .red = 5 }, { 0 } };

    // Warning:     always orange
    colorState warning  = { { 5, 1, 0}, { 0 } };

    // Info:        shades of green
    colorState info     = { { 1, 5, 1}, { 0 } };

    // Debug:       shades of blue
    colorState debug    = { { 2, 2, 4}, { 0 } };

    //    const int max_depth = 15;
    //
    //    int depth = stack_depth(max_depth);
    //
    //    float colorMap[max_depth][3] = {
    //        {1, 1, 1}, //0 - start()
    //        {1, 1, 1}, //1 - main()
    //        {1, 1, 5}, //2 - calls from main()
    //        {1, 2, 1}, //3 - functions or libraries
    //        {1, 3, 1},
    //
    //        {1, 4, 1}, //5
    //        {1, 5, 1},
    //        {1, 1, 2},
    //        {1, 1, 3},
    //        {1, 1, 4},
    //
    //        {1, 1, 5}, //10
    //        {2, 1, 1},
    //        {3, 1, 1},
    //        {4, 1, 1},
    //        {5, 1, 1},
    //    };

    colorState current = { { .white = 23 }, { 0 } };
    switch (level) {
        case L_CRITICAL:
        {
            current = critical;
            break;
        }

        case L_ERROR:
        {
            current = error;
            break;
        }

        case L_WARNING:
        {
            current = warning;
            break;
        }

        case L_INFO:
        {
            current = info;
            break;
        }

        case L_DEBUG:
        {
            current = debug;
            break;
        }

        default:
        {
            break;
        }
    }

    _print_reset(f);
    if (current.foreground.white) {
        _print_gray(f, current.foreground.white, 0);
    } else {
        _print_color(f, current.foreground.red, current.foreground.green, current.foreground.blue, 0);
    }
    if (current.background.white) {
        _print_gray(f, current.background.white, 1);
    } else {
        _print_color(f, current.background.red, current.background.green, current.background.blue, 1);
    }
}

int LogLine(enum LogLevel level, const char* format, ...)
{
    va_list      argp;
    va_start(argp, format);

    int          nchars      = 0;

    static FILE* streams[6]  = {0};

#define USE_EMOJI 0

    char*        prefixes[6] = {
#if USE_EMOJI
        "❕",
        "⛔️",
        "🚸 ",
        "🆗 ",
        "ℹ️",
        "🐞 ",
#else
        "[CRIT] ",
        "[ERR] ",
        "[WARN] ",
        "  ",
        "[INFO] ",
        "[DEBUG] ",
#endif
    };

    if (streams[0] == NULL) {
        streams[L_CRITICAL] = prefixstream(stderr, prefixes[L_CRITICAL]);
        streams[L_ERROR]    = prefixstream(stderr, prefixes[L_ERROR]);
        streams[L_WARNING]  = prefixstream(stderr, prefixes[L_WARNING]);
        streams[L_STANDARD] = prefixstream(stdout, prefixes[L_STANDARD]);

        if (DEBUG) {
            streams[L_INFO]  = prefixstream(stderr, prefixes[L_INFO]);
            streams[L_DEBUG] = prefixstream(stderr, prefixes[L_DEBUG]);
        } else {
            streams[L_INFO]  = nullstream();
            streams[L_DEBUG] = nullstream();
        }
    }

    FILE* fp = stdout;

    if (level <= L_DEBUG)
        fp = streams[level];
    else
        fprintf(stderr, "Uh oh. %u isn't a valid log level.", level);

    _printColor(fp, level);
    nchars += vfprintf(fp, format, argp);
    fputc('\n', fp);
    _print_reset(fp);

    va_end(argp);

    if ( DEBUG && (level <= L_WARNING) ) {
        print_trace(fp, 2);
    }

    fflush(fp);

    if (DEBUG && (level <= L_CRITICAL)) {
        raise(SIGTRAP);
    }

    if (!DEBUG && (level == L_CRITICAL))
        abort();

    return nchars;
}

#define PrintLine_MAX_LINE_LEN 1024
#define PrintLine_MAX_DEPTH    40
int PrintLine(enum LogLevel level, const char* file, const char* function, unsigned int line, const char* format, ...)
{
    va_list      argp;
    va_start(argp, format);

    int          out_bytes = 0;
    char         inputstr[PrintLine_MAX_LINE_LEN];
    unsigned int depth     = stack_depth(PrintLine_MAX_DEPTH) - 1; // remove our frame
    char         indent[PrintLine_MAX_LINE_LEN];


    // "Print" the input format string to a string.
    if (argp != NULL)
        vsnprintf((char*)&inputstr, PrintLine_MAX_LINE_LEN, format, argp);
    else
        (void)strlcpy(inputstr, format, PrintLine_MAX_LINE_LEN);

    // Indent the string with the call depth.
    depth = MIN(depth, PrintLine_MAX_DEPTH);

    // Fill the indent string with spaces.
    memset(indent, ' ', depth);

    // Mark the current depth.
    indent[depth] = '\0';

    // Append the text after the indentation.
    (void)strlcat(indent, inputstr, PrintLine_MAX_LINE_LEN);

    // Now add our format to that string and print to stdout.
    if (DEBUG && (level != L_STANDARD))
        out_bytes += LogLine(level, "%-80s [%s:%u %s()]", indent, basename((char*)file), line, function);
    else
        out_bytes += LogLine(level, "%s", inputstr);

    // Clean up.
    va_end(argp);

    return out_bytes;
}

