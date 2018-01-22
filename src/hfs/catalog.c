//
//  catalog.c
//  hfsinspect
//
//  Created by Adam Knight on 6/4/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//


#include "hfs/Apple/hfs_types.h"   // Apple's FastUnicodeCompare and conversion table.
#include "hfs/catalog.h"
#include "hfs/hfs_endian.h"
#include "hfs/hfs_io.h"
#include "hfs/output_hfs.h"
#include "hfs/unicode.h"
#include "volumes/utilities.h"     // commonly-used utility functions
#include "logging/logging.h"       // console printing routines
#include <sys/mount.h>             // getmntinfo


const uint32_t kAliasCreator            = 'MACS';
const uint32_t kFileAliasType           = 'alis';
const uint32_t kFolderAliasType         = 'fdrp';

char*          HFSPlusMetadataFolder    = HFSPLUSMETADATAFOLDER;
char*          HFSPlusDirMetadataFolder = HFSPLUS_DIR_METADATA_FOLDER;

// In Apple/FastUnicodeCompare.c
int32_t FastUnicodeCompare ( register const uint16_t* str1, register uint32_t length1,
                             register const uint16_t* str2, register uint32_t length2);

int hfsplus_get_catalog_btree(BTreePtr* tree, const HFSPlus* hfs)
{
    trace("tree (%p), hfs (%p)", (void *)tree, (void *)hfs);
    debug("Getting catalog B-Tree");

    static BTreePtr cachedTree;

    if (cachedTree == NULL) {
        HFSPlusFork* fork = NULL;
        FILE*        fp   = NULL;

        debug("Creating catalog B-Tree");

        SALLOC(cachedTree, sizeof(struct _BTree));

        if ( hfsplus_get_special_fork(&fork, hfs, kHFSCatalogFileID) < 0 ) {
            critical("Could not create fork for Catalog B-Tree!");
        }

        fp = fopen_hfsfork(fork);
        if (fp == NULL) return -1;

        btree_init(cachedTree, fp);
        if (hfs->vh.signature == kHFSXSigWord) {
            if (cachedTree->headerRecord.keyCompareType == kHFSCaseFolding) {
                // Case Folding (normal; case-insensitive)
                cachedTree->keyCompare = (btree_key_compare_func)hfsplus_catalog_compare_keys_cf;

            } else if (cachedTree->headerRecord.keyCompareType == kHFSBinaryCompare) {
                // Binary Compare (case-sensitive)
                cachedTree->keyCompare = (btree_key_compare_func)hfsplus_catalog_compare_keys_bc;

            }
        } else {
            // Case Folding (normal; case-insensitive)
            cachedTree->keyCompare = (btree_key_compare_func)hfsplus_catalog_compare_keys_cf;
        }
        cachedTree->treeID  = kHFSCatalogFileID;
        cachedTree->getNode = hfsplus_catalog_get_node;
    }

    // Copy the cached tree out.
    // Note this copies a reference to the same extent list in the HFSPlusFork struct so NEVER free that fork.
    *tree = cachedTree;

    return 0;
}

int hfsplus_catalog_get_node(BTreeNodePtr* out_node, const BTreePtr bTree, bt_nodeid_t nodeNum)
{
    trace("out_node (%p), bTree (%p), nodeNum %u", (void *)out_node, (void *)bTree, nodeNum);
    assert(out_node);
    assert(bTree);
    assert(bTree->treeID == kHFSCatalogFileID);

    BTreeNodePtr node = NULL;

    if ( btree_get_node(&node, bTree, nodeNum) < 0) return -1;

    if (node == NULL) {
        // empty node
        return 0;
    }

    // Swap catalog-specific structs in the records
    if ((node->nodeDescriptor->kind == kBTIndexNode) || (node->nodeDescriptor->kind == kBTLeafNode)) {
        for (unsigned recNum = 0; recNum < node->recordCount; recNum++) {
            void*              record     = NULL;
            HFSPlusCatalogKey* catalogKey = NULL;
            BTRecOffset        keyLen     = 0;

            // Get the raw record
            record     = BTGetRecord(node, recNum);

            // Swap the key first since we need a correct key length
            catalogKey = (HFSPlusCatalogKey*)record;
            swap_HFSPlusCatalogKey(catalogKey);

            // Verify key
            if (catalogKey->nodeName.length > 255) {
                warning("Record %d in node %d has an invalid name length: %d", recNum, nodeNum, catalogKey->nodeName.length);
                catalogKey->nodeName.length = 255; // Not the right answer, but better than reading 8K of data later on.
            }

            // Index nodes are already swapped, so only swap leaf nodes
            if (node->nodeDescriptor->kind == kBTLeafNode) {
                void* catalogRecord = NULL;
                keyLen        = BTGetRecordKeyLength(node, recNum);
                catalogRecord = ((char*)record + keyLen);
                swap_HFSPlusCatalogRecord((HFSPlusCatalogRecord*)catalogRecord);
            }
        }
    }

    *out_node = node;

    return 0;
}

