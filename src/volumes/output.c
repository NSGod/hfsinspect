//
//  output.c
//  volumes
//
//  Created by Adam Knight on 11/10/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include <time.h>           // gmtime

#include "output.h"

#include "logging/logging.h"    // console printing routines
#include "memdmp/memdmp.h"
#include "memdmp/output.h"

out_ctx OCMake(bool decimal_sizes, unsigned indent_step, char* prefix)
{
    out_ctx ctx = {
        .decimal_sizes = decimal_sizes,
        .wideAttributes = false,
        .indent_level  = 0,
        .indent_step   = 2,
        .prefix        = prefix,
        .indent_string = ""
    };
    return ctx;
}

void OCSetIndentLevel(out_ctx* ctx, unsigned new_level)
{
    ctx->indent_level = new_level;

    if (new_level > (sizeof(ctx->indent_string) - 1))
        new_level = 0;

    memset(ctx->indent_string, ' ', sizeof(ctx->indent_string));
    ctx->indent_string[new_level] = '\0';
}

#pragma mark - Print Functions

int BeginSection(out_ctx* ctx, const char* format, ...)
{
    va_list argp;
    va_start(argp, format);

    Print(ctx, "%s", "");
    char    str[255];
    vsprintf(str, format, argp);
    int     bytes = Print(ctx, "# %s", str);

    OCSetIndentLevel(ctx, ctx->indent_level + ctx->indent_step);

    va_end(argp);
    return bytes;
}

void EndSection(out_ctx* ctx)
{
    if (ctx->indent_level > 0)
        OCSetIndentLevel(ctx, ctx->indent_level - MIN(ctx->indent_level, ctx->indent_step));
}

int Print(out_ctx* ctx, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);

    fputs(ctx->indent_string, stdout);
    int     bytes = vfprintf(stdout, format, ap);
    fputs("\n", stdout);

    va_end(ap);
    return bytes;
}

int PrintAttribute(out_ctx* ctx, const char* label, const char* format, ...)
{
    va_list argp;
    va_start(argp, format);

    int     bytes  = 0;
    char    str[PATH_MAX+1];
    char    spc[2] = {' ', '\0'};

    vsprintf(str, format, argp);

    if (!ctx->wideAttributes) {
        if (label == NULL)
            bytes = Print(ctx, "%-23s. %s", "", str);
        else if ( memcmp(label, spc, 2) == 0 )
            bytes = Print(ctx, "%-23s  %s", "", str);
        else
            bytes = Print(ctx, "%-23s= %s", label, str);
    } else {
        if (label == NULL)
            bytes = Print(ctx, "%-30s. %s", "", str);
        else if ( memcmp(label, spc, 2) == 0 )
            bytes = Print(ctx, "%-30s  %s", "", str);
        else
            bytes = Print(ctx, "%-30s= %s", label, str);
    }

    va_end(argp);
    return bytes;
}

#pragma mark Line Print Functions

void _PrintDataLength(out_ctx* ctx, const char* label, uint64_t size)
{
    assert(label != NULL);

    char decimalLabel[50];

    (void)format_size(ctx, decimalLabel, size, 50);

    if (size > 1024) {
        PrintAttribute(ctx, label, "%s (%llu bytes)", decimalLabel, size);
    } else {
        PrintAttribute(ctx, label, "%s", decimalLabel);
    }
}

int _PrintUIChar(out_ctx* ctx, const char* label, uint64_t value, size_t nbytes)
{
    assert(nbytes >= 2 && nbytes <= 8);

    char str[50] = {0};

    (void)format_uint_chars(str, value, nbytes, 50);

    return PrintAttribute(ctx, label, "%#llx ('%s')", value, str);
}

void _PrintUIBinary(out_ctx* ctx, const char* label, uint64_t value, size_t nbytes)
{
    assert(nbytes >= 2 && nbytes <= 8);

    char str[65] = {0};

    int len = format_uint_binary(str, value, nbytes, 65);

#define segmentLength 32

    for (size_t i = 0; i < len; i += segmentLength) {
        char segment[segmentLength+1]; memset(segment, '\0', segmentLength+1);

        (void)strlcpy(segment, &str[i], MIN(segmentLength+1, (len - i)+1));

        PrintAttribute(ctx, label, "%s", segment);

        if (i == 0) label = " ";
    }
}

void _PrintHexData(out_ctx* ctx, const char* label, const void* map, size_t nbytes)
{
    assert(label != NULL);
    assert(map != NULL);
    assert(nbytes > 0);

#define segmentLength 32
    char* str       = NULL;
    ssize_t len     = 0;
    ssize_t msize   = 0;

    msize = format_hex_data(NULL, map, nbytes, 0);
    if (msize < 0) { perror("format_hex_data"); return; }
    msize++; // NULL terminator
    SALLOC(str, msize);

    if ( (len = format_hex_data(str, map, nbytes, msize)) < 0) {
        SFREE(str);
        warning("format_hex_data failed!");
        return;
    }

    for (size_t i = 0; i < len; i += segmentLength) {
        char segment[segmentLength+1]; memset(segment, '\0', segmentLength+1);

        (void)strlcpy(segment, &str[i], MIN(segmentLength+1, (len - i)+1));

        PrintAttribute(ctx, label, "0x%s", segment);
        if (i == 0) label = " ";
    }
}

void VisualizeData(const void* data, size_t length)
{
    // Init the last line to something unlikely so a zero line is shown.
    memdmp_state state = { .prev = "5432" };

    memdmp(stdout, data, length, NULL, &state);
}

