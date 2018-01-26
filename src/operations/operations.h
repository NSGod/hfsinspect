//
//  operations.h
//  hfsinspect
//
//  Created by Adam Knight on 4/1/14.
//  Copyright (c) 2014 Adam Knight. All rights reserved.
//

#ifndef hfsinspect_operations_h
#define hfsinspect_operations_h


#include "volumes/volume.h"
#include "hfs/hfs.h"
#include "hfs/types.h"
#include "hfs/catalog.h"
#include "hfs/output_hfs.h"
#include "hfs/unicode.h"
#include "logging/logging.h"    // console printing routines

extern char* BTreeOptionCatalog;
extern char* BTreeOptionExtents;
extern char* BTreeOptionAttributes;
extern char* BTreeOptionHotfiles;

typedef enum BTreeTypes {
    BTreeTypeCatalog = 0,
    BTreeTypeExtents,
    BTreeTypeAttributes,
    BTreeTypeHotfiles
} BTreeTypes;

enum HIModes {
    HIModeShowVolumeInfo = 0,
    HIModeShowJournalInfo,
    HIModeShowSummary,
    HIModeShowBTreeInfo,
    HIModeShowBTreeNode,
    HIModeShowCatalogRecord,
    HIModeShowCNID,
    HIModeShowPathInfo,
    HIModeExtractFile,
    HIModeListFolder,
    HIModeShowDiskInfo,
    HIModeYankFS,
    HIModeFreeSpace,
    HIModeShowFragmentation,
    HIModeInspectBlockRange,
};

// Configuration context
typedef struct HIOptions {
    HFSPlus*            hfs;                            // 8
    BTreePtr            tree;                           // 8
    FILE*               extract_fp;                     // 8
    HFSPlusFork*        extract_HFSPlusFork;            // 8
    HFSPlusCatalogFile* extract_HFSPlusCatalogFile;     // 8

    uint32_t            mode;                           // 4
    bt_nodeid_t         record_parent;                  // 4
    bt_nodeid_t         cnid;                           // 4
    bt_nodeid_t         node_id;                        // 4
    BTreeTypes          tree_type;                      // 4

    char                device_path[PATH_MAX];          // 1024
    char                file_path[PATH_MAX];            // 1024
    char                record_filename[PATH_MAX];      // 1024
    char                extract_path[PATH_MAX];         // 1024

    uint32_t            topCount;                       // 4
    uint64_t            blockRangeStart;                // 8
    uint64_t            blockRangeCount;                // 8
    
    bool                verbose;                        // 1
} HIOptions;

void set_mode (HIOptions* options, int mode);
void clear_mode (HIOptions* options, int mode);
bool check_mode (HIOptions* options, int mode);

void die(int val, char* format, ...) __attribute__(( noreturn ));

void    showFreeSpace(HIOptions* options);
void    showPathInfo(HIOptions* options);
void    showCatalogRecord(HIOptions* options, FSSpec spec, bool followThreads);
ssize_t extractFork(const HFSPlusFork* fork, const char* extractPath);
void    extractHFSPlusCatalogFile(const HFSPlus* hfs, const HFSPlusCatalogFile* file, const char* extractPath);
void    inspectBlockRange(HIOptions* options);

// For volume statistics
typedef struct Rank {
    uint64_t measure;
    uint32_t cnid;
    uint32_t _reserved;
} Rank;

typedef struct ForkSummary {
    uint64_t count;
    uint64_t fragmentedCount;
    uint64_t blockCount;
    uint64_t logicalSpace;
    uint64_t extentRecords;
    uint64_t extentDescriptors;
    uint64_t overflowExtentRecords;
    uint64_t overflowExtentDescriptors;
} ForkSummary;

typedef struct VolumeSummary {
    uint64_t    nodeCount;
    uint64_t    recordCount;
    uint64_t    fileCount;
    uint64_t    folderCount;
    uint64_t    aliasCount;
    uint64_t    hardLinkFileCount;
    uint64_t    hardLinkFolderCount;
    uint64_t    symbolicLinkCount;
    uint64_t    invisibleFileCount;
    uint64_t    emptyFileCount;
    uint64_t    emptyDirectoryCount;
    
    Rank        *topLargestFiles;
    uint64_t    topLargestFileCount;
    
    ForkSummary dataFork;
    ForkSummary resourceFork;
} VolumeSummary;

// For volume fragmentation statistics
typedef struct FragmentedFile {
    uint64_t    logicalSize;
    uint32_t    cnid;
    uint32_t    fragmentCount;
} FragmentedFile;

typedef struct VolumeFragmentationSummary {
    uint64_t        nodeCount;
    uint64_t        recordCount;
    uint64_t        fileCount;
    uint64_t        folderCount;
    uint64_t        aliasCount;
    uint64_t        hardLinkFileCount;
    uint64_t        hardLinkFolderCount;
    uint64_t        symbolicLinkCount;
    uint64_t        invisibleFileCount;
    uint64_t        emptyFileCount;
    uint64_t        emptyDirectoryCount;

    uint64_t        dataForksLogicalSize;
    uint64_t        resourceForksLogicalSize;
    uint64_t        fragmentedDataForkCount;
    uint64_t        fragmentedResourceForkCount;
    uint32_t        blockSize;
    uint64_t        filesLargerThanBlockSizeCount;
    uint64_t        fragmentedFileCount;

    FragmentedFile  *topFragementedFiles;
    uint64_t        topFragementedFilesCount;
    
} VolumeFragmentationSummary;

VolumeSummary* createVolumeSummary(HIOptions* options);
void          freeVolumeSummary(VolumeSummary* summary) _NONNULL;
void          generateForkSummary(HIOptions* options, ForkSummary* forkSummary, const HFSPlusCatalogFile* file, const HFSPlusForkData* fork, hfs_forktype_t type);
void          PrintVolumeSummary             (out_ctx* ctx, const VolumeSummary* summary) _NONNULL;
void          PrintForkSummary               (out_ctx* ctx, const ForkSummary* summary) _NONNULL;

VolumeFragmentationSummary*     createVolumeFragmentationSummary(HIOptions* options);
void                            freeVolumeFragmentationSummary(VolumeFragmentationSummary* summary) _NONNULL;
void                            generateFragmentedFile(HIOptions* options, FragmentedFile* fragmentedFile, const HFSPlusCatalogFile* file);
void          PrintFragmentedFork(out_ctx* ctx, const HFSPlusFork* hfsfork) _NONNULL;
void          PrintVolumeFragmentationSummary(out_ctx* ctx, const VolumeFragmentationSummary* summary) _NONNULL;
void          PrintFragmentedFile(out_ctx* ctx, uint64_t index, const FragmentedFile* file) _NONNULL;

#endif