/*
// Return is the record type (eg. kHFSPlusFolderRecord) and references to the key and value structs.
 int hfs_get_catalog_leaf_record(HFSPlusCatalogKey* const record_key, HFSPlusCatalogRecord* const record_value, const BTreeNodePtr node, BTRecNum recordID)
 {
     if (recordID >= node->recordCount) {
         error("Requested record %d, which is beyond the range of node %d (%d)", recordID, node->recordCount, node->nodeNumber);
         return -1;
     }

     debug("Getting catalog leaf record %d of node %d", recordID, node->nodeNumber);

     if (node->nodeDescriptor->kind != kBTLeafNode) return 0;

     BTNodeRecord record = {0};
     if (BTGetBTNodeRecord(&record, node, recordID) < 0)
         return -1;

     uint16_t max_key_length = sizeof(HFSPlusCatalogKey);
     uint16_t max_value_length = sizeof(HFSPlusCatalogRecord) + sizeof(uint16_t);

     uint16_t key_length = record.keyLen;
     uint16_t value_length = record.valueLen;

     if (key_length > max_key_length) {
         warning("Key length of record %d in node %d is invalid (%d; maximum is %d)", recordID, node->nodeNumber, key_length, max_key_length);
     }
     if (value_length > max_value_length) {
         warning("Value length of record %d in node %d is invalid (%d; maximum is %d)", recordID, node->nodeNumber, value_length, max_value_length);
     }

     if (record_key != NULL)     *record_key = *(HFSPlusCatalogKey*)record.key;
     if (record_value != NULL)   *record_value = *(HFSPlusCatalogRecord*)record.value;

     return ((HFSPlusCatalogRecord*)record.value)->record_type;
 }
*/

int8_t hfsplus_catalog_find_record(BTreeNodePtr* node, BTRecNum* recordID, FSSpec spec)
{
    hfs_str        record_name  = {0};
    const HFSPlus* hfs          = spec.hfs;
    bt_nodeid_t    parentFolder = spec.parentID;
    HFSUniStr255   name         = spec.name;

    trace("node (%p), recordID (%p), spec (%p, %u, (%u))", (void *)node, (void *)recordID, (void *)spec.hfs, spec.parentID, spec.name.length);

    hfsuc_to_str(&record_name, &name);
    debug("Searching catalog for %d:%s", parentFolder, record_name);

    HFSPlusCatalogKey catalogKey = {0};
    catalogKey.parentID  = parentFolder;
    catalogKey.nodeName  = name;
    catalogKey.keyLength = sizeof(catalogKey.parentID) + catalogKey.nodeName.length;
    catalogKey.keyLength = MAX(kHFSPlusCatalogKeyMinimumLength, catalogKey.keyLength);

    BTreePtr     catalogTree = NULL;
    hfsplus_get_catalog_btree(&catalogTree, hfs);
    BTreeNodePtr searchNode  = NULL;
    BTRecNum     searchIndex = 0;

    bool         result      = btree_search(&searchNode, &searchIndex, catalogTree, &catalogKey);

    if (result == true) {
        *node     = searchNode;
        *recordID = searchIndex;
        return 1;
    }

    return 0;
}

int hfsplus_catalog_compare_keys_cf(const HFSPlusCatalogKey* key1, const HFSPlusCatalogKey* key2)
{
    int result = 0;

    trace("key1 (%p) (%u, %u), key2 (%p) (%u, %u)",
          (void *)key1, key1->keyLength, key1->parentID,
          (void *)key2, key2->keyLength, key2->parentID);

    if ( (result = cmp(key1->parentID, key2->parentID)) != 0) return result;

    result = FastUnicodeCompare(key1->nodeName.unicode, key1->nodeName.length, key2->nodeName.unicode, key2->nodeName.length);

    return result;
}

