//
//  prefixstream.c
//  hfsinspect
//
//  Created by Adam Knight on 11/30/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include <stdlib.h>
#include <stdint.h>
#include <sys/param.h>

#include <string.h>             // memcpy, strXXX, etc.
#if defined(__linux__)
    #include <bsd/string.h>     // strlcpy, etc.
#endif

#include "prefixstream.h"
#include "hfsinspect/cdefs.h"

struct prefixstream_context {
    FILE*   fp;
    char    prefix[80];
    uint8_t newline;
};

int prefixstream_close(void * context)
{
    FREE(context);
    return 0;
}

#if defined (BSD)
int prefixstream_write(void * c, const char * buf, int nbytes)
#else
ssize_t prefixstream_write(void * c, const char * buf, size_t nbytes)
#endif
{
    struct prefixstream_context *context = c;
    
    char *token, *string, *tofree;
    
    tofree = string = calloc(1, nbytes+1);
    memcpy(string, buf, nbytes);
    string[nbytes] = '\0';
    assert(string != NULL);
    assert(context->fp);
    
    while ( (token = strsep(&string, "\n")) != NULL) {
        
        if (context->newline && strlen(token)) {
            fprintf(context->fp, "%s ", (char*)context->prefix);
            context->newline = 0;
        }
        
        fprintf(context->fp, "%s", token);
        
        if ((token && string) || (string != NULL && strlen(string) == 0)) {
            fputc('\n', context->fp);
            context->newline = 1;
        }
    };
    
    FREE(tofree);
    
    return nbytes;
}

FILE* prefixstream(FILE* fp, char* prefix)
{
    struct prefixstream_context *context = NULL;
    ALLOC(context, sizeof(struct prefixstream_context));
    context->fp = fp;
    context->newline = 1;
    (void)strlcpy(context->prefix, prefix, 80);
    
#if defined(BSD)
    return funopen(context, NULL, prefixstream_write, NULL, prefixstream_close);
#else
    cookie_io_functions_t prefix_funcs = {
        NULL,
        prefixstream_write,
        NULL,
        prefixstream_close
    };
    return fopencookie(context, "w", prefix_funcs);
#endif
}
