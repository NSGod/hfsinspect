//
//  hfs_inspect_blocks.c
//  hfsinspect
//
//  Created by Mark Douma on 1/23/2018.
//  Copyright (c) 2018 Mark Douma. All rights reserved.
//

#include "operations.h"
#if defined(__linux__)
    #include <bsd/sys/queue.h>  // TAILQ
#else
    #include <sys/queue.h>
#endif
#include "hfs/range.h"

typedef struct _FileExtent {
    size_t          startBlock;
    size_t          blockCount;
    hfs_cnid_t      cnid;
    uint32_t        extentIndex;
    uint32_t        extentCount;
    hfs_forktype_t  forkType;
    uint8_t         reserved[3];
    TAILQ_ENTRY(_FileExtent) extents;
} FileExtent;

typedef TAILQ_HEAD(_FileExtentList, _FileExtent) FileExtentList;

void fileExtentList_sorted_insert(FileExtentList* list, size_t startBlock, size_t blockCount, hfs_cnid_t cnid, uint32_t extentIndex, uint32_t extentCount, hfs_forktype_t forkType) __attribute__((nonnull));
void fileExtentList_free(FileExtentList* list) __attribute__((nonnull));


FileExtentList* fileExtentList_create(void)
{
    trace("%s()", __FUNCTION__);
    FileExtentList* retval = NULL;
    SALLOC(retval, sizeof(FileExtentList));
    retval->tqh_first = NULL;
    retval->tqh_last = &retval->tqh_first; // Ensure this is pointing to the value on the heap, not the stack.
    TAILQ_INIT(retval);
    return retval;
}

void fileExtentList_sorted_insert(FileExtentList* list, size_t startBlock, size_t blockCount, hfs_cnid_t cnid, uint32_t extentIndex, uint32_t extentCount, hfs_forktype_t forkType)
{
    if (blockCount == 0) return;

    FileExtent* newExtent = NULL;

    SALLOC(newExtent, sizeof(FileExtent));
    newExtent->startBlock = startBlock;
    newExtent->blockCount = blockCount;
    newExtent->cnid = cnid;
    newExtent->extentIndex = extentIndex;
    newExtent->extentCount = extentCount;
    newExtent->forkType = forkType;

    if (TAILQ_EMPTY(list)) {
        TAILQ_INSERT_HEAD(list, newExtent, extents);
    } else {

        FileExtent *e = NULL, * next = NULL;

        TAILQ_FOREACH_SAFE(e, list, extents, next) {
            if (startBlock < e->startBlock) {
                TAILQ_INSERT_BEFORE(e, newExtent, extents);
                break;
                
            } else if (startBlock > e->startBlock) {
                if (next) {
                    if (startBlock < next->startBlock) {
                        TAILQ_INSERT_BEFORE(next, newExtent, extents);
                        break;
                    } else {
                        continue;
                    }
                } else {
                    TAILQ_INSERT_AFTER(list, e, newExtent, extents);
                    break;
                }
            }
        }
    }
    
}

void fileExtentList_free(FileExtentList* list)
{
    FileExtent* extent = NULL;

    trace("list (%p)", (void *)list);

    while ( (extent = TAILQ_FIRST(list)) ) {
        TAILQ_REMOVE(list, extent, extents);
        SFREE(extent);
    }
    SFREE(list);
}

void inspectHFSCatalogFile(const HFSPlusCatalogFile* file, const HFSPlus* hfs, range blockRange, FileExtentList* fileExtents, hfs_forktype_t forkType) {
    assert(forkType == HFSDataForkType || forkType == HFSResourceForkType);
    
    HFSPlusFork* hfsfork = NULL;
    if ( hfsfork_make(&hfsfork, hfs, (forkType == HFSDataForkType ? file->dataFork : file->resourceFork), forkType, file->fileID) ) {
        die(1, "Could not create fork reference for fileID %u", file->fileID);
    }

    ExtentList* list = hfsfork->extents;

    uint32_t    extentCount = 0;
    uint32_t    extentIndex = 0;
    Extent*     e    = NULL;
    // get total extent count first
    TAILQ_FOREACH(e, list, extents) extentCount++;

    TAILQ_FOREACH(e, list, extents) {

        // first see if any of the extents fall within our given block range
        if (range_contains(e->startBlock, blockRange) || range_contains(e->startBlock + e->blockCount, blockRange)) {
            fileExtentList_sorted_insert(fileExtents, e->startBlock, e->blockCount, file->fileID, extentIndex, extentCount, forkType);
            extentIndex++;
        }
    }

    hfsfork_free(hfsfork);
}