int hfsplus_catalog_compare_keys_bc(const HFSPlusCatalogKey* key1, const HFSPlusCatalogKey* key2)
{
    int     result   = 0;
    hfs_str key1Name = {0};
    hfs_str key2Name = {0};

    hfsuc_to_str(&key1Name, &key1->nodeName);
    hfsuc_to_str(&key2Name, &key2->nodeName);

    trace("BC compare: key1 (%p) (%u, %u, '%s'), key2 (%p) (%u, %u, '%s')",
          (void *)key1, key1->parentID, key1->nodeName.length, key1Name,
          (void *)key2, key2->parentID, key2->nodeName.length, key2Name);

    if ( (result = cmp(key1->parentID, key2->parentID)) != 0) {
        trace("* Different parents")
        return result;
    }

    unsigned len = MIN(key1->nodeName.length, key2->nodeName.length);
    for (unsigned i = 0; i < len; i++) {
        if ((result = cmp(key1->nodeName.unicode[i], key2->nodeName.unicode[i])) != 0) {
            trace("* Character difference at position %u", i);
            break;
        }
    }

    if (result == 0) {
        // The shared prefix sorted the same, so the shorter one wins.
        result = cmp(key1->nodeName.length, key2->nodeName.length);
        if (result != 0) trace("* Shorter wins.");
    }

    return result;
}

#pragma mark - Names/Paths

int HFSPlusGetCNIDName(hfs_str* name, FSSpec spec)
{
    const HFSPlus*    hfs      = spec.hfs;
    bt_nodeid_t       cnid     = spec.parentID;
    BTreePtr          tree     = NULL;
    BTreeNodePtr      node     = NULL;
    BTRecNum          recordID = 0;
    HFSPlusCatalogKey key      = {0};

    trace("name %p, spec (%p, %u, (%u))", (void *)name, (void *)spec.hfs, spec.parentID, spec.name.length);

    key.parentID  = cnid;
    key.keyLength = kHFSPlusCatalogKeyMinimumLength;

    if (hfsplus_get_catalog_btree(&tree, hfs) < 0)
        return -1;

    int found = btree_search(&node, &recordID, tree, &key);
    if ((found != 1) || (node->dataLen == 0)) {
        warning("No thread record for %d found.", cnid);
        return -1;
    }

    debug("Found thread record %d:%d", node->nodeNumber, recordID);

    BTreeKeyPtr           recordKey    = NULL;
    void*                 recordValue  = NULL;
    btree_get_record(&recordKey, &recordValue, node, recordID);

    HFSPlusCatalogThread* threadRecord = (HFSPlusCatalogThread*)recordValue;

    return hfsuc_to_str(name, &threadRecord->nodeName);
}

