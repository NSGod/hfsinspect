//
//  attributes.c
//  hfsinspect
//
//  Created by Adam Knight on 6/16/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include <wchar.h>

#include "hfsplus/hfsplus.h"

#include "hfs/hfs_io.h"
#include "hfs/unicode.h"
#include "hfs/btree/btree.h"
#include "volumes/utilities.h" // commonly-used utility functions
#include "logging/logging.h"   // console printing routines
#include "hfs/output_hfs.h"


int hfsplus_get_attributes_btree(BTreePtr* tree, const HFSPlus* hfs)
{
    static BTreePtr cachedTree = NULL;

    debug("Getting Attributes B-Tree");

    if (cachedTree == NULL) {
        HFSPlusFork* fork = NULL;
        FILE*        fp   = NULL;

        debug("Creating Attributes B-Tree");

        SALLOC(cachedTree, sizeof(struct _BTree));

        if ( hfsplus_get_special_fork(&fork, hfs, kHFSAttributesFileID) < 0 ) {
            critical("Could not create fork for Attributes B-Tree!");
            return -1;
        }

        fp                     = fopen_hfsfork(fork);
        btree_init(cachedTree, fp);
        cachedTree->treeID     = kHFSAttributesFileID;
        cachedTree->keyCompare = (btree_key_compare_func)hfsplus_attributes_compare_keys;
        cachedTree->getNode    = hfsplus_attributes_get_node;
    }

    *tree = cachedTree;

    return 0;
}

int hfsplus_attributes_compare_keys (const HFSPlusAttrKey* key1, const HFSPlusAttrKey* key2)
{
    int          result     = 0;

    HFSPlusAttrStr127 key1UniStr = HFSPlusAttrKeyGetStr(key1);
    HFSPlusAttrStr127 key2UniStr = HFSPlusAttrKeyGetStr(key2);

    hfs_attr_str key1Name = {0};
    hfs_attr_str key2Name = {0};

    attruc_to_attrstr(&key1Name, &key1UniStr);
    attruc_to_attrstr(&key2Name, &key2UniStr);

    trace("compare: key1 (%p) (%u, %u, '%s' (%u)), key2 (%p) (%u, %u, '%s' (%u))",
          (void *)key1, key1->fileID, key1->startBlock, key1Name, key1->attrNameLen,
          (void *)key2, key2->fileID, key2->startBlock, key2Name, key2->attrNameLen);

    if ( (result = cmp(key1->fileID, key2->fileID)) != 0) {
        trace("* %d: File ID difference.", result);
        return result;
    }

    unsigned len = MIN(key1->attrNameLen, key2->attrNameLen);
    for (unsigned i = 0; i < len; i++) {
        if ((result = cmp(key1->attrName[i], key2->attrName[i])) != 0) {
            trace("* %d: Character difference at position %u", result, i);
            return result;
        }
    }

    // The shared prefix sorted the same, so the shorter one wins.
    if ((result = cmp(key1->attrNameLen, key2->attrNameLen)) != 0) {
        trace("* %d: Shorter attrName wins.", result);
        return result;
    }

    // If names are equal, compare startBlock
    if ((result = cmp(key1->startBlock, key2->startBlock)) != 0) {
        trace("* %d: startBlock difference.", result);
        return result;
    }
    return 0;
}

int hfsplus_attributes_get_node(BTreeNodePtr* out_node, const BTreePtr bTree, bt_nodeid_t nodeNum)
{
    BTreeNodePtr node = NULL;

    assert(out_node);
    assert(bTree);
    assert(bTree->treeID == kHFSAttributesFileID);

    if ( btree_get_node(&node, bTree, nodeNum) < 0)
        return -1;

    // Handle empty nodes (error?)
    if (node == NULL) {
        return 0;
    }

    // Swap tree-specific structs in the records
    if ((node->nodeDescriptor->kind == kBTIndexNode) || (node->nodeDescriptor->kind == kBTLeafNode)) {
        for (unsigned recNum = 0; recNum < node->recordCount; recNum++) {
            void*           record = BTGetRecord(node, recNum);
            HFSPlusAttrKey* key    = (HFSPlusAttrKey*)record;

            swap_HFSPlusAttrKey(key);

            if (node->nodeDescriptor->kind == kBTLeafNode) {
                BTRecOffset keyLen     = BTGetRecordKeyLength(node, recNum);
                void*       attrRecord = ((char*)record + keyLen);

                swap_HFSPlusAttrRecord((HFSPlusAttrRecord*)attrRecord);
            }
        }
    }

    *out_node = node;

    return 0;
}

int hfsplus_attributes_get_xattrlist_for_cnid(XAttrList* list, hfs_cnid_t cnid, const HFSPlus* hfs)
{
    BTreePtr       bTree       = NULL;
    BTreeNodePtr   node        = NULL;
    BTRecNum       recordID    = 0;
    HFSPlusAttrKey key         = {0};

    if (hfsplus_get_attributes_btree(&bTree, hfs) < 0) {
        printf("failed to get the Attributes B-Tree!\n");
        return -1;
    }

    // we want a key that will sort before anything else, so use a NULL attrName...
    int result = 0;
    if ( (result = hfsplus_attributes_make_key(&key, cnid, NULL)) < 0) {
        printf("hfsplus_attributes_make_key() returned %d\n", result);
        return result;
    }

    btree_search(&node, &recordID, bTree, &key);

    // Iterate through the records finding those with our CNID
    while (1) {
        BTNodeRecord btRecord = {0};
        debug("Loading record %u", recordID);
        BTGetBTNodeRecord(&btRecord, node, recordID);

        HFSPlusAttrKey*     attrKey     = (HFSPlusAttrKey*)btRecord.key;
        debug("%u, %u", cnid, attrKey->fileID);

        if (attrKey->fileID > cnid) {
            break;

        } else if (attrKey->fileID == cnid) {
            xattrlist_add_record(list, attrKey, (HFSPlusAttrRecord*)btRecord.value);
        }

        if (++recordID >= node->nodeDescriptor->numRecords) {
            bt_nodeid_t nextNodeID = node->nodeDescriptor->fLink;
            debug("Loading node %u", nextNodeID);
            BTFreeNode(node);
            recordID  = 0;
            BTGetNode(&node, bTree, nextNodeID);
        }
    }
    return 0;
}

int hfsplus_attributes_make_key(HFSPlusAttrKey* key, hfs_cnid_t cnid, const char *attrName)
{
    int result = 0;

    key->fileID = cnid;
    key->pad = 0;
    key->startBlock = 0;

    if (attrName) {
        HFSPlusAttrStr127 name = {0};
        if ( (result = attrstr_to_attruc(&name, (const uint8_t*)attrName)) < 0) {
            return result;
        }

        key->attrNameLen = name.attrNameLen;
        memcpy(key->attrName, &name.attrName, name.attrNameLen * sizeof(uint16_t));
        key->keyLength = kHFSPlusAttrKeyMinimumLength + key->attrNameLen * sizeof(uint16_t);

    } else {
        key->attrNameLen = 0;
        key->keyLength = kHFSPlusAttrKeyMinimumLength;
    }

    return 0;
}

HFSPlusAttrStr127 HFSPlusAttrKeyGetStr(const HFSPlusAttrKey* key)
{
    HFSPlusAttrStr127 str = { .attrNameLen = key->attrNameLen, {0}};
    memcpy(&str.attrName, key->attrName, key->attrNameLen * sizeof(uint16_t));
    return str;
}
