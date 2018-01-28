//
//  hfs_fragmentation.c
//  hfsinspect
//
//  Created by Mark Douma on 12/31/2017.
//  Copyright (c) 2017 Mark Douma. All rights reserved.
//

#include "operations.h"

int compareFragmentedFiles(const void* a, const void* b)
{
    const FragmentedFile* A     = (FragmentedFile *)a;
    const FragmentedFile* B     = (FragmentedFile *)b;

    int         result = cmp(A->fragmentCount, B->fragmentCount);
    
    return result;
}

VolumeFragmentationSummary* createVolumeFragmentationSummary(HIOptions *options)
{
    VolumeFragmentationSummary* summary = NULL;
    SALLOC(summary, sizeof(VolumeFragmentationSummary));

    summary->topFragementedFilesCount = options->topCount;
    
    if (summary->topFragementedFilesCount) {
        SALLOC(summary->topFragementedFiles, summary->topFragementedFilesCount * sizeof(FragmentedFile));
    }


    /*
       Walk the leaf catalog nodes and gather various stats about the volume as a whole.
     */

    HFSPlus* hfs = options->hfs;
    out_ctx* ctx = hfs->vol->ctx;

    summary->blockSize = hfs->vh.blockSize;

    BTreePtr catalog = NULL;
    hfsplus_get_catalog_btree(&catalog, hfs);
    hfs_cnid_t cnid = catalog->headerRecord.firstLeafNode;

    if (options->verbose) BeginSection(ctx, "Volume Fragmentation Details");

    while (1) {
        BTreeNodePtr node = NULL;
        if ( BTGetNode(&node, catalog, cnid) < 0) {
            perror("get node");
            die(1, "There was an error fetching node %d", cnid);
        }

        // Process node
//        debug("Processing node %d", cnid);
        summary->nodeCount++;

        for (unsigned recNum = 0; recNum < node->nodeDescriptor->numRecords; recNum++) {
            summary->recordCount++;

            BTreeKeyPtr recordKey   = NULL;
            void *recordValue = NULL;
            btree_get_record(&recordKey, &recordValue, node, recNum);

            HFSPlusCatalogRecord *record = (HFSPlusCatalogRecord *)recordValue;
            switch (record->record_type) {
                case kHFSPlusFileRecord:
                {
                    summary->fileCount++;

                    HFSPlusCatalogFile *file = &record->catalogFile;

                    // hard links
                    if (HFSPlusCatalogFileIsHardLink(record)) { summary->hardLinkFileCount++; continue; }
                    if (HFSPlusCatalogFolderIsHardLink(record)) { summary->hardLinkFolderCount++; continue; }

                    // symlink
                    if (HFSPlusCatalogRecordIsSymLink(record)) { summary->symbolicLinkCount++; continue; }

                    // alias
                    if (file->userInfo.fdFlags & kIsAlias) summary->aliasCount++;

                    // invisible
                    if (file->userInfo.fdFlags & kIsInvisible) summary->invisibleFileCount++;

                    // compressed
                    if (HFSPlusCatalogRecordIsCompressed(record)) summary->compressedFileCount++;

                    // file sizes
                    if ((file->dataFork.logicalSize == 0) && (file->resourceFork.logicalSize == 0)) { summary->emptyFileCount++; continue; }

                    summary->dataForksLogicalSize += file->dataFork.logicalSize;
                    summary->resourceForksLogicalSize += file->resourceFork.logicalSize;

                    if (file->dataFork.logicalSize > hfs->block_size || file->resourceFork.logicalSize > hfs->block_size) {
                        summary->filesLargerThanBlockSizeCount++;
                    }

                    // not fragmented
                    if ((file->dataFork.extents[1].blockCount == 0) && (file->resourceFork.extents[1].blockCount == 0)) { continue; }

                    if (file->dataFork.extents[1].blockCount > 0) summary->fragmentedDataForkCount++;
                    if (file->resourceFork.extents[1].blockCount > 0) summary->fragmentedResourceForkCount++;

                    if ( (file->dataFork.extents[1].blockCount > 0) || (file->dataFork.extents[1].blockCount > 0) ) summary->fragmentedFileCount++;

                    if (options->topCount || options->verbose) {

                        FragmentedFile fragmentedFile = {0};

                        generateFragmentedFile(options, &fragmentedFile, file);

                        if (summary->topFragementedFilesCount) {
                            if (summary->topFragementedFiles[0].fragmentCount < fragmentedFile.fragmentCount) {
                                summary->topFragementedFiles[0].logicalSize = fragmentedFile.logicalSize;
                                summary->topFragementedFiles[0].cnid = fragmentedFile.cnid;
                                summary->topFragementedFiles[0].fragmentCount = fragmentedFile.fragmentCount;

                                qsort(summary->topFragementedFiles, summary->topFragementedFilesCount, sizeof(FragmentedFile), compareFragmentedFiles);
                            }
                        }

                    }

                    break;
                }

                case kHFSPlusFolderRecord:
                {
                    summary->folderCount++;

                    HFSPlusCatalogFolder *folder = &record->catalogFolder;
                    if (folder->valence == 0) summary->emptyDirectoryCount++;
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

        if (!options->verbose) {
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
                size_t space     = summary->dataForksLogicalSize + summary->resourceForksLogicalSize;
                char   size[128] = {0};
                (void)format_size(hfs->vol->ctx, size, space, 128);

                fprintf(stdout, "\r%0.2f%% (files: %ju; directories: %ju; size: %s)",
                        ((float)count / (float)catalog->headerRecord.leafRecords) * 100.,
                        (intmax_t)summary->fileCount,
                        (intmax_t)summary->folderCount,
                        size
                        );
                fflush(stdout);
            }
        }

        btree_free_node(node);
    }

    if (options->verbose) EndSection(ctx); // Volume Fragmentation Details

    return summary;
}

void freeVolumeFragmentationSummary(VolumeFragmentationSummary* summary)
{
    SFREE(summary->topFragementedFiles);
    SFREE(summary);
}

void PrintFragmentedFork(out_ctx* ctx, const HFSPlusFork* hfsfork)
{
    // There's probably a better way to do this, but until then...

    uint32_t totalFragmentCount = 0;

    ExtentList* list = hfsfork->extents;

    Extent*     e       = NULL;
    TAILQ_FOREACH(e, list, extents) { totalFragmentCount++; }

    size_t mapStrLen = 1 + (totalFragmentCount * 11) + 2; // assume a max of 11 chars per blockCount "segment"
    char* mapStr = NULL;
    SALLOC(mapStr, mapStrLen);

    strlcpy(mapStr, ":", mapStrLen);

    TAILQ_FOREACH(e, list, extents) {
        char blockCountStr[12] = "";
        sprintf(blockCountStr, "%u:", (uint32_t)e->blockCount);
        strlcat(mapStr, blockCountStr, mapStrLen);
    }

    hfs_str forkPath = "";
    int result = HFSPlusGetCNIDPath(&forkPath, (FSSpec){ hfsfork->hfs, hfsfork->cnid } );
    if (result < 0) strlcpy((char *)forkPath, "<unknown>", PATH_MAX);

    fprintf(stdout, "cnid=%u fork=%s map=%s bytes=%llu extents=%u path=%s\n",
            hfsfork->cnid,
            hfsfork->forkType == HFSDataForkType ? "data" : "rsrc",
            mapStr,
            hfsfork->logicalSize,
            totalFragmentCount,
            forkPath
            );
    fflush(stdout);

    SFREE(mapStr);
}

void countFragmentsInFork(HIOptions* options, FragmentedFile* fragmentedFile, const HFSPlusCatalogFile* file, hfs_forktype_t forkType)
{
    assert(forkType == HFSDataForkType || forkType == HFSResourceForkType);
    HFSPlusFork* hfsfork = NULL;
    if ( hfsfork_make(&hfsfork, options->hfs, (forkType == HFSDataForkType ? file->dataFork : file->resourceFork), forkType, file->fileID) ) {
        die(1, "Could not create fork reference for fileID %u", file->fileID);
    }

    uint32_t fragmentCount = 0;

    ExtentList* list = hfsfork->extents;

    Extent*     e       = NULL;
    TAILQ_FOREACH(e, list, extents) { fragmentCount++; }

    if (options->verbose) {
        PrintFragmentedFork(options->hfs->vol->ctx, hfsfork);
    }
    
    fragmentedFile->fragmentCount += fragmentCount;

    hfsfork_free(hfsfork);
}

void generateFragmentedFile(HIOptions* options, FragmentedFile* fragmentedFile, const HFSPlusCatalogFile* file)
{
    fragmentedFile->cnid = file->fileID;
    fragmentedFile->logicalSize = file->dataFork.logicalSize + file->resourceFork.logicalSize;

    if (file->dataFork.logicalSize) countFragmentsInFork(options, fragmentedFile, file, HFSDataForkType);
    if (file->resourceFork.logicalSize) countFragmentsInFork(options, fragmentedFile, file, HFSResourceForkType);
}

void PrintVolumeFragmentationSummary(out_ctx* ctx, const VolumeFragmentationSummary* summary)
{
    BeginSection  (ctx, "Volume Fragmentation Summary");
    ctx->wideAttributes = true;

    PrintUI            (ctx, summary, nodeCount);
    PrintUI            (ctx, summary, recordCount);
    PrintUI            (ctx, summary, fileCount);
    PrintUI            (ctx, summary, folderCount);
    PrintUI            (ctx, summary, aliasCount);
    PrintUI            (ctx, summary, hardLinkFileCount);
    PrintUI            (ctx, summary, hardLinkFolderCount);
    PrintUI            (ctx, summary, symbolicLinkCount);
    PrintUI            (ctx, summary, invisibleFileCount);
    PrintUI            (ctx, summary, emptyFileCount);
    PrintUI            (ctx, summary, emptyDirectoryCount);
    PrintUI            (ctx, summary, compressedFileCount);
    PrintDataLength    (ctx, summary, dataForksLogicalSize);
    PrintDataLength    (ctx, summary, resourceForksLogicalSize);
    PrintUI            (ctx, summary, fragmentedDataForkCount);
    PrintUI            (ctx, summary, fragmentedResourceForkCount);
    PrintDataLength    (ctx, summary, blockSize);
    PrintAttribute(ctx, "filesLargerThanBlockSizeCount", "%llu of %llu (%0.1f%%)",
                    summary->filesLargerThanBlockSizeCount,
                    summary->fileCount,
                    100.0 * ((double)summary->filesLargerThanBlockSizeCount/(double)summary->fileCount));

    PrintAttribute(ctx, "fragmentedFileCount", "%llu (%0.5f)",
                    summary->fragmentedFileCount,
                    (double)summary->fragmentedFileCount/(double)summary->fileCount);

    Print(ctx, "");
    Print(ctx, "# Effective Fragmentation: %llu out of %llu possible files are fragmented (%0.2f%%)",
          summary->fragmentedFileCount,
          summary->filesLargerThanBlockSizeCount,
          100.0 * ((double)summary->fragmentedFileCount/(double)summary->filesLargerThanBlockSizeCount));

    if (summary->topFragementedFilesCount) {
        BeginSection(ctx, "Most Fragmented Files");
        Print(ctx, "%4s %10s %13s %10s  %s", "#", "CNID", "Size", "Fragments", "Path");

        for (int64_t i = summary->topFragementedFilesCount - 1; i >= 0; i--) {
            if (summary->topFragementedFiles[i].cnid == 0) continue;

            PrintFragmentedFile(ctx, summary->topFragementedFilesCount - i, &summary->topFragementedFiles[i]);
        }
        EndSection(ctx); // most fragmented files
    }

    ctx->wideAttributes = false;

    EndSection(ctx); // volume fragmentation summary
}

void PrintFragmentedFile(out_ctx* ctx, uint64_t index, const FragmentedFile* file)
{
    char size[50];
    (void)format_size(ctx, size, file->logicalSize, 50);

    hfs_str     fullPath      = "";
    int result = HFSPlusGetCNIDPath(&fullPath, (FSSpec){ get_hfs_volume(), file->cnid });
    if (result < 0) strlcpy((char *)fullPath, "<unknown>", PATH_MAX);
    Print(ctx, "%4llu %10u %13s %10u  %s", index, file->cnid, size, file->fragmentCount, fullPath);
}