int HFSPlusGetCNIDPath(hfs_str* path, FSSpec spec)
{
    const HFSPlus*      hfs     = spec.hfs;

    // get path prefix for files on this volume
    static char* pathPrefix = NULL;

    if (pathPrefix == NULL) {
        SALLOC(pathPrefix, PATH_MAX+1);

        struct statfs* stats = NULL;
        int count = getmntinfo(&stats, 0);

        // make a copy of devicePath in case we need to change it
        char devicePath[PATH_MAX+1] = "";
        (void)strlcpy(devicePath, hfs->vol->source, PATH_MAX);

    PREFIX:
        for (int i = 0; i < count; i++) {
            struct statfs volumeStats = stats[i];
            if (strcmp(devicePath, volumeStats.f_mntfromname) == 0) {
                strlcpy(pathPrefix, volumeStats.f_mntonname, PATH_MAX);
                // if it's not the root volume, add a trailing slash
                if (strlen(pathPrefix) > 1) {
                    (void)strlcat(pathPrefix, "/", PATH_MAX);
                }
            }
        }

        if (strlen(pathPrefix) == 0) {
            // if our volume is using the raw disk path, try the regular device
            if (strstr(devicePath, "/dev/rdisk") != NULL) {
                char *newDevicePath = NULL;
                SALLOC(newDevicePath, PATH_MAX + 1);
                (void)strlcat(newDevicePath, "/dev/disk", PATH_MAX);
                (void)strlcat(newDevicePath, &devicePath[10], PATH_MAX);
                info("No match for %s in getmntinfo(); trying %s instead.", devicePath, newDevicePath);
                (void)strlcpy(devicePath, newDevicePath, PATH_MAX);
                SFREE(newDevicePath);
                goto PREFIX;
                
            } else {
                // unknown?
                (void)strlcpy(pathPrefix, "/?/", PATH_MAX);
            }
        }
    }

    char fullPath[PATH_MAX+1]     = "";

    hfs_cnid_t              originalCNID    = spec.parentID;
    hfs_cnid_t              parentID        = spec.parentID;
    HFSPlusCatalogRecord    catalogRecord   = {0};
    int                     found           = 0;

    while (parentID != kHFSRootFolderID) {
        found = !(HFSPlusGetCatalogRecordByFSSpec(&catalogRecord, spec) < 0);
        if ( !found ) {
            debug("Record NOT found:");
            hfs_str str = "";
            hfsuc_to_str(&str, &spec.name);
            debug("(%u:%s) not found", spec.parentID, str);
            break;
        }

        switch (catalogRecord.record_type) {
            case kHFSPlusFolderRecord:
            case kHFSPlusFileRecord:
            {
                break;
            }

            case kHFSPlusFolderThreadRecord:
            case kHFSPlusFileThreadRecord:
            {
                hfs_str itemName = "";
                hfsuc_to_str(&itemName, &catalogRecord.catalogThread.nodeName);

                char tempPath[PATH_MAX+1] = "";

                (void)strlcpy(tempPath, (const char *)itemName, PATH_MAX);

                if (catalogRecord.record_type == kHFSPlusFolderThreadRecord) {
                    // If the original target CNID is a folder, don't add a trailing slash.
                    if (parentID != originalCNID) (void)strlcat(tempPath, "/", PATH_MAX);
                    (void)strlcat(tempPath, fullPath, PATH_MAX);
                }

                (void)strlcpy(fullPath, tempPath, PATH_MAX);

                spec.parentID   = catalogRecord.catalogThread.parentID;
                break;
            }

            default:
            {
                error("Unknown Catalog Record Type (%d).", catalogRecord.record_type);
                break;
            }
        }

        parentID = spec.parentID;
    }
    
    if (found) {
        (void)strlcpy((char *)path, pathPrefix, PATH_MAX);
        (void)strlcat((char *)path, fullPath, PATH_MAX);
    }
    return (found ? 0 : -1);
}

#pragma mark - Searching
/*
//
//   FIXME: Incomplete
//   Tries to follow all possible references from a catalog record, but only once. Returns 1 if the FSSpec refers to a new record, 0 if the source was not a reference, and -1 on error.
//
//int HFSPlusGetTargetOfCatalogRecord(FSSpec* targetSpec, const HFSPlusCatalogRecord* sourceRecord, const HFSPlus* hfs)
//{
//    trace("targetSpec (%p), sourceRecord (%p), hfs (%p)", targetSpec, sourceRecord, hfs);
//
//    switch(sourceRecord->record_type) {
//        case kHFSPlusFileRecord:
//        {
//            if (HFSPlusCatalogRecordIsHardLink(sourceRecord)) {
//                bt_nodeid_t targetCNID = sourceRecord->catalogFile.bsdInfo.special.iNodeNum;
//                FSSpec      spec       = {0};
//                if ( HFSPlusGetCatalogInfoByCNID(&spec, NULL, hfs, targetCNID) < 0)
//                    return -1;
//                *targetSpec = spec;
//                return 1;
//            }
//        }
//
//        case kHFSPlusFolderRecord:
//        {
//
//        }
//
//        case kHFSPlusFolderThreadRecord:
//        case kHFSPlusFileThreadRecord:
//        {
//
//        }
//
//        default:
//            return -1;
//    }
//    return 0;
//}
 */

