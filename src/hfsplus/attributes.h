//
//  attributes.h
//  hfsinspect
//
//  Created by Adam Knight on 6/16/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#ifndef hfsinspect_hfs_attribute_ops_h
#define hfsinspect_hfs_attribute_ops_h

#include "hfs/types.h"
#include "hfsplus/hfsplus_xattrlist.h"

#define _NONNULL __attribute__((nonnull))

struct HFSPlusAttrStr127 {
    uint16_t    attrNameLen;                    /* number of unicode characters */
    uint16_t    attrName[kHFSMaxAttrNameLen];   /* attribute name (Unicode) */
} __attribute__((aligned(2), packed));
typedef struct HFSPlusAttrStr127 HFSPlusAttrStr127;

int hfsplus_get_attributes_btree     (BTreePtr* tree, const HFSPlus* hfs) _NONNULL;
int hfsplus_attributes_compare_keys (const HFSPlusAttrKey* key1, const HFSPlusAttrKey* key2) _NONNULL;
int hfsplus_attributes_get_node     (BTreeNodePtr* node, const BTreePtr bTree, bt_nodeid_t nodeNum) _NONNULL;
int hfsplus_attributes_get_xattrlist_for_cnid(XAttrList* list, hfs_cnid_t cnid, const HFSPlus* hfs) _NONNULL;

void swap_HFSPlusAttrKey        (HFSPlusAttrKey* record) _NONNULL;
void swap_HFSPlusAttrData       (HFSPlusAttrData* record) _NONNULL;
void swap_HFSPlusAttrForkData   (HFSPlusAttrForkData* record) _NONNULL;
void swap_HFSPlusAttrExtents    (HFSPlusAttrExtents* record) _NONNULL;
void swap_HFSPlusAttrRecord     (HFSPlusAttrRecord* record) _NONNULL;

int hfsplus_attributes_make_key(HFSPlusAttrKey* key, hfs_cnid_t cnid, const char *attrName) __attribute__((nonnull(1)));

HFSPlusAttrStr127 HFSPlusAttrKeyGetStr(const HFSPlusAttrKey* key);

#endif
