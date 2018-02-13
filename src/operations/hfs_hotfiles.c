//
//  hfs_hotfiles.c
//  hfsinspect
//
//  Created by Mark Douma on 2/1/2018.
//  Copyright (c) 2018 Mark Douma. All rights reserved.
//

#include "operations.h"
#include "volumes/utilities.h"     // commonly-used utility functions
#include "hotfiles.h"

int compareHotfiles(const void* a, const void* b)
{
    const HotFileKey* A     = (HotFileKey *)a;
    const HotFileKey* B     = (HotFileKey *)b;

    return hfs_hotfiles_compare_keys(A, B);
}

void showHotFiles(HIOptions* options)
{
    HFSPlus*    hfs = options->hfs;

    BTreePtr    hotfiles = NULL;
    hfs_get_hotfiles_btree(&hotfiles, hfs);
    hfs_cnid_t  cnid = hotfiles->headerRecord.firstLeafNode;

    uint32_t topCount = options->topCount;

    HotFileKey* topHotfiles = NULL;

    SALLOC(topHotfiles, topCount * sizeof(HotFileKey));

    out_ctx* ctx = hfs->vol->ctx;

    uint32_t totalCount = 0;

    /*
       Walk the leaf nodes and gather the hotfile records.
     */

    while (1) {
        BTreeNodePtr node = NULL;
        if ( BTGetNode(&node, hotfiles, cnid) < 0) {
            perror("get node");
            die(1, "There was an error fetching node %u", cnid);
        }

        for (unsigned recNum = 0; recNum < node->nodeDescriptor->numRecords; recNum++) {
            // we'll just look at the Hot File Records not thread records
            BTreeKeyPtr     recordKey = NULL;
            void*           recordValue = NULL;
            btree_get_record(&recordKey, &recordValue, node, recNum);

            HotFileKey* hotKey = (HotFileKey *)recordKey;

            if (hotKey->temperature != HFC_LOOKUPTAG) {

                totalCount++;

                if (topHotfiles[0].temperature < hotKey->temperature) {
                    topHotfiles[0].temperature = hotKey->temperature;
                    topHotfiles[0].fileID = hotKey->fileID;
                    topHotfiles[0].forkType = hotKey->forkType;
                    qsort(topHotfiles, topCount, sizeof(HotFileKey), compareHotfiles);
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

    HotFilesInfo info = {0};
    hfs_hotfiles_get_info(&info, hotfiles);
    PrintHotFilesInfo(ctx, &info);

    BeginSection(ctx, "Hottest Files");
    Print(ctx, "%4s %10s %6s %11s  %s", "#", "CNID", "Fork", "Temperature", "Path");
    
    for (int32_t i = topCount-1; i >= 0; i--) {
        if (topHotfiles[i].fileID == 0) continue;

        hfs_str fullPath = "";
        int result = HFSPlusGetCNIDPath(&fullPath, (FSSpec){ hfs, topHotfiles[i].fileID } );
        if (result < 0) strlcpy((char *)fullPath, "<unknown>", PATH_MAX);

        Print(ctx, "%4u %10u %6s %11u  %s",
              topCount - i,
              topHotfiles[i].fileID,
              topHotfiles[i].forkType == HFSDataForkType ? "data" : "rsrc",
              topHotfiles[i].temperature,
              fullPath
              );
    }
    Print(ctx, "%s", "");
    Print(ctx, "A total of %u hot files are being tracked.", totalCount);

    EndSection(ctx); // Hottest Files

    SFREE(topHotfiles);
}


