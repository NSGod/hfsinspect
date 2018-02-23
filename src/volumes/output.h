//
//  output.h
//  volumes
//
//  Created by Adam Knight on 11/10/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#ifndef volumes_output_h
#define volumes_output_h

#include <time.h>

#define MEMBER_LABEL(s, m) #m, s->m

struct out_ctx {
    unsigned indent_level;
    unsigned indent_step;
    char     indent_string[32];
    char*    prefix;
    bool     decimal_sizes;
    bool     wideAttributes;
    uint8_t  _reserved[6];
};
typedef struct out_ctx out_ctx;

out_ctx OCMake          (bool decimal_sizes, unsigned indent_step, char* prefix);
void    OCSetIndentLevel(out_ctx* ctx, unsigned new_level);

void _PrintDataLength   (out_ctx* ctx, const char* label, uint64_t size);
void VisualizeData      (const void* data, size_t length);

#define PrintDataLength(ctx, record, value)         _PrintDataLength(ctx, #value, (uint64_t)record->value)
#define PrintUIChar(ctx, record, value)             _PrintUIChar(ctx, #value, (uint64_t)record->value, sizeof(record->value))
#define PrintUIBinary(ctx, record, value)           _PrintUIBinary(ctx, #value, (uint64_t)record->value, sizeof(record->value))
#define PrintHexData(ctx, record, value)            _PrintHexData(ctx, #value, &(record->value), sizeof(record->value))

#define PrintInt(ctx, record, value)                PrintAttribute(ctx, #value, "%lld", (int64_t)record->value)
#define PrintUI(ctx, record, value)                 PrintAttribute(ctx, #value, "%llu", (uint64_t)record->value)
#define PrintUIOct(ctx, record, value)              PrintAttribute(ctx, #value, "0%06o (%u)", record->value, record->value)
#define PrintUIHex(ctx, record, value)              PrintAttribute(ctx, #value, "%#llx (%llu)", (uint64_t)record->value, (uint64_t)record->value)

#define PrintUIntFlag(ctx, label, name, value)      PrintAttribute(ctx, label, "%s (%llu)", name, (uint64_t)value)
#define PrintIntFlag(ctx, label, name, value)       PrintAttribute(ctx, label, "%s (%lld)", name, (int64_t)value)
#define PrintOctFlag(ctx, label, name, value)       PrintAttribute(ctx, label, "0%06o (%s)", value, name)
#define PrintHexFlag(ctx, label, name, value)       PrintAttribute(ctx, label, "%s (%#x)", name, value)

#define PrintUIFlagIfSet(ctx, source, flag)         { if (((uint64_t)(source)) & (((uint64_t)1) << ((uint64_t)(flag)))) PrintUIntFlag(ctx, NULL, #flag, flag); }
#define PrintUIFlagIfMatch(ctx, source, mask)       { if ((source) & mask) PrintUIntFlag(ctx, NULL, #mask, mask); }
#define PrintUIOctFlagIfMatch(ctx, source, mask)    { if ((source) & mask) PrintOctFlag(ctx, NULL, #mask, mask); }
#define PrintHexFlagIfMatch(ctx, source, mask)      { if ((source) & mask) PrintHexFlag(ctx, NULL, #mask, mask); }

#define PrintConstIfEqual(ctx, source, c)           { if ((source) == c)   PrintUIntFlag(ctx, NULL, #c, c); }
#define PrintLabeledConstIfEqual(ctx, record, value, c)    { if ((record->value) == c)   PrintIntFlag(ctx, #value, #c, c); }
#define PrintLabeledConstHexIfEqual(ctx, record, value, c)    { if ((record->value) == c)   PrintHexFlag(ctx, #value, #c, c); }
#define PrintConstOctIfEqual(ctx, source, c)        { if ((source) == c)   PrintOctFlag(ctx, NULL, #c, c); }
#define PrintConstHexIfEqual(ctx, source, c)        { if ((source) == c)   PrintHexFlag(ctx, NULL, #c, c); }

int  BeginSection   (out_ctx* ctx, const char* format, ...) __printflike(2, 3);
void EndSection     (out_ctx* ctx);

int Print           (out_ctx* ctx, const char* format, ...) __printflike(2, 3);
int PrintAttribute  (out_ctx* ctx, const char* label, const char* format, ...) __printflike(3, 4);
int _PrintUIChar    (out_ctx* ctx, const char* label, uint64_t value, size_t nbytes);
void _PrintUIBinary (out_ctx* ctx, const char* label, uint64_t value, size_t nbytes);
void _PrintHexData  (out_ctx* ctx, const char* label, const void* map, size_t nbytes);

int format_size     (out_ctx* ctx, char* out, uint64_t value, size_t length);
int format_blocks   (out_ctx* ctx, char* out, uint64_t blocks, size_t block_size, size_t length);
int format_time     (out_ctx* ctx, char* out, time_t gmt_time, size_t length);

int format_uint_oct (char* out, uint64_t value, uint8_t padding, size_t length);
int format_uint_dec (char* out, uint64_t value, uint8_t padding, size_t length);
int format_uint_hex (char* out, uint64_t value, uint8_t padding, size_t length);
int format_uuid     (char* out, const unsigned char value[16]);
int format_uint_chars(char* out, uint64_t value, size_t nbytes, size_t length);
int format_uint_binary(char* out, uint64_t value, size_t nbytes, size_t length);
ssize_t format_hex_data(char* restrict out, const void* data, size_t nbytes, size_t length);

#endif
