//
//  free_space.c
//  hfsinspect
//
//  Created by Adam Knight on 4/1/14.
//  Copyright (c) 2014 Adam Knight. All rights reserved.
//

#include "operations.h"


// For freespace statistics
typedef struct FreespaceExtent {
    size_t    startBlock;
    size_t    blockCount;
} FreespaceExtent;

int compareFreespaceExtents(const void* a, const void* b)
{
    const FreespaceExtent* A     = (FreespaceExtent *)a;
    const FreespaceExtent* B     = (FreespaceExtent *)b;

    int         result = cmp(A->blockCount, B->blockCount);
    
    return result;
}

void showFreeSpace(HIOptions* options)
{
    HFSPlusFork* fork = NULL;
    if ( hfsplus_get_special_fork(&fork, options->hfs, kHFSAllocationFileID) < 0 )
        die(1, "Couldn't get a reference to the volume allocation file.");

    char*        data = NULL;
    SALLOC(data, fork->logicalSize);
    {
        size_t  block_size = 128*1024, offset = 0;
        ssize_t bytes      = 0;
        void*   block      = NULL;
        SALLOC(block, block_size);
        while ( (bytes = hfs_read_fork_range(block, fork, block_size, offset)) > 0 ) {
            memcpy(data+offset, block, bytes);
            offset += bytes;
            if (offset >= fork->logicalSize) break;
        }
        SFREE(block);

        if (offset == 0) {
            SFREE(data);

            // We didn't read anything.
            if (bytes < 0) {
                // Error
                die(errno, "error reading allocation file");
            }

            return;
        }
    }

    uint32_t topCount = options->topCount;

    FreespaceExtent* topFreeExtents = NULL;

    if (topCount) {
        SALLOC(topFreeExtents, topCount * sizeof(FreespaceExtent));
    }

    out_ctx* ctx = fork->hfs->vol->ctx;

    if (options->verbose) {
        BeginSection(ctx, "Freespace Details");
        Print(ctx, "%12s %26s %13s", "Start Block", "# Free Contiguous Blocks", "Free Space");
    }

    size_t total_used    = 0;
    size_t total_free    = 0;
    size_t total_extents = 0;

    struct extent {
        size_t  start;
        size_t  length;
        bool    used;
        uint8_t _reserved[7];
    };

    // The first allocation block is used by the VH.
    struct extent currentExtent = {1,0,1};

    for (size_t i = 0; i < options->hfs->vh.totalBlocks; i++) {

        if (!options->verbose) {
            
            // Console Status
            static unsigned iter_size = 0; // update the status after this many blocks
            if (iter_size == 0) {
                iter_size = (options->hfs->vh.totalBlocks/10000);
                if (iter_size == 0) iter_size = 50000;
            }

            if ((i % iter_size) == 0) {
                // Update status
                fprintf(stdout, "\r%0.2f%% (processing: %ju of %ju blocks)",
                        ((float)i / (float)options->hfs->vh.totalBlocks) * 100.,
                        (intmax_t)i,
                        (intmax_t)options->hfs->vh.totalBlocks
                        );
                fflush(stdout);
            }
        }
        
        bool used = BTIsBlockUsed((uint32_t)i, data, (size_t)fork->logicalSize);
        if ((used == currentExtent.used) && (i != (options->hfs->vh.totalBlocks - 1))) {
            currentExtent.length++;
            continue;
        }

        total_extents += 1;

        if (currentExtent.used) {
            total_used += currentExtent.length;

        } else {
            total_free += currentExtent.length;

            if (topCount) {
                FreespaceExtent freespaceExtent = { .startBlock = currentExtent.start, .blockCount = currentExtent.length };

                if (topFreeExtents[0].blockCount < freespaceExtent.blockCount) {
                    topFreeExtents[0].blockCount = freespaceExtent.blockCount;
                    topFreeExtents[0].startBlock = freespaceExtent.startBlock;
                    qsort(topFreeExtents, topCount, sizeof(FreespaceExtent), compareFreespaceExtents);
                }
            }
            
            if (options->verbose) {
                char size[50];
                (void)format_size(ctx, size, (uint64_t)currentExtent.length * fork->hfs->block_size, 50);
                Print(ctx, "%12zu %26zu %13s", currentExtent.start, currentExtent.length, size);
            }
        }

        currentExtent.used   = used;
        currentExtent.start  = i;
        currentExtent.length = 1;
    }
    if (options->verbose) EndSection(ctx); // Freespace Details

    if (topCount) {
        BeginSection(ctx, "Largest Contiguous Freespace Segments");
        Print(ctx, "%12s %26s %13s", "Start Block", "# Free Contiguous Blocks", "Free Space");

        for (int32_t i = topCount-1; i >= 0; i--) {
            if (topFreeExtents[i].blockCount == 0) continue;

            char size[50];
            (void)format_size(ctx, size, (uint64_t)topFreeExtents[i].blockCount * fork->hfs->block_size, 50);

            Print(ctx, "%12zu %26zu %13s", topFreeExtents[i].startBlock, topFreeExtents[i].blockCount, size);
        }
        EndSection(ctx); // Largest Contiguous Freespace Segments
    }


    BeginSection(ctx, "Allocation File Statistics");
    PrintAttribute(ctx, "Segments", "%zu", total_extents);
    _PrintHFSBlocks(ctx, "Used Blocks", total_used);
    _PrintHFSBlocks(ctx, "Free Blocks", total_free);
    _PrintHFSBlocks(ctx, "Total Blocks", total_used + total_free);
    EndSection(ctx); // Allocation File Statistics

    SFREE(topFreeExtents);
    SFREE(data);
    hfsfork_free(fork);
}

