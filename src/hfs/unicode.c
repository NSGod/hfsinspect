//
//  hfs/unicode.c
//  hfsinspect
//
//  Created by Adam Knight on 2015-02-01.
//  Copyright (c) 2015 Adam Knight. All rights reserved.
//

#include <string.h>

#include "hfs/Apple/utfconv.h"
#include "hfs/unicode.h"
#include "logging/logging.h"    // console printing routines


int hfsuc_to_str(hfs_str* str, const HFSUniStr255* hfs)
{
    int    result    = 0;
    size_t utf8_size = 0;

    result = utf8_encodestr(hfs->unicode, hfs->length*2, *str, &utf8_size, 1024, '/', 0);
    if (result < 0) {
        utf8_size = result;
    }
    return (int)utf8_size;
}

int str_to_hfsuc(HFSUniStr255* hfs, const hfs_str str)
{
    int    result = 0;
    size_t ucslen = 0;

    result      = utf8_decodestr(str, strlen((char*)str)+1, hfs->unicode, &ucslen, 256, '/', UTF_DECOMPOSED);
    hfs->length = (uint16_t)(ucslen/2);
    if (result < 0) {
        ucslen = result;
    }
    return (int)ucslen;
}

int attruc_to_attrstr(hfs_attr_str* str, const HFSPlusAttrStr127* name)
{
    int     result      = 0;
    ssize_t  utf8_size   = 0;

    result = utf8_encodestr(name->attrName, name->attrNameLen*2, *str, (size_t*)&utf8_size, sizeof(hfs_attr_str), 0, 0);
    if (result < 0) {
        utf8_size = -result;
    }
    return (int)utf8_size;
}

int attrstr_to_attruc(HFSPlusAttrStr127* name, const hfs_attr_str str)
{
    int     result  = 0;
    ssize_t  unicodeBytes  = 0;

    result = utf8_decodestr(str, strlen((char*)str), name->attrName, (size_t*)&unicodeBytes, sizeof(name->attrName), 0, 0);
    name->attrNameLen = (uint16_t)(unicodeBytes/2);
    if (result < 0) {
        unicodeBytes = -result;
    }
    return (int)unicodeBytes;
}
