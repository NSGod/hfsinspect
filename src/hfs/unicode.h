//
//  hfs/unicode.h
//  hfsinspect
//
//  Created by Adam Knight on 2015-02-01.
//  Copyright (c) 2015 Adam Knight. All rights reserved.
//

#ifndef hfs_unicode_h
#define hfs_unicode_h

#include <stdint.h>             // uint*
#include "hfs/types.h"          // hfs_str, hfs_attr_str
#include "hfsplus/attributes.h" // HFSPlusAttrStr127

#pragma mark UTF conversions

int hfsuc_to_str(hfs_str* str, const HFSUniStr255* hfs);
int str_to_hfsuc(HFSUniStr255* hfs, const hfs_str str);

int attruc_to_attrstr(hfs_attr_str* str, const HFSPlusAttrStr127* name);
int attrstr_to_attruc(HFSPlusAttrStr127* name, const hfs_attr_str str);

#endif
