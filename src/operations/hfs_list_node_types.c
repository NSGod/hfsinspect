//
//  hfs_list_node_types.c
//  hfsinspect
//
//  Created by Mark Douma on 2/17/2018.
//  Copyright (c) 2018 Mark Douma. All rights reserved.
//

#include "operations.h"

void listNodeTypes(HIOptions* options)
{
    HFSPlus* hfs = options->hfs;
    out_ctx* ctx = hfs->vol->ctx;

    BTreePtr tree = options->tree;

    hfs_cnid_t cnid = tree->headerRecord.firstLeafNode;
    
    bt_nodeid_t hotfilesTreeID = hfs_hotfiles_get_tree_id();

    while (1) {
        BTreeNodePtr node = NULL;
        if ( BTGetNode(&node, tree, cnid) < 0) {
            perror("get node");
            die(EXIT_FAILURE, "There was an error fetching node %u", cnid);
        }

        for (unsigned recNum = 0; recNum < node->nodeDescriptor->numRecords; recNum++) {
            BTNodeRecord    record = {0};
            BTGetBTNodeRecord(&record, node, recNum);

            if (node->bTree->treeID == kHFSExtentsFileID) {
                if (check_list_type(options, BTreeListTypeExtents)) {
                    BeginSection(ctx, "Extent Record");
                    HFSPlusExtentKey* key = (HFSPlusExtentKey*)record.key;
                    PrintHFSPlusExtentKey(ctx, key);
                    HFSPlusExtentRecord* extentRecord = (HFSPlusExtentRecord*)record.value;
                    ExtentList* list = extentlist_make();
                    extentlist_add_record(list, *extentRecord);
                    PrintExtentList(ctx, list, 0);
                    extentlist_free(list);
                    EndSection(ctx);
                }
            } else if (node->bTree->treeID == kHFSCatalogFileID) {

                HFSPlusCatalogRecord* catRecord = (HFSPlusCatalogRecord*)record.value;

                switch (catRecord->record_type) {
                    case kHFSPlusFileRecord:
                    {
                        if (check_list_type(options, BTreeListTypeFile)) {
                            BeginSection(ctx, "Catalog File Record");
                            PrintHFSPlusCatalogFile(ctx, (const HFSPlusCatalogFile*)catRecord);
                            EndSection(ctx);
                        }
                        break;
                    }

                    case kHFSPlusFolderRecord:
                    {
                        if (check_list_type(options, BTreeListTypeFolder)) {
                            BeginSection(ctx, "Catalog Folder Record");
                            PrintHFSPlusCatalogFolder(ctx, (const HFSPlusCatalogFolder*)catRecord);
                            EndSection(ctx);
                        }
                        break;
                    }

                    case kHFSPlusFileThreadRecord:
                    {
                        if (check_list_type(options, BTreeListTypeFileThread)) {
                            BeginSection(ctx, "File Thread Record");
                            PrintHFSPlusCatalogThread(ctx, (const HFSPlusCatalogThread*)catRecord);
                            EndSection(ctx);
                        }
                        break;
                    }

                    case kHFSPlusFolderThreadRecord:
                    {
                        if (check_list_type(options, BTreeListTypeFolderThread)) {
                            BeginSection(ctx, "Folder Thread Record");
                            PrintHFSPlusCatalogThread(ctx, (const HFSPlusCatalogThread*)catRecord);
                            EndSection(ctx);
                        }
                        break;
                    }

                    default:
                        break;
                }
            } else if (node->bTree->treeID == kHFSAttributesFileID) {
                if (check_list_type(options, BTreeListTypeAny)) {
                    BeginSection(ctx, "Attribute Record");
                    HFSPlusAttrKey* key = (HFSPlusAttrKey*)record.key;
                    PrintHFSPlusAttrKey(ctx, key);
                    PrintPath(ctx, "path", key->fileID);
                    PrintHFSPlusAttrRecord(ctx, (HFSPlusAttrRecord*)record.value);
                    EndSection(ctx);
                }
            } else if (node->bTree->treeID == hotfilesTreeID) {

                HotFileKey* keyStruct = (HotFileKey*) record.key;

                if (keyStruct->temperature == HFC_LOOKUPTAG) {
                    // it's a thread record
                    if (check_list_type(options, BTreeListTypeHotfileThread)) {
                        BeginSection(ctx, "Hotfiles Thread Record");
                        PrintHotFileThreadRecord(ctx, (const HotFileThreadRecord*)record.record);
                        EndSection(ctx);
                    }
                } else {
                    if (check_list_type(options, BTreeListTypeHotfile)) {
                        BeginSection(ctx, "Hotfiles File Record");
                        PrintHotFileRecord(ctx, (const HotFileRecord*)record.record);
                        EndSection(ctx);
                    }
                }
            }
        }

        // Follow link
        cnid = node->nodeDescriptor->fLink;
        if (cnid == 0) {       // End Of Line
            btree_free_node(node);
            break;
        }
        btree_free_node(node);
    }
}