int HFSPlusGetCatalogInfoByPath(FSSpecPtr out_spec, HFSPlusCatalogRecord* out_catalogRecord, const char* path, const HFSPlus* hfs)
{
    trace("out_spec (%p), out_catalogRecord (%p), path '%s', hfs (%p)", (void *)out_spec, (void *)out_catalogRecord, path, (void *)hfs);

    debug("Finding record for path: %s", path);

    if (strlen(path) == 0) {
        errno = EINVAL;
        return -1;
    }

    int                  found                  = 0;
    hfs_cnid_t           parentID               = kHFSRootFolderID;
    hfs_cnid_t           last_parentID          = 0;
    char*                file_path              = NULL;
    char*                dup_path               = NULL;
    char*                segment                = NULL;
    char                 last_segment[PATH_MAX] = "";
    HFSPlusCatalogRecord catalogRecord          = {0};

    // Copy the path for tokenization.
    file_path = dup_path = strdup(path);

    // Iterate over path segments.
    while ( (segment = strsep(&file_path, "/")) != NULL ) {
        debug("Segment: %d:%s", parentID, segment);

        // Persist the search info for later.
        (void)strlcpy(last_segment, segment, PATH_MAX);
        last_parentID = parentID;

        // Perform the search
        HFSUniStr255 name = {0};
        str_to_hfsuc(&name, (uint8_t*)segment);
        FSSpec       spec = { .hfs = hfs, .parentID = last_parentID, .name = name };
        found = !(HFSPlusGetCatalogRecordByFSSpec(&catalogRecord, spec) < 0);
        if ( !found ) {
            debug("Record NOT found:");
            debug("%s: segment '%u:%s' failed", path, parentID, segment);
            break;
        }

        debug("Record found:");
        debug("%s: parent: %d ", segment, parentID);

        // If this was a null segment (eg. ///bar) then move to the next without
        // changing the parent.
        if (strlen(segment) == 0) continue;

        // Get the parent information from the folder or thread record.
        switch (catalogRecord.record_type) {
            case kHFSPlusFolderRecord:
            {
                parentID = catalogRecord.catalogFolder.folderID;
                break;
            }

            case kHFSPlusFolderThreadRecord:
            case kHFSPlusFileThreadRecord:
            {
                parentID = catalogRecord.catalogThread.parentID;
                break;
            }

            case kHFSPlusFileRecord:
            {
                break;
            }

            default:
            {
                error("Didn't change the parent (%d).", catalogRecord.record_type);
                break;
            }
        }

        if (catalogRecord.record_type == kHFSPlusFileRecord) break;
    }

    if (found) {
        if (out_spec != NULL) {
            HFSUniStr255 name = {0};
            str_to_hfsuc(&name, (uint8_t*)last_segment);

            out_spec->hfs      = hfs;
            out_spec->parentID = last_parentID;
            out_spec->name     = name;
        }
        if (out_catalogRecord != NULL) *out_catalogRecord = catalogRecord;
    }

    SFREE(dup_path);

    debug("found: %u", found);
    return (found ? 0 : -1);
}

HFSPlusCatalogKey HFSPlusCatalogKeyFromFSSpec(FSSpec spec)
{
    trace("spec (%p, %u, (%u)", (void *)spec.hfs, spec.parentID, spec.name.length);

    HFSPlusCatalogKey catalogKey = {0};
    catalogKey.parentID  = spec.parentID;
    catalogKey.nodeName  = spec.name;
    catalogKey.keyLength = sizeof(catalogKey.parentID) + catalogKey.nodeName.length;
    catalogKey.keyLength = MAX(kHFSPlusCatalogKeyMinimumLength, catalogKey.keyLength);
    return catalogKey;
}

/*
FSSpec HFSPlusFSSpecFromCatalogKey(HFSPlusCatalogKey key)
{
    trace("key (%u, %u)", key.keyLength, key.parentID);

    FSSpec spec = { .parentID = key.parentID, .name = key.nodeName };
    return spec;
}
*/

int HFSPlusGetCatalogRecordByFSSpec(HFSPlusCatalogRecord* catalogRecord, FSSpec spec)
{
    const HFSPlus*    hfs         = spec.hfs;
    HFSPlusCatalogKey catalogKey  = HFSPlusCatalogKeyFromFSSpec(spec);
    BTreePtr          catalogTree = NULL;
    BTreeNodePtr      searchNode  = NULL;
    BTRecNum          searchIndex = 0;
    bool              result      = false;
    BTNodeRecord      record      = {0};

    trace("catalogRecord (%p), spec (%p, %u, (%u)", (void *)catalogRecord, (void *)spec.hfs, spec.parentID, spec.name.length);

    if ( hfsplus_get_catalog_btree(&catalogTree, hfs) < 0 ) return -1;

    result = btree_search(&searchNode, &searchIndex, catalogTree, &catalogKey);
    if (result != true) return -1;

    if ( BTGetBTNodeRecord(&record, searchNode, searchIndex) < 0) {
        debug("Get record failed.");
        return -1;
    }

    *catalogRecord = *(HFSPlusCatalogRecord*)record.value; //copy

    btree_free_node(searchNode);

    debug("returning");
    return 0;
}