#pragma mark - Formatters

int format_size(out_ctx* ctx, char* out, uint64_t value, size_t length)
{
    char*       binaryNames[]  = { "bytes", "KiB", "MiB", "GiB", "TiB", "EiB", "PiB", "ZiB", "YiB" };
    char*       decimalNames[] = { "bytes", "KB", "MB", "GB", "TB", "EB", "PB", "ZB", "YB" };
    float       divisor        = 1024.0f;
    bool        decimal        = ctx->decimal_sizes;
    long double displaySize    = value;
    int         count          = 0;
    char*       sizeLabel;

    assert(ctx != NULL);
    assert(out != NULL);
    assert(length > 0);

    if (decimal) divisor = 1000.0f;

    while (count < 9) {
        if (displaySize < divisor) break;
        displaySize /= divisor;
        count++;
    }

    if (decimal) sizeLabel = decimalNames[count]; else sizeLabel = binaryNames[count];

    if (count > 0) {
        snprintf(out, length, "%0.2Lf %s", displaySize, sizeLabel);
    } else {
        snprintf(out, length, "%0.Lf %s", displaySize, sizeLabel);
    }

    return (int)strlen(out);
}

int format_blocks(out_ctx* ctx, char* out, uint64_t blocks, size_t block_size, size_t length)
{
    uint64_t displaySize = (blocks * block_size);
    char   sizeLabel[50];
    (void)format_size(ctx, sizeLabel, displaySize, 50);
    return snprintf(out, length, "%s (%llu blocks)", sizeLabel, blocks);
}

int format_time(out_ctx* ctx, char* out, time_t gmt_time, size_t length)
{
    struct tm* t = gmtime(&gmt_time);
#if defined(__APPLE__)
    return (int)strftime(out, length, "%c %Z", t);
#else
    return (int)strftime(out, length, "%c", t);
#endif
}

int format_uint_oct(char* out, uint64_t value, uint8_t padding, size_t length)
{
    char format[100];
    snprintf(format, 100, "0%%%ullo", padding);
    return snprintf(out, length, format, value);
}

int format_uint_dec(char* out, uint64_t value, uint8_t padding, size_t length)
{
    char format[100];
    snprintf(format, 100, "%%%ullu", padding);
    return snprintf(out, length, format, value);
}

int format_uint_hex(char* out, uint64_t value, uint8_t padding, size_t length)
{
    char format[100];
    snprintf(format, 100, "0x%%%ullx", padding);
    return snprintf(out, length, format, value);
}

int format_uuid(char* out, const unsigned char value[16])
{
    uuid_unparse(value, out);
    return (int)strlen(out);
}

int format_uint_chars(char* out, uint64_t value, size_t nbytes, size_t length)
{
    const char* chars = (const char*)&value;
#if defined(__LITTLE_ENDIAN__)
    while (nbytes--) *out++ = chars[nbytes];
    *out++      = '\0';
#elif defined(__BIG_ENDIAN__)
    memcpy(out, chars + (8 - nbytes), nbytes);
    out[nbytes] = '\0';
#else
#error unknown byte order!
#endif
    return (int)strlen(out);
}

int format_uint_binary(char* out, uint64_t value, size_t nbytes, size_t length)
{
    // Determine the size of the result string.
    size_t rlength = (nbytes * 8);

    // Return the size if the output buffer is empty.
    if (out == NULL) return (int)rlength;

    // Zero length buffer == zero length result.
    if (nbytes == 0) return 0;

    // Ensure we have been granted enough space to do this.
    if (rlength > length) { errno = ENOMEM; return -1; }

    /* Process the string */

    // Init the string's bytes to the character zero so that positions we don't write to have something printable.
    // THIS IS INTENTIONALLY NOT '\0'!!
    memset(out, '0', rlength);

    const uint8_t* data = (const uint8_t*)&value;

    // We build the result string from the tail, so here's the index in that string, starting at the end.
    ssize_t ridx = rlength - 1;
    ssize_t byteIndex = 0;
#if defined(__LITTLE_ENDIAN__)
    for (byteIndex = 0; byteIndex < nbytes; byteIndex++) {
#elif defined(__BIG_ENDIAN__)
    for (byteIndex = 7; byteIndex > (7 - (ssize_t)nbytes); byteIndex--) {
#else
#error unknown byte order!
#endif
        uint8_t chr = data[byteIndex];
        int8_t bitIndex = 7;
        while (bitIndex >= 0 && ridx >= 0) {
            uint8_t idx = chr % 2;
            out[ridx] = (idx ? '1' : '0');
            chr /= 2;
            ridx--;
            bitIndex--;
        }
    }

    // Cap the string.
    out[rlength] = '\0';

    return (int)rlength;
}

ssize_t format_hex_data(char* restrict out, const void* data, size_t nbytes, size_t length)
{
    // Determine the size of the result string.
    size_t rlength = (nbytes * 2);

    // Return the size if the output buffer is empty.
    if (out == NULL) return rlength;

    // Zero length buffer == zero length result.
    if (nbytes == 0) return 0;

    // No source buffer?
    if (data == NULL) { errno = EINVAL; return -1; }

    // Ensure we have been granted enough space to do this.
    if (rlength > length) { errno = ENOMEM; return -1; }

    /* Process the string */

    for (size_t i = 0; i < nbytes; i++) {
        uint8_t chr = ((uint8_t *)data)[i];
        sprintf(&out[i * 2], "%02x", chr);
    }

    // Cap the string.
    out[rlength] = '\0';

    return rlength;
}
