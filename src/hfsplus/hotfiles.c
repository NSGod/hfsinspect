//
//  hotfiles.c
//  hfsinspect
//
//  Created by Adam Knight on 12/3/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include "hfsplus/hfsplus.h"
#include "hfs/catalog.h"
#include "hfs/hfs_io.h"
#include "hfs/unicode.h"
#include "logging/logging.h"    // console printing routines

static BTreePtr cachedTree = NULL;

int hfs_get_hotfiles_btree(BTreePtr* tree, const HFSPlus* hfs)
{
    debug("Getting hotfiles B-Tree");

    if (cachedTree == NULL) {
        BTreeNodePtr node          = NULL;
        BTreeKeyPtr  recordKey     = NULL;
        void*        recordValue   = NULL;
        HFSPlusFork* fork          = NULL;
        FILE*        fp            = NULL;
        BTRecNum     recordID      = 0;
        bt_nodeid_t  parentfolder  = kHFSRootFolderID;
        FSSpec       spec          = { .hfs = hfs, .parentID = parentfolder, .name = {0} };
        int          found         = 0;

        str_to_hfsuc(&spec.name, (const uint8_t *)HFC_FILENAME);

        found = hfsplus_catalog_find_record(&node, &recordID, spec);
        if (found != 1)
            return -1;

        debug("Creating hotfiles B-Tree");

        btree_get_record(&recordKey, &recordValue, node, recordID);

        HFSPlusCatalogRecord* record = (HFSPlusCatalogRecord*)recordValue;
        if ( hfsfork_make(&fork, hfs, record->catalogFile.dataFork, HFSDataForkType, record->catalogFile.fileID) < 0 )
            return -1;

        SALLOC(cachedTree, sizeof(struct _BTree));

        fp = fopen_hfsfork(fork);
        if (fp == NULL) return -1;

        if (btree_init(cachedTree, fp) < 0) {
            error("Error initializing hotfiles btree.");
            return -1;
        }
        cachedTree->treeID  = record->catalogFile.fileID;
        cachedTree->getNode = hfs_hotfiles_get_node;
        cachedTree->keyCompare = (btree_key_compare_func)hfs_hotfiles_compare_keys;
    }

    // Copy the cached tree out.
    // Note this copies a reference to the same extent list in the HFSPlusFork struct so NEVER free that fork.
    *tree = cachedTree;

    return 0;
}

int hfs_hotfiles_get_node(BTreeNodePtr* out_node, const BTreePtr bTree, bt_nodeid_t nodeNum)
{
    BTreeNodePtr node = NULL;

    assert(out_node);
    assert(bTree);
    assert(bTree->treeID > kHFSFirstUserCatalogNodeID);

    if ( btree_get_node(&node, bTree, nodeNum) < 0)
        return -1;

    if (node == NULL) {
        // empty node
        return 0;
    }

    // Swap hotfile-specific structs in the records
    if (node->nodeDescriptor->kind == kBTIndexNode || node->nodeDescriptor->kind == kBTLeafNode) {
        for (unsigned recNum = 0; recNum < node->recordCount; recNum++) {
            BTNodeRecord record = {0};
            BTGetBTNodeRecord(&record, node, recNum);
            HotFileKey* key = (HotFileKey*)record.key;
            swap_HotFileKey(key);

            if (key->temperature == HFC_LOOKUPTAG) {
                // this is a thread record, so swap the record value (the temperature)
                HotFileThreadRecord* value = (HotFileThreadRecord*)record.record;
                Swap32(value->temperature);
            } else {
                // file record, so swap the data
                HotFileRecord* value = (HotFileRecord*)record.record;
                Swap32(value->data);
            }
        }
    }

    *out_node = node;

    return 0;
}

int hfs_hotfiles_compare_keys(const HotFileKey* key1, const HotFileKey* key2)
{
    trace("key1 (%p) forkType == %u, temp == %u, fileID == %u; key2 (%p) forkType == %u, temp == %u, fileID == %u",
          (void *)key1, key1->forkType, key1->temperature, key1->fileID,
          (void *)key2, key2->forkType, key2->temperature, key2->fileID);

    int result = 0;
    if ( (result = cmp(key1->temperature, key2->temperature)) != 0) return result;
    if ( (result = cmp(key1->fileID, key2->fileID)) != 0) return result;
    if ( (result = cmp(key1->forkType, key2->forkType)) != 0) return result;
    return 0;
}

bt_nodeid_t hfs_hotfiles_get_tree_id() {
    if (cachedTree == NULL) return 0;
    return cachedTree->treeID;
}

int hfs_hotfiles_get_info(HotFilesInfo* info, const BTreePtr tree)
{
    BTreeNodePtr node = NULL;
    if ( BTGetNode(&node, cachedTree, 0) < 0) {
        error("There was an error fetching node 0");
        return -1;
    }
    // get the userData record (recordNum == 1)
    void* userData = BTGetRecord(node, 1);
    if (userData == NULL) {
        return -1;
    }
    HotFilesInfo* hotFilesInfo = (HotFilesInfo*)userData;
    swap_HotFilesInfo(hotFilesInfo);

    *info = *hotFilesInfo; // copy

    btree_free_node(node);
    
    return 0;
}