int HFSPlusGetCatalogInfoByCNID(FSSpec* out_spec, HFSPlusCatalogRecord* out_catalogRecord, const HFSPlus* hfs, bt_nodeid_t cnid)
{
    FSSpec               spec          = { .hfs = hfs, .parentID = cnid };
    HFSPlusCatalogRecord catalogRecord = {0};

    trace("out_spec (%p), out_catalogRecord (%p), hfs (%p), cnid %u", (void *)out_spec, (void *)out_catalogRecord, (void *)hfs, cnid);

    if ( HFSPlusGetCatalogRecordByFSSpec(&catalogRecord, spec) < 0 )
        return -1;

    if ((catalogRecord.record_type == kHFSPlusFileThreadRecord) || (catalogRecord.record_type == kHFSPlusFolderThreadRecord)) {
        spec.parentID = catalogRecord.catalogThread.parentID;
        spec.name     = catalogRecord.catalogThread.nodeName;

        if ( HFSPlusGetCatalogRecordByFSSpec(&catalogRecord, spec) < 0 )
            return -1;
    }

    if (out_spec)           *out_spec = spec;
    if (out_catalogRecord)  *out_catalogRecord = catalogRecord;

    return 0;
}

#pragma mark - Property Tests

bool HFSPlusCatalogFileIsHardLink(const HFSPlusCatalogRecord* record)
{
    trace("record (%p)", (void *)record);

    return (
        (record->record_type == kHFSPlusFileRecord) &&
        (record->catalogFile.userInfo.fdCreator == kHFSPlusCreator && record->catalogFile.userInfo.fdType == kHardLinkFileType)
        );
}

bool HFSPlusCatalogFolderIsHardLink(const HFSPlusCatalogRecord* record)
{
    trace("record (%p)", (void *)record);

    return (
        (record->record_type == kHFSPlusFileRecord) &&
        ( (record->catalogFile.flags & kHFSHasLinkChainMask) && HFSPlusCatalogRecordIsFolderAlias(record) )
        );
}

bool HFSPlusCatalogRecordIsHardLink(const HFSPlusCatalogRecord* record)
{
    trace("record (%p)", (void *)record);

    return ( HFSPlusCatalogFileIsHardLink(record) || HFSPlusCatalogFolderIsHardLink(record) );
}

bool HFSPlusCatalogRecordIsSymLink(const HFSPlusCatalogRecord* record)
{
    trace("record (%p)", (void *)record);

    return (
        (record->record_type == kHFSPlusFileRecord)
        && (record->catalogFile.userInfo.fdCreator == kSymLinkCreator)
        && (record->catalogFile.userInfo.fdType == kSymLinkFileType)
        );
}

bool HFSPlusCatalogRecordIsFileAlias(const HFSPlusCatalogRecord* record)
{
    trace("record (%p)", (void *)record);

    return (
        (record->record_type == kHFSPlusFileRecord)
        && (record->catalogFile.userInfo.fdFlags & kIsAlias)
        && (record->catalogFile.userInfo.fdCreator == kAliasCreator)
        && (record->catalogFile.userInfo.fdType == kFileAliasType)
        );
}

bool HFSPlusCatalogRecordIsFolderAlias(const HFSPlusCatalogRecord* record)
{
    trace("record (%p)", (void *)record);

    return (
        (record->record_type == kHFSPlusFileRecord)
        && (record->catalogFile.userInfo.fdFlags & kIsAlias)
        && (record->catalogFile.userInfo.fdCreator == kAliasCreator)
        && (record->catalogFile.userInfo.fdType == kFolderAliasType)
        );
}

bool HFSPlusCatalogRecordIsAlias(const HFSPlusCatalogRecord* record)
{
    trace("record (%p)", (void *)record);

    return ( HFSPlusCatalogRecordIsFileAlias(record) || HFSPlusCatalogRecordIsFolderAlias(record) );
}