void inspectBlockRange(HIOptions* options)
{
    range blockRange = make_range(options->blockRangeStart, options->blockRangeCount);
    
    HFSPlus* hfs = options->hfs;
    out_ctx* ctx = hfs->vol->ctx;

	if (blockRange.count == 0) {
		blockRange.count = hfs->block_count - blockRange.start;
	}
	
    // Sanity checks
    if (blockRange.count < 1) {
        die(1, "Invalid request size: %zu blocks", blockRange.count);
    }

    if (blockRange.start > hfs->block_count) {
        die(1, "Invalid range: specified start block would begin beyond the end of the volume (start block: %zu; volume block count: %u blocks).", blockRange.start, hfs->block_count);
    }

    if ( range_max(blockRange) > hfs->block_count ) {
        blockRange.count = hfs->block_count - blockRange.start;
        blockRange.count = MAX(blockRange.count, 1);
        debug("Trimmed range to (%zu, %zu) (volume only has %lu blocks)", blockRange.start, blockRange.count, hfs->block_count);
    }

    FileExtentList* fileExtents = fileExtentList_create();
    
    size_t finalBlock = range_max(blockRange);

    /*
        Walk the leaf catalog nodes looking for those with extents in the given block range.
     */

    BTreePtr catalog = NULL;
    hfsplus_get_catalog_btree(&catalog, hfs);
    hfs_cnid_t cnid = catalog->headerRecord.firstLeafNode;

    while (1) {
        BTreeNodePtr node = NULL;
        if ( BTGetNode(&node, catalog, cnid) < 0) {
            perror("get node");
            die(1, "There was an error fetching node %d", cnid);
        }

        for (unsigned recNum = 0; recNum < node->nodeDescriptor->numRecords; recNum++) {

            BTreeKeyPtr recordKey   = NULL;
            void *recordValue       = NULL;
            btree_get_record(&recordKey, &recordValue, node, recNum);

            HFSPlusCatalogRecord *record = (HFSPlusCatalogRecord *)recordValue;
            switch (record->record_type) {
                case kHFSPlusFileRecord:
                {
                    HFSPlusCatalogFile *file = &record->catalogFile;

                    if (file->dataFork.logicalSize) {
                        inspectHFSCatalogFile(file, hfs, blockRange, fileExtents, HFSDataForkType);
                    }

                    if (file->resourceFork.logicalSize) {
                        inspectHFSCatalogFile(file, hfs, blockRange, fileExtents, HFSResourceForkType);
                    }

                    break;
                }
                    
                default:
                {
                    break;
                }
            }
            
        }

        // Follow link
        cnid = node->nodeDescriptor->fLink;
        if (cnid == 0) break;       // End Of Line

        // Console Status
        static int      count     = 0; // count of records so far
        static unsigned iter_size = 0; // update the status after this many records
        if (iter_size == 0) {
            iter_size = (catalog->headerRecord.leafRecords/100000);
            if (iter_size == 0) iter_size = 100;
        }

        count += node->nodeDescriptor->numRecords;

        if ((count % iter_size) == 0) {
            // Update status

            fprintf(stdout, "\r%0.2f%% (searching catalog for files between blocks %zu — %zu)",
                    ((float)count / (float)catalog->headerRecord.leafRecords) * 100.,
                    blockRange.start,
                    finalBlock
                    );
            fflush(stdout);
        }

        btree_free_node(node);
    }

    BeginSection(ctx, "Block Inspection Details");

    Print(ctx, "%10s    %12s %12s    %8s %4s %13s  %s", "CNID", "Start Block", "Block Count", "Extent #", "Fork", "Size", "Path");

    FileExtent* fileExtent      = NULL;

    TAILQ_FOREACH(fileExtent, fileExtents, extents) {
        char size[50];
        (void)format_size(ctx, size, (uint64_t)fileExtent->blockCount * hfs->block_size, 50);

        hfs_str path = "";
        int result = HFSPlusGetCNIDPath(&path, (FSSpec){ hfs, fileExtent->cnid });
        if (result < 0) strlcpy((char *)path, "<unknown>", PATH_MAX);

        char extentNumber[25] = "";
        sprintf(extentNumber, "%u/%u", fileExtent->extentIndex+1, fileExtent->extentCount);

        Print(ctx, "%10u   ┃%12zu %12zu  ┃ %10s %4s %13s  %s",
              fileExtent->cnid,
              fileExtent->startBlock,
              fileExtent->blockCount,
              extentNumber,
              fileExtent->forkType == HFSDataForkType ? "data" : "rsrc",
              size,
              path);

        // if there is freespace between this file and the next, print a record of that

        FileExtent* next = NULL;
        next = TAILQ_NEXT(fileExtent, extents);
        
        if (next == NULL) continue;

        if (fileExtent->startBlock + fileExtent->blockCount == next->startBlock) continue;

        range freespaceRange = make_range(fileExtent->startBlock + fileExtent->blockCount, next->startBlock - (fileExtent->startBlock + fileExtent->blockCount));

        (void)format_size(ctx, size, (uint64_t)freespaceRange.count * hfs->block_size, 50);

        Print(ctx, "%10s   ┃%12zu %12zu  ┃ %10s %4s %13s  %s",
              "",
              freespaceRange.start,
              freespaceRange.count,
              "",
              "",
              size,
              "");

    }

    EndSection(ctx);

    fileExtentList_free(fileExtents);
}

