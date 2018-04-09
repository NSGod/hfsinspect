//
//  output_hfs.c
//  hfsinspect
//
//  Created by Adam Knight on 5/5/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include "hfs/output_hfs.h"
#include "volumes/output.h"
#include "memdmp/memdmp.h"
#include "hfs/unicode.h"
#include "hfs/types.h"
#include "hfs/catalog.h"
#include "hfs/hfs.h"
#include "logging/logging.h"    // console printing routines
#include "hfsplus/attributes.h"

static HFSPlus* volume_ = NULL;
void set_hfs_volume(HFSPlus* v) { volume_ = v; }
HFSPlus* get_hfs_volume(void) { return volume_; }

#pragma mark - Value Print Functions

void _PrintCatalogPath(out_ctx* ctx, char* label, bt_nodeid_t cnid)
{
    hfs_str name = "";
    if (cnid != 0) {
        if ( (HFSPlusGetCNIDPath(&name, (FSSpec){volume_, cnid})) < 0) {
            // if the file/folder isn't found, print '<missing>' like `bless` does
            strlcpy((char *)name, "<missing>", PATH_MAX);
        }
    }
    if (cnid != 0) {
        PrintAttribute(ctx, label, "%u => %s", cnid, name);
    } else {
        PrintAttribute(ctx, label, "%u %s", cnid, name);
    }
}

void PrintPath(out_ctx* ctx, char* label, bt_nodeid_t cnid)
{
    hfs_str path = "";
    int result = HFSPlusGetCNIDPath(&path, (FSSpec){ volume_, cnid });
    if (result < 0) strlcpy((char *)path, "<unknown>", PATH_MAX);
    PrintAttribute(ctx, label, "%s", path);
}

void _PrintCatalogName(out_ctx* ctx, char* label, bt_nodeid_t cnid)
{
    hfs_str name = "";
    if (cnid != 0)
        HFSPlusGetCNIDName(&name, (FSSpec){volume_, cnid});

    PrintAttribute(ctx, label, "%u (%s)", cnid, name);
}

void _PrintHFSBlocks(out_ctx* ctx, const char* label, uint64_t blocks)
{
    char sizeLabel[50] = "";
    (void)format_blocks(ctx, sizeLabel, blocks, volume_->block_size, 50);
    PrintAttribute(ctx, label, "%s", sizeLabel);
}

void _PrintHFSTimestamp(out_ctx* ctx, const char* label, uint32_t timestamp)
{
    if (timestamp == 0) {
        PrintAttribute(ctx, label, "0");
        return;
    }
    char buf[50];
    (void)format_hfs_timestamp(ctx, buf, timestamp, 50);
    PrintAttribute(ctx, label, "%s", buf);
}

void PrintHFSUniStr255(out_ctx* ctx, const char* label, const HFSUniStr255* record)
{
    hfs_str name = "";
    hfsuc_to_str(&name, record);
    PrintAttribute(ctx, label, "\"%s\" (%u)", name, record->length);
}

void PrintHFSPlusAttrStr127(out_ctx* ctx, const char* label, const HFSPlusAttrStr127* record)
{
    hfs_attr_str name = "";
    attruc_to_attrstr(&name, record);
    PrintAttribute(ctx, label, "\"%s\" (%u)", name, record->attrNameLen);
}

#pragma mark - Structure Print Functions

void PrintVolumeInfo(out_ctx* ctx, const HFSPlus* hfs)
{
    if (hfs->vh.signature == kHFSPlusSigWord)
        BeginSection(ctx, "HFS+ Volume Format (v%d)", hfs->vh.version);
    else if (hfs->vh.signature == kHFSXSigWord)
        BeginSection(ctx, "HFSX Volume Format (v%d)", hfs->vh.version);
    else
        BeginSection(ctx, "Unknown Volume Format"); // Curious.

    hfs_str  volumeName = "";
    int      success    = HFSPlusGetCNIDName(&volumeName, (FSSpec){hfs, kHFSRootFolderID});
    if (success)
        PrintAttribute(ctx, "volume name", "%s", volumeName);

    BTreePtr catalog    = NULL;
    hfsplus_get_catalog_btree(&catalog, hfs);

    if ((hfs->vh.signature == kHFSXSigWord) && (catalog->headerRecord.keyCompareType == kHFSBinaryCompare)) {
        PrintAttribute(ctx, "case sensitivity", "case sensitive");
    } else {
        PrintAttribute(ctx, "case sensitivity", "case insensitive");
    }

    HFSPlusVolumeFinderInfo finderInfo = { .finderInfo = {*hfs->vh.finderInfo} };
    if (finderInfo.bootDirID || finderInfo.bootParentID || finderInfo.os9DirID || finderInfo.osXDirID) {
        PrintAttribute(ctx, "bootable", "yes");
    } else {
        PrintAttribute(ctx, "bootable", "no");
    }
    EndSection(ctx);
}

void PrintHFSMasterDirectoryBlock(out_ctx* ctx, const HFSMasterDirectoryBlock* vcb)
{
    BeginSection(ctx, "HFS Master Directory Block");

    PrintUIChar(ctx, vcb, drSigWord);
    PrintHFSTimestamp(ctx, vcb, drCrDate);
    PrintHFSTimestamp(ctx, vcb, drLsMod);
    PrintUIBinary(ctx, vcb, drAtrb);
    PrintUI(ctx, vcb, drNmFls);
    PrintUI(ctx, vcb, drVBMSt);
    PrintUI(ctx, vcb, drAllocPtr);
    PrintUI(ctx, vcb, drNmAlBlks);
    PrintDataLength(ctx, vcb, drAlBlkSiz);
    PrintDataLength(ctx, vcb, drClpSiz);
    PrintUI(ctx, vcb, drAlBlSt);
    PrintUI(ctx, vcb, drNxtCNID);
    PrintUI(ctx, vcb, drFreeBks);

    size_t name_len = sizeof(vcb->drVN);
    char   name[name_len]; memset(name, '\0', name_len); memcpy(name, vcb->drVN, (name_len - 1));
    PrintAttribute(ctx, "drVN", "%s", name);

    PrintHFSTimestamp(ctx, vcb, drVolBkUp);
    PrintUI(ctx, vcb, drVSeqNum);
    PrintUI(ctx, vcb, drWrCnt);
    PrintDataLength(ctx, vcb, drXTClpSiz);
    PrintDataLength(ctx, vcb, drCTClpSiz);
    PrintUI(ctx, vcb, drNmRtDirs);
    PrintUI(ctx, vcb, drFilCnt);
    PrintUI(ctx, vcb, drDirCnt);
    PrintUI(ctx, vcb, drFndrInfo[0]);
    PrintUI(ctx, vcb, drFndrInfo[1]);
    PrintUI(ctx, vcb, drFndrInfo[2]);
    PrintUI(ctx, vcb, drFndrInfo[3]);
    PrintUI(ctx, vcb, drFndrInfo[4]);
    PrintUI(ctx, vcb, drFndrInfo[5]);
    PrintUI(ctx, vcb, drFndrInfo[6]);
    PrintUI(ctx, vcb, drFndrInfo[7]);
    PrintUIChar(ctx, vcb, drEmbedSigWord);
    PrintUI(ctx, vcb, drEmbedExtent.startBlock);
    PrintUI(ctx, vcb, drEmbedExtent.blockCount);
    EndSection(ctx);
}

void PrintVolumeHeader(out_ctx* ctx, const HFSPlusVolumeHeader* vh)
{
    BeginSection(ctx, "HFS Plus Volume Header");
    PrintUIChar         (ctx, vh, signature);
    PrintUI             (ctx, vh, version);

    PrintUIBinary       (ctx, vh, attributes);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSVolumeHardwareLockMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSVolumeUnmountedMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSVolumeSparedBlocksMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSVolumeNoCacheRequiredMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSBootVolumeInconsistentMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSCatalogNodeIDsReusedMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSVolumeJournaledMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSVolumeInconsistentMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSVolumeSoftwareLockMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSUnusedNodeFixMask);
    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSContentProtectionMask);
//    PrintUIFlagIfMatch  (ctx, vh->attributes, kHFSMDBAttributesMask);

    PrintUIChar         (ctx, vh, lastMountedVersion);
    PrintUI             (ctx, vh, journalInfoBlock);
    PrintHFSTimestamp   (ctx, vh, createDate);
    PrintHFSTimestamp   (ctx, vh, modifyDate);
    PrintHFSTimestamp   (ctx, vh, backupDate);
    PrintHFSTimestamp   (ctx, vh, checkedDate);
    PrintUI             (ctx, vh, fileCount);
    PrintUI             (ctx, vh, folderCount);
    PrintDataLength     (ctx, vh, blockSize);
    PrintHFSBlocks      (ctx, vh, totalBlocks);
    PrintHFSBlocks      (ctx, vh, freeBlocks);
    PrintUI             (ctx, vh, nextAllocation);
    PrintDataLength     (ctx, vh, rsrcClumpSize);
    PrintDataLength     (ctx, vh, dataClumpSize);
    PrintUI             (ctx, vh, nextCatalogID);
    PrintUI             (ctx, vh, writeCount);

    PrintUIBinary       (ctx, vh, encodingsBitmap);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacRoman);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacJapanese);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacChineseTrad);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacKorean);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacArabic);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacHebrew);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacGreek);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacCyrillic);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacDevanagari);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacGurmukhi);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacGujarati);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacOriya);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacBengali);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacTamil);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacTelugu);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacKannada);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacMalayalam);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacSinhalese);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacBurmese);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacKhmer);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacThai);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacLaotian);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacGeorgian);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacArmenian);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacChineseSimp);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacTibetan);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacMongolian);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacEthiopic);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacCentralEurRoman);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacVietnamese);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacExtArabic);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacSymbol);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacDingbats);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacTurkish);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacCroatian);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacIcelandic);
    PrintUIFlagIfSet    (ctx, vh->encodingsBitmap, kTextEncodingMacRomanian);
    if (vh->encodingsBitmap & ((uint64_t)1 << 49)) PrintAttribute(ctx, NULL, "%s (%u)", "kTextEncodingMacFarsi", 49);
    if (vh->encodingsBitmap & ((uint64_t)1 << 48)) PrintAttribute(ctx, NULL, "%s (%u)", "kTextEncodingMacUkrainian", 48);

    BeginSection(ctx, "Finder Info");

    HFSPlusVolumeFinderInfo* finderInfo = (void*)&vh->finderInfo;

    PrintCatalogPath    (ctx, finderInfo, bootDirID);
    PrintCatalogPath    (ctx, finderInfo, bootParentID);
    PrintCatalogPath    (ctx, finderInfo, openWindowDirID);
    PrintCatalogPath    (ctx, finderInfo, os9DirID);
    PrintUI             (ctx, finderInfo, reserved);
    PrintCatalogPath    (ctx, finderInfo, osXDirID);
    PrintAttribute      (ctx, "volID", "%#llx", finderInfo->volID);

    EndSection(ctx);

    BeginSection(ctx, "Allocation Bitmap File");
    PrintHFSPlusForkData(ctx, &vh->allocationFile, kHFSAllocationFileID, HFSDataForkType);
    EndSection(ctx);

    BeginSection(ctx, "Extents Overflow File");
    PrintHFSPlusForkData(ctx, &vh->extentsFile, kHFSExtentsFileID, HFSDataForkType);
    EndSection(ctx);

    BeginSection(ctx, "Catalog File");
    PrintHFSPlusForkData(ctx, &vh->catalogFile, kHFSCatalogFileID, HFSDataForkType);
    EndSection(ctx);

    BeginSection(ctx, "Attributes File");
    PrintHFSPlusForkData(ctx, &vh->attributesFile, kHFSAttributesFileID, HFSDataForkType);
    EndSection(ctx);

    BeginSection(ctx, "Startup File");
    PrintHFSPlusForkData(ctx, &vh->startupFile, kHFSStartupFileID, HFSDataForkType);
    EndSection(ctx);
}

void PrintExtentList(out_ctx* ctx, const ExtentList* list, uint32_t totalBlocks)
{
    PrintAttribute(ctx, "extents", "%12s %12s %12s", "startBlock", "blockCount", "% of file");
    int     usedExtents   = 0;
    int     catalogBlocks = 0;
    float   total         = 0;

    Extent* e             = NULL;

    TAILQ_FOREACH(e, list, extents) {
        usedExtents++;
        catalogBlocks += e->blockCount;

        if (totalBlocks) {
            float pct = (float)e->blockCount/(float)totalBlocks*100.f;
            total += pct;
            PrintAttribute(ctx, "", "%12zu %12zu %12.2f", e->startBlock, e->blockCount, pct);
        } else {
            PrintAttribute(ctx, "", "%12zu %12zu %12s", e->startBlock, e->blockCount, "?");
        }
    }

    char sumLine[50] = {'\0'};
    memset(sumLine, '-', 38);
    PrintAttribute(ctx, "", "%s", sumLine);

    if (totalBlocks) {
        PrintAttribute(ctx, "", "%4d extents %12d %12.2f", usedExtents, catalogBlocks, total);
    } else {
        PrintAttribute(ctx, "", "%12s %12d %12s", "", catalogBlocks, "?");
    }

//    PrintAttribute(ctx, "", "%d allocation blocks in %d extents total.", catalogBlocks, usedExtents);
    PrintAttribute(ctx, "", "%0.2f blocks per extent on average.", ( (float)catalogBlocks / (float)usedExtents) );
}

void PrintForkExtentsSummary(out_ctx* ctx, const HFSPlusFork* fork)
{
    debug("Printing extents summary");

    PrintExtentList(ctx, fork->extents, fork->totalBlocks);
}

void PrintHFSPlusForkData(out_ctx* ctx, const HFSPlusForkData* fork, uint32_t cnid, uint8_t forktype)
{
    if (forktype == HFSDataForkType) {
        PrintAttribute(ctx, "fork", "data");
    } else if (forktype == HFSResourceForkType) {
        PrintAttribute(ctx, "fork", "resource");
    }
    if (fork->logicalSize == 0) {
        PrintAttribute(ctx, "logicalSize", "(empty)");
        return;
    }

    PrintDataLength (ctx, fork, logicalSize);
    PrintDataLength (ctx, fork, clumpSize);
    PrintHFSBlocks  (ctx, fork, totalBlocks);

    if (fork->totalBlocks) {
        HFSPlusFork* hfsfork;
        if ( hfsfork_make(&hfsfork, volume_, *fork, forktype, cnid) < 0 ) {
            critical("Could not create fork for fileID %u", cnid);
            return;
        }
        PrintForkExtentsSummary(ctx, hfsfork);
        hfsfork_free(hfsfork);
        hfsfork = NULL;
    }
}

void PrintBTNodeDescriptor(out_ctx* ctx, const BTNodeDescriptor* node)
{
    BeginSection(ctx, "B-Tree Node Descriptor");
    PrintUI(ctx, node, fLink);
    PrintUI(ctx, node, bLink);
    PrintLabeledConstIfEqual(ctx, node, kind, kBTLeafNode);
    PrintLabeledConstIfEqual(ctx, node, kind, kBTIndexNode);
    PrintLabeledConstIfEqual(ctx, node, kind, kBTHeaderNode);
    PrintLabeledConstIfEqual(ctx, node, kind, kBTMapNode);
    PrintUI(ctx, node, height);
    PrintUI(ctx, node, numRecords);
    PrintUI(ctx, node, reserved);
    EndSection(ctx);
}

void PrintBTHeaderRecord(out_ctx* ctx, const BTHeaderRec* hr)
{
    BeginSection(ctx, "B-Tree Header Record");
    PrintUI         (ctx, hr, treeDepth);
    PrintUI         (ctx, hr, rootNode);
    PrintUI         (ctx, hr, leafRecords);
    PrintUI         (ctx, hr, firstLeafNode);
    PrintUI         (ctx, hr, lastLeafNode);
    PrintDataLength (ctx, hr, nodeSize);
    PrintUI         (ctx, hr, maxKeyLength);
    PrintUI         (ctx, hr, totalNodes);
    PrintUI         (ctx, hr, freeNodes);
    PrintUI         (ctx, hr, reserved1);
    PrintDataLength (ctx, hr, clumpSize);

    PrintLabeledConstIfEqual(ctx, hr, btreeType, kBTHFSTreeType);
    PrintLabeledConstIfEqual(ctx, hr, btreeType, kBTUserTreeType);
    PrintLabeledConstIfEqual(ctx, hr, btreeType, kBTReservedTreeType);

    PrintLabeledConstHexIfEqual(ctx, hr, keyCompareType, kHFSCaseFolding);
    PrintLabeledConstHexIfEqual(ctx, hr, keyCompareType, kHFSBinaryCompare);

    PrintUIBinary(ctx, hr, attributes);
    PrintUIFlagIfMatch(ctx, hr->attributes, kBTBadCloseMask);
    PrintUIFlagIfMatch(ctx, hr->attributes, kBTBigKeysMask);
    PrintUIFlagIfMatch(ctx, hr->attributes, kBTVariableIndexKeysMask);

    EndSection(ctx);
}

int _genModeString(char* modeString, uint16_t mode)
{
    if (S_ISBLK(mode)) {
        modeString[0] = 'b';
    }
    if (S_ISCHR(mode)) {
        modeString[0] = 'c';
    }
    if (S_ISDIR(mode)) {
        modeString[0] = 'd';
    }
    if (S_ISFIFO(mode)) {
        modeString[0] = 'p';
    }
    if (S_ISREG(mode)) {
        modeString[0] = '-';
    }
    if (S_ISLNK(mode)) {
        modeString[0] = 'l';
    }
    if (S_ISSOCK(mode)) {
        modeString[0] = 's';
    }
#if defined(BSD)
    if (S_ISWHT(mode)) {
        modeString[0] = 'x';
    }
#endif

    modeString[1] = (mode & S_IRUSR ? 'r' : '-');
    modeString[2] = (mode & S_IWUSR ? 'w' : '-');

    modeString[4] = (mode & S_IRGRP ? 'r' : '-');
    modeString[5] = (mode & S_IWGRP ? 'w' : '-');

    modeString[7] = (mode & S_IROTH ? 'r' : '-');
    modeString[8] = (mode & S_IWOTH ? 'w' : '-');

    if ((mode & S_ISUID) && !(mode & S_IXUSR)) {
        modeString[3] = 'S';
    } else if ((mode & S_ISUID) && (mode & S_IXUSR)) {
        modeString[3] = 's';
    } else if (!(mode & S_ISUID) && (mode & S_IXUSR)) {
        modeString[3] = 'x';
    } else {
        modeString[3] = '-';
    }

    if ((mode & S_ISGID) && !(mode & S_IXGRP)) {
        modeString[6] = 'S';
    } else if ((mode & S_ISGID) && (mode & S_IXGRP)) {
        modeString[6] = 's';
    } else if (!(mode & S_ISGID) && (mode & S_IXGRP)) {
        modeString[6] = 'x';
    } else {
        modeString[6] = '-';
    }

    if ((mode & S_ISVTX) && !(mode & S_IXOTH)) {
        modeString[9] = 'T';
    } else if ((mode & S_ISVTX) && (mode & S_IXOTH)) {
        modeString[9] = 't';
    } else if (!(mode & S_ISVTX) && (mode & S_IXOTH)) {
        modeString[9] = 'x';
    } else {
        modeString[9] = '-';
    }

    modeString[10] = '\0';

    return (int)strlen(modeString);
}

void PrintHFSPlusBSDInfo(out_ctx* ctx, const HFSPlusBSDInfo* record, bool isHardLink)
{
    if (isHardLink == false) {
        PrintUI(ctx, record, ownerID);
        PrintUI(ctx, record, groupID);
    } else {
        PrintAttribute(ctx, "ownerID", "%u (previous link ID)", record->ownerID);
        PrintAttribute(ctx, "groupID", "%u (next link ID)", record->groupID);
    }
    
    int   flagMasks[] = {
        UF_NODUMP,
        UF_IMMUTABLE,
        UF_APPEND,
        UF_OPAQUE,
        UF_COMPRESSED,
        UF_TRACKED,
        UF_HIDDEN,
        SF_ARCHIVED,
        SF_IMMUTABLE,
        SF_APPEND,
    };

    char* flagNames[] = {
        "no dump",
        "immutable",
        "appendOnly",
        "directoryOpaque",
        "compressed",
        "tracked",
        "hidden",
        "archived (superuser)",
        "immutable (superuser)",
        "append (superuser)",
    };

    PrintUIOct(ctx, record, adminFlags);

    for (unsigned i = 0; i < 10; i++) {
        uint8_t flag = record->adminFlags;
        if (flag & flagMasks[i]) {
            PrintAttribute(ctx, NULL, "%05o %s", flagMasks[i], flagNames[i]);
        }
    }

    PrintUIOct(ctx, record, ownerFlags);

    for (unsigned i = 0; i < 10; i++) {
        uint8_t flag = record->ownerFlags;
        if (flag & flagMasks[i]) {
            PrintAttribute(ctx, NULL, "%05o %s", flagMasks[i], flagNames[i]);
        }
    }

    uint16_t mode = record->fileMode;

    char     modeString[11];
    _genModeString(modeString, mode);
    PrintAttribute(ctx, "fileMode", "%s", modeString);

    PrintUIOct(ctx, record, fileMode);

    PrintConstOctIfEqual(ctx, mode & S_IFMT, S_IFBLK);
    PrintConstOctIfEqual(ctx, mode & S_IFMT, S_IFCHR);
    PrintConstOctIfEqual(ctx, mode & S_IFMT, S_IFDIR);
    PrintConstOctIfEqual(ctx, mode & S_IFMT, S_IFIFO);
    PrintConstOctIfEqual(ctx, mode & S_IFMT, S_IFREG);
    PrintConstOctIfEqual(ctx, mode & S_IFMT, S_IFLNK);
    PrintConstOctIfEqual(ctx, mode & S_IFMT, S_IFSOCK);
//    PrintConstOctIfEqual(ctx, mode & S_IFMT, S_IFWHT);

    PrintUIOctFlagIfMatch(ctx, mode, S_ISUID);
    PrintUIOctFlagIfMatch(ctx, mode, S_ISGID);
    PrintUIOctFlagIfMatch(ctx, mode, S_ISVTX);

    PrintUIOctFlagIfMatch(ctx, mode, S_IRUSR);
    PrintUIOctFlagIfMatch(ctx, mode, S_IWUSR);
    PrintUIOctFlagIfMatch(ctx, mode, S_IXUSR);

    PrintUIOctFlagIfMatch(ctx, mode, S_IRGRP);
    PrintUIOctFlagIfMatch(ctx, mode, S_IWGRP);
    PrintUIOctFlagIfMatch(ctx, mode, S_IXGRP);

    PrintUIOctFlagIfMatch(ctx, mode, S_IROTH);
    PrintUIOctFlagIfMatch(ctx, mode, S_IWOTH);
    PrintUIOctFlagIfMatch(ctx, mode, S_IXOTH);


    PrintUI(ctx, record, special.iNodeNum);
}

void PrintFndrFileInfo(out_ctx* ctx, const FndrFileInfo* record)
{
    PrintUIChar(ctx, record, fdType);
    PrintUIChar(ctx, record, fdCreator);
    PrintUIBinary(ctx, record, fdFlags);
    PrintFinderFlags(ctx, record->fdFlags);
    PrintAttribute(ctx, "fdLocation", "(%d, %d)", record->fdLocation.v, record->fdLocation.h);
    PrintInt(ctx, record, opaque);
}

void PrintFndrDirInfo(out_ctx* ctx, const FndrDirInfo* record)
{
    PrintAttribute(ctx, "frRect", "(%d, %d, %d, %d)", record->frRect.top, record->frRect.left, record->frRect.bottom, record->frRect.right);
    PrintUIBinary(ctx, record, frFlags);
    PrintFinderFlags    (ctx, record->frFlags);
    PrintAttribute(ctx, "frLocation", "(%u, %u)", record->frLocation.v, record->frLocation.h);
    PrintInt            (ctx, record, opaque);
}

enum {
    kLabelColorNone         = 0,
    kLabelColorGray         = 1,
    kLabelColorGreen        = 2,
    kLabelColorPurple       = 3,
    kLabelColorBlue         = 4,
    kLabelColorYellow       = 5,
    kLabelColorRed          = 6,
    kLabelColorOrange       = 7,
};

void PrintFinderFlags(out_ctx* ctx, uint16_t record)
{
    uint16_t kIsOnDesk            = 0x0001;      /* Files and folders (System 6) */
    //  NOTE: kColor is the Finder label color, which is 3 bits in size, for a total of 8 different values
    uint16_t kColor               = 0x000E;      /* Files and folders */
    uint16_t kRequireSwitchLaunch = 0x0020;      /* bit 0x0020 was kRequireSwitchLaunch, but is now reserved for future use */
    uint16_t kIsShared            = 0x0040;      /* Files only (Applications only) If clear, the application needs to write to
                                                    its resource fork, and therefore cannot be shared on a server */
    uint16_t kHasNoINITs          = 0x0080;      /* Files only (Extensions/Control Panels only) This file contains no INIT resource */
    uint16_t kHasBeenInited       = 0x0100;      /* Files only.  Clear if the file */
                                                 /* bit 0x0200 was the letter bit for AOCE, but is now reserved for future use */
    uint16_t kHasCustomIcon       = 0x0400;      /* Files and folders */
    uint16_t kIsStationery        = 0x0800;      /* Files only */
    uint16_t kNameLocked          = 0x1000;      /* Files and folders */
    uint16_t kHasBundle           = 0x2000;      /* Files and folders */
                                            /* Indicates that a file has a BNDL resource */
                                            /* Indicates that a folder is displayed as a package */
    uint16_t kIsInvisible         = 0x4000;      /* Files and folders */
    uint16_t kIsAlias             = 0x8000;      /* Files only */

    uint16_t labelColor = record & kColor;
    labelColor = (labelColor >> 1L);

#define PrintFinderLabelColorIfEqual(var, val) if (var == val) { \
    PrintAttribute(ctx, NULL, "kColor == %s (%u)", #val, val); \
}

    PrintUIFlagIfMatch(ctx, record, kIsOnDesk);

    PrintFinderLabelColorIfEqual(labelColor, kLabelColorGray);
    PrintFinderLabelColorIfEqual(labelColor, kLabelColorGreen);
    PrintFinderLabelColorIfEqual(labelColor, kLabelColorPurple);
    PrintFinderLabelColorIfEqual(labelColor, kLabelColorBlue);
    PrintFinderLabelColorIfEqual(labelColor, kLabelColorYellow);
    PrintFinderLabelColorIfEqual(labelColor, kLabelColorRed);
    PrintFinderLabelColorIfEqual(labelColor, kLabelColorOrange);

    PrintHexFlagIfMatch(ctx, record, kRequireSwitchLaunch);
    PrintHexFlagIfMatch(ctx, record, kIsShared);
    PrintHexFlagIfMatch(ctx, record, kHasNoINITs);
    PrintHexFlagIfMatch(ctx, record, kHasBeenInited);
    PrintHexFlagIfMatch(ctx, record, kHasCustomIcon);
    PrintHexFlagIfMatch(ctx, record, kIsStationery);
    PrintHexFlagIfMatch(ctx, record, kNameLocked);
    PrintHexFlagIfMatch(ctx, record, kHasBundle);
    PrintHexFlagIfMatch(ctx, record, kIsInvisible);
    PrintHexFlagIfMatch(ctx, record, kIsAlias);
}

void PrintExtendedFinderFlags(out_ctx* ctx, uint16_t record)
{
    // From Finder.h:
    uint16_t kExtendedFlagsAreInvalid      = 0x8000; /* If set the other extended flags are ignored */
    uint16_t kExtendedFlagHasCustomBadge   = 0x0100; /* Set if the file or folder has a badge resource */
    uint16_t kExtendedFlagObjectIsBusy     = 0x0080; /* Set if the object is marked as busy/incomplete */
    uint16_t kExtendedFlagHasRoutingInfo   = 0x0004; /* Set if the file contains routing info resource */

    PrintHexFlagIfMatch(ctx, record, kExtendedFlagsAreInvalid);
    PrintHexFlagIfMatch(ctx, record, kExtendedFlagHasCustomBadge);
    PrintHexFlagIfMatch(ctx, record, kExtendedFlagObjectIsBusy);
    PrintHexFlagIfMatch(ctx, record, kExtendedFlagHasRoutingInfo);
}

void PrintFndrExtendedDirInfo(out_ctx* ctx, const struct FndrExtendedDirInfo* record)
{
    PrintUI(ctx, record, document_id);
    PrintUI(ctx, record, date_added);
    PrintHFSTimestamp(ctx, record, date_added);
    PrintUIBinary(ctx, record, extended_flags);
    PrintExtendedFinderFlags(ctx, record->extended_flags);
    PrintUI(ctx, record, reserved3);
    PrintUI(ctx, record, write_gen_counter);
}

void PrintFndrExtendedFileInfo(out_ctx* ctx, const struct FndrExtendedFileInfo* record)
{
    PrintUI(ctx, record, document_id);
    PrintUI(ctx, record, date_added);
    PrintHFSTimestamp(ctx, record, date_added);
    PrintUIBinary(ctx, record, extended_flags);
    PrintExtendedFinderFlags(ctx, record->extended_flags);
    PrintUI(ctx, record, reserved2);
    PrintUI(ctx, record, write_gen_counter);
}

void PrintFndrOldExtendedDirInfo(out_ctx* ctx, const FndrOldExtendedDirInfo* record)
{
    PrintAttribute(ctx, "scrollPosition", "(%d, %d)", record->scrollPosition.v, record->scrollPosition.h);
    PrintUI(ctx, record, reserved1);
    PrintUIBinary(ctx, record, extended_flags);
    PrintExtendedFinderFlags(ctx, record->extended_flags);
    PrintUI(ctx, record, reserved2);
    PrintUI(ctx, record, putAwayFolderID);
}

void PrintFndrOldExtendedFileInfo(out_ctx* ctx, const FndrOldExtendedFileInfo* record)
{
    PrintInt(ctx, record, reserved1[0]);
    PrintInt(ctx, record, reserved1[1]);
    PrintInt(ctx, record, reserved1[2]);
    PrintInt(ctx, record, reserved1[3]);
    PrintUIBinary(ctx, record, extended_flags);
    PrintExtendedFinderFlags(ctx, record->extended_flags);
    PrintUI(ctx, record, reserved2);
    PrintUI(ctx, record, putAwayFolderID);
}

void PrintHotFilesInfo(out_ctx* ctx, const HotFilesInfo* record)
{
    BeginSection(ctx, "Hotfiles Info");
    PrintUIHex(ctx, record, magic);
    PrintUI(ctx, record, version);
    PrintAttribute(ctx, "duration", "%u seconds", record->duration);
    PrintUI(ctx, record, timebase);
    // Not really sure yet how this value is supposed to be interpreted (in other words, this is probably wrong):
    uint32_t adjTimebase = MAC_GMT_FACTOR + HFC_MIN_BASE_TIME + record->timebase;
    _PrintHFSTimestamp(ctx, "timebase", adjTimebase);
    PrintAttribute(ctx, "timeleft", "%u seconds", record->timeleft);
    PrintUI(ctx, record, threshold);
    PrintAttribute(ctx, "maxfileblcks", "%u blocks", record->maxfileblks);
    PrintUI(ctx, record, maxfilecnt);
    PrintAttribute(ctx, "tag", "%s", (const char *)record->tag);
    EndSection(ctx);
}

void PrintPartialHotFileKey(out_ctx* ctx, const HotFileKey* record)
{
    PrintUI             (ctx, record, keyLength);
    PrintAttribute      (ctx, "forkType", "%#x (%s)", record->forkType, (record->forkType == HFSResourceForkType ? "rsrc" : "data"));
    PrintUI             (ctx, record, pad);
    if (record->temperature == HFC_LOOKUPTAG) {
        PrintAttribute  (ctx, "temperature", "%#x (HFC_LOOKUPTAG)", record->temperature);
    } else {
        PrintUI         (ctx, record, temperature);
    }
}

void PrintHotFileThreadRecord(out_ctx* ctx, const HotFileThreadRecord* record)
{
    PrintPartialHotFileKey  (ctx, &record->key);
    PrintAttribute          (ctx, "data (temperature)", "%u", record->temperature);
    PrintAttribute          (ctx, "fileID", "%u", record->key.fileID);
    PrintPath               (ctx, "path", record->key.fileID);
}

void PrintHotFileRecord(out_ctx* ctx, const HotFileRecord* record)
{
    PrintPartialHotFileKey  (ctx, &record->key);
    _PrintUIChar            (ctx, "data", record->data, sizeof(record->data));
    PrintAttribute          (ctx, "fileID", "%u", record->key.fileID);
    PrintPath               (ctx, "path", record->key.fileID);
}

void PrintHFSPlusExtentKey(out_ctx* ctx, const HFSPlusExtentKey* record)
{
    PrintUI             (ctx, record, keyLength);
    PrintAttribute      (ctx, "forkType", "%#x (%s)", record->forkType, (record->forkType == HFSResourceForkType ? "rsrc" : "data"));
    PrintUI             (ctx, record, pad);
    PrintUI             (ctx, record, fileID);
    PrintUI             (ctx, record, startBlock);
    PrintPath           (ctx, "path", record->fileID);
}

void PrintHFSPlusAttrKey(out_ctx* ctx, const HFSPlusAttrKey* record)
{
    PrintUI(ctx, record, keyLength);
    PrintUI(ctx, record, pad);
    PrintUI(ctx, record, fileID);
    PrintUI(ctx, record, startBlock);
    PrintUI(ctx, record, attrNameLen);
    HFSPlusAttrStr127 attrName = HFSPlusAttrKeyGetStr(record);
    PrintHFSPlusAttrStr127(ctx, "attrName", &attrName);
}

#pragma mark - Catalog Records

void PrintHFSPlusCatalogFolder(out_ctx* ctx, const HFSPlusCatalogFolder* record)
{
    PrintAttribute(ctx, "recordType", "kHFSPlusFolderRecord");

    PrintUI(ctx, record, folderID);
    PrintPath(ctx, "path", record->folderID);
    
    PrintUIBinary(ctx, record, flags);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSFileLockedMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSThreadExistsMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasAttributesMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasSecurityMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasFolderCountMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasLinkChainMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasChildLinkMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasDateAddedMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSFastDevPinnedMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSDoNotFastDevPinMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSFastDevCandidateMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSAutoCandidateMask);

    PrintUI(ctx, record, valence);
    PrintHFSTimestamp(ctx, record, createDate);
    PrintHFSTimestamp(ctx, record, contentModDate);
    PrintHFSTimestamp(ctx, record, attributeModDate);
    PrintHFSTimestamp(ctx, record, accessDate);
    PrintHFSTimestamp(ctx, record, backupDate);

    PrintHFSPlusBSDInfo(ctx, &record->bsdInfo, (record->flags & kHFSHasLinkChainMask));

    BeginSection(ctx, "Finder Info");
    PrintFndrDirInfo(ctx, &record->userInfo);
    EndSection(ctx);

    BeginSection(ctx, "Extended Finder Info");
    if (record->flags & kHFSHasDateAddedMask) {
        PrintFndrExtendedDirInfo(ctx, (const struct FndrExtendedDirInfo*)&record->finderInfo);
    } else {
        PrintFndrOldExtendedDirInfo(ctx, (const FndrOldExtendedDirInfo*)&record->finderInfo);
    }
    EndSection(ctx);
    Print(ctx, "%s", "");

    PrintUI(ctx, record, textEncoding);
    if ((record->flags & kHFSHasFolderCountBit))
        PrintUI(ctx, record, folderCount);

    if ((record->flags & kHFSHasAttributesMask))
        PrintHFSPlusAttributes(ctx, record->folderID, volume_);
}

void PrintHFSPlusCatalogFile(out_ctx* ctx, const HFSPlusCatalogFile* record)
{
    PrintAttribute(ctx, "recordType", "kHFSPlusFileRecord");

    PrintUI                 (ctx, record, fileID);
    PrintPath               (ctx, "path", record->fileID);
    
    PrintUIBinary(ctx, record, flags);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSFileLockedMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSThreadExistsMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasAttributesMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasSecurityMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasFolderCountMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasLinkChainMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasChildLinkMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSHasDateAddedMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSFastDevPinnedMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSDoNotFastDevPinMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSFastDevCandidateMask);
    PrintUIFlagIfMatch(ctx, record->flags, kHFSAutoCandidateMask);

    PrintUI                 (ctx, record, reserved1);
    PrintHFSTimestamp       (ctx, record, createDate);
    PrintHFSTimestamp       (ctx, record, contentModDate);
    PrintHFSTimestamp       (ctx, record, attributeModDate);
    PrintHFSTimestamp       (ctx, record, accessDate);
    PrintHFSTimestamp       (ctx, record, backupDate);

    PrintHFSPlusBSDInfo     (ctx, &record->bsdInfo, (record->flags & kHFSHasLinkChainMask));

    BeginSection(ctx, "Finder Info");
    PrintFndrFileInfo       (ctx, &record->userInfo);
    EndSection(ctx);

    BeginSection(ctx, "Extended Finder Info");
    if (record->flags & kHFSHasDateAddedMask) {
        PrintFndrExtendedFileInfo(ctx, (const struct FndrExtendedFileInfo*)&record->finderInfo);
    } else {
        PrintFndrOldExtendedFileInfo(ctx, (const FndrOldExtendedFileInfo*)&record->finderInfo);
    }
    EndSection(ctx);
    Print(ctx, "%s", "");

    PrintUI                 (ctx, record, textEncoding);
    PrintUI                 (ctx, record, reserved2);

    BeginSection(ctx, "Data Fork");
    PrintHFSPlusForkData(ctx, &record->dataFork, record->fileID, HFSDataForkType);
    EndSection(ctx);

    if (record->resourceFork.logicalSize) {
        BeginSection(ctx, "Resource Fork");
        PrintHFSPlusForkData(ctx, &record->resourceFork, record->fileID, HFSResourceForkType);
        EndSection(ctx);
    }

    if ((record->flags & kHFSHasAttributesMask))
        PrintHFSPlusAttributes(ctx, record->fileID, volume_);
}

void PrintHFSPlusFolderThreadRecord(out_ctx* ctx, const HFSPlusCatalogThread* record)
{
    PrintAttribute       (ctx, "recordType", "kHFSPlusFolderThreadRecord");
    PrintHFSPlusCatalogThread   (ctx, record);
}

void PrintHFSPlusFileThreadRecord(out_ctx* ctx, const HFSPlusCatalogThread* record)
{
    PrintAttribute       (ctx, "recordType", "kHFSPlusFileThreadRecord");
    PrintHFSPlusCatalogThread   (ctx, record);
}

void PrintHFSPlusCatalogThread(out_ctx* ctx, const HFSPlusCatalogThread* record)
{
    PrintInt            (ctx, record, reserved);
    PrintUI             (ctx, record, parentID);
    PrintHFSUniStr255   (ctx, "nodeName", &record->nodeName);
}

#pragma mark - Attribute Records

void PrintHFSPlusAttrForkData(out_ctx* ctx, const HFSPlusAttrForkData* record)
{
    PrintHFSPlusForkData(ctx, &record->theFork, 0, HFSDataForkType);
}

void PrintHFSPlusAttrExtents(out_ctx* ctx, const HFSPlusAttrExtents* record)
{
    PrintHFSPlusExtentRecord(ctx, &record->extents);
}

void PrintHFSPlusAttrData(out_ctx* ctx, const HFSPlusAttrData* record)
{
    PrintDataLength(ctx, record, attrSize);
    PrintAttribute(ctx, "attrData", "");
    VisualizeData((char*)&record->attrData, record->attrSize);
}

void PrintHFSPlusAttrRecord(out_ctx* ctx, const HFSPlusAttrRecord* record)
{
    PrintLabeledConstHexIfEqual(ctx, record, recordType, kHFSPlusAttrInlineData);
    PrintLabeledConstHexIfEqual(ctx, record, recordType, kHFSPlusAttrForkData);
    PrintLabeledConstHexIfEqual(ctx, record, recordType, kHFSPlusAttrExtents);

    switch (record->recordType) {
        case kHFSPlusAttrInlineData:
        {
            BeginSection(ctx, "Inline Data");
            PrintHFSPlusAttrData(ctx, &record->attrData);
            EndSection(ctx);
            break;
        }

        case kHFSPlusAttrForkData:
        {
            BeginSection(ctx, "Fork Data");
            PrintHFSPlusAttrForkData(ctx, &record->forkData);
            EndSection(ctx);
            break;
        }

        case kHFSPlusAttrExtents:
        {
            BeginSection(ctx, "Overflow Extents Data");
            PrintHFSPlusAttrExtents(ctx, &record->overflowExtents);
            EndSection(ctx);
            break;
        }

        default:
        {
            error("unknown attribute record type: %d", record->recordType);
            VisualizeData((char*)record, sizeof(HFSPlusAttrRecord));
            break;
        }
    }
}

void PrintHFSPlusAttributes(out_ctx* ctx, uint32_t cnid, HFSPlus* hfs)
{
    XAttrList *list = xattrlist_make();

    int result = 0;
    if ( (result = hfsplus_attributes_get_xattrlist_for_cnid(list, cnid, hfs)) < 0) {
        xattrlist_free(list);
        return;
    }

    // Print file compression info if applicable
    XAttr* xattr = NULL;

    uint32_t xattrCount = 0;

    TAILQ_FOREACH(xattr, list, xattrs) {
        HFSPlusAttrStr127 ucName = HFSPlusAttrKeyGetStr(&xattr->key);
        hfs_attr_str attr_name = "";
        attruc_to_attrstr(&attr_name, &ucName);

        if (strcmp((const char*)attr_name, DECMPFS_XATTR_NAME) == 0) {
            HFSPlusAttrRecord* record = xattr->record;
            if (record->recordType != kHFSPlusAttrInlineData || record->attrData.attrSize < sizeof(decmpfs_disk_header)) {
                continue;
            }
            
            decmpfs_disk_header header = {0};
            decmpfs_disk_header* headerPtr = &header;
            memcpy(headerPtr, record->attrData.attrData, sizeof(decmpfs_disk_header));
            swap_decmpfs_disk_header(headerPtr);
            
            BeginSection            (ctx, "File Compression Info");
            PrintUIChar             (ctx, headerPtr, compression_magic);
            
            PrintLabeledConstIfEqual(ctx, headerPtr, compression_type, kAppleFSCompressionUncompressedType);
            PrintLabeledConstIfEqual(ctx, headerPtr, compression_type, kAppleFSCompressionZlibType3);
            PrintLabeledConstIfEqual(ctx, headerPtr, compression_type, kAppleFSCompressionZlibType4);
            PrintLabeledConstIfEqual(ctx, headerPtr, compression_type, kAppleFSCompressionDatalessType);
            PrintLabeledConstIfEqual(ctx, headerPtr, compression_type, kAppleFSCompressionLZVNType7);
            PrintLabeledConstIfEqual(ctx, headerPtr, compression_type, kAppleFSCompressionLZVNType8);
            PrintLabeledConstIfEqual(ctx, headerPtr, compression_type, kAppleFSCompressionLZVNType9);
            PrintLabeledConstIfEqual(ctx, headerPtr, compression_type, kAppleFSCompressionLZVNType10);

            _PrintDataLength        (ctx, "compressed_size", record->attrData.attrSize);
            PrintDataLength         (ctx, headerPtr, uncompressed_size);
            
            EndSection(ctx); // file compression info
        }
        xattrCount++;
    }

    if (xattrCount) {
        BeginSection(ctx, "Extended Attributes");
        PrintXAttrList(ctx, list);
        EndSection(ctx); // extended ettributes
    }

    xattrlist_free(list);
}

void PrintXAttrList(out_ctx* ctx, const XAttrList* list)
{
    XAttr* attribute = NULL;

    TAILQ_FOREACH(attribute, list, xattrs) {
//        PrintHFSPlusAttrKey(ctx, &attribute->key);
        VisualizeHFSPlusAttrKey(ctx, &attribute->key, "Attribute Key", false);
        PrintHFSPlusAttrRecord(ctx, attribute->record);
    }
}

#pragma mark - Extent Records

void PrintHFSPlusExtentRecord(out_ctx* ctx, const HFSPlusExtentRecord* record)
{
    ExtentList* list = extentlist_make();
    extentlist_add_record(list, *record);
    PrintExtentList(ctx, list, 0);
    extentlist_free(list);
}

#pragma mark - Structure Visualization Functions

void VisualizeHFSPlusExtentKey(out_ctx* ctx, const HFSPlusExtentKey* record, const char* label, bool oneLine)
{
    if (oneLine) {
        Print(ctx, "%s: %s:%-6u; %s:%-4u; %s:%-3u; %s:%-10u; %s:%-10u",
              label,
              "length",
              record->keyLength,
              "fork",
              record->forkType,
              "pad",
              record->pad,
              "fileID",
              record->fileID,
              "startBlock",
              record->startBlock
              );
    } else {
        Print(ctx, "%s", label);
        Print(ctx, "+-----------------------------------------------+");
        Print(ctx, "| %-6s | %-4s | %-3s | %-10s | %-10s |",
              "length",
              "fork",
              "pad",
              "fileID",
              "startBlock");
        Print(ctx, "| %-6u | %-4u | %-3u | %-10u | %-10u |",
              record->keyLength,
              record->forkType,
              record->pad,
              record->fileID,
              record->startBlock);
        Print(ctx, "+-----------------------------------------------+");
    }
}

void VisualizeHFSPlusCatalogKey(out_ctx* ctx, const HFSPlusCatalogKey* record, const char* label, bool oneLine)
{
    hfs_str name = "";
    hfsuc_to_str(&name, &record->nodeName);
    if (oneLine) {
        Print(ctx, "%s: %s:%-6u; %s:%-10u; %s:%-6u; %s:%-50s",
              label,
              "length",
              record->keyLength,
              "parentID",
              record->parentID,
              "length",
              record->nodeName.length,
              "nodeName",
              name
              );
    } else {
        char* names  = "| %-6s | %-10s | %-6s | %-50s |";
        char* format = "| %-6u | %-10u | %-6u | %-50s |";
        char* line_f =  "+%s+";
        char  dashes[90];
        memset(dashes, '-', 83);

        Print(ctx, "%s", label);
        Print(ctx, line_f, dashes);
        Print(ctx, names, "length", "parentID", "length", "nodeName");
        Print(ctx, format, record->keyLength, record->parentID, record->nodeName.length, name);
        Print(ctx, line_f, dashes);
    }
    printf("\n");
}

void VisualizeHFSPlusAttrKey(out_ctx* ctx, const HFSPlusAttrKey* record, const char* label, bool oneLine)
{
    HFSPlusAttrStr127 attrName = HFSPlusAttrKeyGetStr(record);
    hfs_attr_str name = "";
    attruc_to_attrstr(&name, &attrName);

    if (oneLine) {
        Print(ctx, "%s: %s = %-6u; %s = %-10u; %s = %-10u; %s = %-6u; %s = %-40s",
              label,
              "length",
              record->keyLength,
              "fileID",
              record->fileID,
              "startBlock",
              record->startBlock,
              "length",
              record->attrNameLen,
              "attrName",
              name
              );
    } else {
        char* names      = "| %-6s | %-10s | %-10s | %-6s | %-40s |";
        char* format     = "| %-6u | %-10u | %-10u | %-6u | %-40s |";

        char* line_f     =  "+%s+";
        char  dashes[87] = {'\0'};
        memset(dashes, '-', 86);
        dashes[86] = '\0';

        Print(ctx, "%s", label);
        Print(ctx, line_f, dashes);
        Print(ctx, names, "length", "fileID", "startBlock", "length", "attrName");
        Print(ctx, format, record->keyLength, record->fileID, record->startBlock, record->attrNameLen, name);
        Print(ctx, line_f, dashes);
    }
    printf("\n");
}

void VisualizeHFSBTreeNodeRecord(out_ctx* ctx, const char* label, BTHeaderRec headerRecord, const BTreeNodePtr node, BTRecNum recNum)
{
    BTNodeRecord record = {0};
    BTGetBTNodeRecord(&record, node, recNum);

    char*        names  = "| %-9s | %-6s | %-6s | %-10s | %-12s |";
    char*        format = "| %-9u | %-6u | %-6u | %-10u | %-12u |";

    char*        line_f =  "+%s+";
    char         dashes[60];
    memset(dashes, '-', 57);

    Print(ctx, "%s", label);
    Print(ctx, line_f, dashes);
    Print(ctx, names, "record_id", "offset", "length", "key_length", "value_length");
    Print(ctx, format, recNum, record.offset, record.recordLen, record.keyLen, record.valueLen);
    Print(ctx, line_f, dashes);
    Print(ctx, "Key data:");
    VisualizeData((char*)record.key, MIN(record.recordLen, record.keyLen));
    Print(ctx, "Value data:");
    VisualizeData((char*)record.value, MIN(record.recordLen, record.valueLen));
}

void VisualizeHotFileKey(out_ctx* ctx, const HotFileKey* record, const char* label, bool oneLine)
{
    char tempStr[15] = "";

    if (record->temperature == HFC_LOOKUPTAG) {
        strlcpy(tempStr, "HFC_LOOKUPTAG", 15);
    } else {
        sprintf(tempStr, "%u", record->temperature);
    }

    if (oneLine) {
        Print(ctx, "%s: %s:%-6u; %s:%-4u; %s:%-3u; %s:%-13s; %s:%-10u",
              label,
              "length",
              record->keyLength,
              "fork",
              record->forkType,
              "pad",
              record->pad,
              "temperature",
              tempStr,
              "fileID",
              record->fileID
              );
    } else {
        Print(ctx, "%s", label);
        Print(ctx, "+--------------------------------------------------+");
        Print(ctx, "| %-6s | %-4s | %-3s | %-13s | %-10s |",
              "length",
              "fork",
              "pad",
              "temperature",
              "fileID");
        Print(ctx, "| %-6u | %-4u | %-3u | %-13s | %-10u |",
              record->keyLength,
              record->forkType,
              record->pad,
              tempStr,
              record->fileID);
        Print(ctx, "+--------------------------------------------------+");
    }
}

#pragma mark - Node Print Functions

void PrintTreeNode(out_ctx* ctx, const BTreePtr tree, uint32_t nodeID)
{
    debug("PrintTreeNode");

    BTreeNodePtr node = NULL;
    if ( BTGetNode(&node, tree, nodeID) < 0) {
        error("node %u is invalid or out of range.", nodeID);
        return;
    }
    if (node == NULL) {
        Print(ctx, "(unused node)");
        return;
    }
    PrintNode(ctx, node);
    btree_free_node(node);
}

void PrintNode(out_ctx* ctx, const BTreeNodePtr node)
{
    debug("PrintNode");

    BeginSection(ctx, "Node %u (offset %lld; length: %zu bytes)", node->nodeNumber, node->nodeOffset, node->nodeSize);
    PrintBTNodeDescriptor(ctx, node->nodeDescriptor);

//    uint16_t *records = ((uint16_t*)(node->data + node->bTree->headerRecord.nodeSize) - node->nodeDescriptor->numRecords);
//    for (unsigned i = node->nodeDescriptor->numRecords-1; i >= 0; --i) {
//        char label[50] = "";
//        sprintf(label, "Record Offset #%u", node->nodeDescriptor->numRecords - i);
//        PrintAttribute(ctx, label, "%u", records[i]);
//    }

    for (unsigned recordNumber = 0; recordNumber < node->nodeDescriptor->numRecords; recordNumber++) {
        PrintNodeRecord(ctx, node, recordNumber);
    }
    EndSection(ctx);
}

void PrintFolderListing(out_ctx* ctx, uint32_t folderID)
{
    debug("Printing listing for folder ID %u", folderID);

    // CNID kind mode user group data rsrc name
    char         lineStr[110] = {0};
    memset(lineStr, '-', 100);
    char*        headerFormat = "%-9s %-10s %-10s %-9s %-9s %-15s %-15s %s";
    char*        rowFormat    = "%-9u %-10s %-10s %-9d %-9d %-15s %-15s %s";

    // Search for thread record
    FSSpec       spec         = { .parentID = folderID };
    BTreeNodePtr node         = NULL;
    BTRecNum     recordID     = 0;

    if (hfsplus_catalog_find_record(&node, &recordID, spec) < 0) {
        error("No thread record for %d found.", folderID);
        return;
    }

    debug("Found thread record %d:%d", node->nodeNumber, recordID);

    // Get thread record
    HFSPlusCatalogKey*    recordKey     = NULL;
    HFSPlusCatalogRecord* catalogRecord = NULL;
    btree_get_record((void*)&recordKey, (void*)&catalogRecord, node, recordID);

    uint32_t              threadID      = recordKey->parentID;
    hfs_str               name          = "";
    hfsuc_to_str(&name, &catalogRecord->catalogThread.nodeName);

    __attribute__((aligned(8))) struct {
        uint64_t fileCount;
        uint64_t folderCount;
        uint64_t hardLinkCount;
        uint64_t symlinkCount;
        uint64_t dataForkCount;
        uint64_t dataForkSize;
        uint64_t rsrcForkCount;
        uint64_t rsrcForkSize;
    } folderStats = {0};

    // Start output
    BeginSection(ctx, "Listing for %s", name);
    Print(ctx, headerFormat, "CNID", "kind", "mode", "user", "group", "data", "rsrc", "name");

    // Loop over siblings until NULL
    while (1) {
        // Locate next record, even if it's in the next node.
        if (++recordID >= node->recordCount) {
            debug("Done with records in node %d", node->nodeNumber);
            bt_nodeid_t nextNode = node->nodeDescriptor->fLink;
            BTreePtr    tree     = node->bTree;
            btree_free_node(node);
            if ( BTGetNode(&node, tree, nextNode) < 0) { error("couldn't get the next catalog node"); return; }
            recordID = 0;
        }

        btree_get_record((void*)&recordKey, (void*)&catalogRecord, node, recordID);

        if (threadID == recordKey->parentID) {
            debug("Looking at record %d", recordID);

            if ((catalogRecord->record_type == kHFSPlusFileRecord) || (catalogRecord->record_type == kHFSPlusFolderRecord)) {
                uint32_t cnid         = 0;
                char     kind[10];
                char     mode[11];
                int      user         = -1, group = -1;
                char     dataSize[20] = {'-', '\0'};
                char     rsrcSize[20] = {'-', '\0'};

                hfsuc_to_str(&name, &recordKey->nodeName);

                if ( HFSPlusCatalogFolderIsHardLink(catalogRecord)) {
                    folderStats.hardLinkCount++;
                    (void)strlcpy(kind, "dir link", 10);
                } else if (HFSPlusCatalogFileIsHardLink(catalogRecord)) {
                    folderStats.hardLinkCount++;
                    (void)strlcpy(kind, "hard link", 10);
                } else if (HFSPlusCatalogRecordIsSymLink(catalogRecord)) {
                    folderStats.symlinkCount++;
                    (void)strlcpy(kind, "symlink", 10);
                } else if (catalogRecord->record_type == kHFSPlusFolderRecord) {
                    folderStats.folderCount++;
                    (void)strlcpy(kind, "folder", 10);
                } else {
                    folderStats.fileCount++;
                    (void)strlcpy(kind, "file", 10);
                }

                // Files and folders share these attributes at the same locations.
                cnid  = catalogRecord->catalogFile.fileID;
                user  = catalogRecord->catalogFile.bsdInfo.ownerID;
                group = catalogRecord->catalogFile.bsdInfo.groupID;
                _genModeString(mode, catalogRecord->catalogFile.bsdInfo.fileMode);

                if (catalogRecord->record_type == kHFSPlusFileRecord) {
                    if (catalogRecord->catalogFile.dataFork.logicalSize)
                        (void)format_size(ctx, dataSize, catalogRecord->catalogFile.dataFork.logicalSize, 50);

                    if (catalogRecord->catalogFile.resourceFork.logicalSize)
                        (void)format_size(ctx, rsrcSize, catalogRecord->catalogFile.resourceFork.logicalSize, 50);

                    if (catalogRecord->catalogFile.dataFork.totalBlocks > 0) {
                        folderStats.dataForkCount++;
                        folderStats.dataForkSize += catalogRecord->catalogFile.dataFork.logicalSize;
                    }

                    if (catalogRecord->catalogFile.resourceFork.totalBlocks > 0) {
                        folderStats.rsrcForkCount++;
                        folderStats.rsrcForkSize += catalogRecord->catalogFile.resourceFork.logicalSize;
                    }
                }

                Print(ctx, rowFormat, cnid, kind, mode, user, group, dataSize, rsrcSize, name);
            }
        } else {
            break;
        } // parentID == parentID
    }     // while(1)

    char dataTotal[50];
    char rsrcTotal[50];

    format_size(ctx, dataTotal, folderStats.dataForkSize, 50);
    format_size(ctx, rsrcTotal, folderStats.rsrcForkSize, 50);

    Print(ctx, "%s", lineStr);
    Print(ctx, headerFormat, "", "", "", "", "", dataTotal, rsrcTotal, "");

    Print(ctx, "   Folders: %-10llu Data Forks: %-10llu Hard Links: %-10llu",
          folderStats.folderCount,
          folderStats.dataForkCount,
          folderStats.hardLinkCount
          );

    Print(ctx, "     Files: %-10llu RSRC Forks: %-10llu   Symlinks: %-10llu",
          folderStats.fileCount,
          folderStats.rsrcForkCount,
          folderStats.symlinkCount
          );

    btree_free_node(node);

    EndSection(ctx);

    debug("Done listing.");
}

void PrintNodeRecord(out_ctx* ctx, const BTreeNodePtr node, int recordNumber)
{
    debug("PrintNodeRecord");

    if(recordNumber >= node->nodeDescriptor->numRecords) return;

    BTNodeRecord    _record = {0};
    BTNodeRecordPtr record  = &_record;
    BTGetBTNodeRecord(record, node, recordNumber);

    if (record->recordLen == 0) {
        error("Invalid record: no data found.");
        return;
    }

    bt_nodeid_t hotfilesTreeID = hfs_hotfiles_get_tree_id();

    // Index nodes (done)
    if (node->nodeDescriptor->kind == kBTIndexNode) {
        if (node->bTree->treeID == kHFSExtentsFileID) {
            if (recordNumber == 0) {
                BeginSection(ctx, "Extent Tree Index Records");
                Print(ctx, "%-3s %-12s %-12s %-4s   %-12s %-12s", "#", "Node ID", "Key Length", "Fork", "File ID", "Start Block");
            }
            bt_nodeid_t       next_node = *(bt_nodeid_t*)record->value;
            HFSPlusExtentKey* key       = (HFSPlusExtentKey*)record->key;
            Print(ctx, "%-3u %-12u %-12u 0x%02x   %-12u %-12u", recordNumber, next_node, key->keyLength, key->forkType, key->fileID, key->startBlock);
            return;

        } else if (node->bTree->treeID == kHFSCatalogFileID) {
            if (recordNumber == 0) {
                BeginSection(ctx, "Catalog Tree Index Records");
                Print(ctx, "%-3s %-12s %-5s %-12s %s", "#", "nodeID", "kLen", "parentID", "nodeName");
            }
            bt_nodeid_t        next_node = *(bt_nodeid_t*)record->value;
            HFSPlusCatalogKey* key       = (HFSPlusCatalogKey*)record->key;
            hfs_str            nodeName  = "";
            hfsuc_to_str(&nodeName, &key->nodeName);
            Print(ctx, "%-3u %-12u %-5u %-12u %s", recordNumber, next_node, key->keyLength, key->parentID, nodeName);
            return;

        } else if (node->bTree->treeID == kHFSAttributesFileID) {
            if (recordNumber == 0) {
                BeginSection(ctx, "Attribute Tree Index Records");
                Print(ctx, "%-3s %-12s %-5s %-12s %s", "#", "nodeID", "kLen", "fileID", "attrName");
            }
            bt_nodeid_t     next_node = *(bt_nodeid_t*)record->value;
            HFSPlusAttrKey* key       = (HFSPlusAttrKey*)record->key;

            HFSPlusAttrStr127 uniStr = HFSPlusAttrKeyGetStr(key);
            hfs_attr_str attrName = "";
            attruc_to_attrstr(&attrName, &uniStr);

            Print(ctx, "%-3u %-12u %-5u %-12u %s", recordNumber, next_node, key->keyLength, key->fileID, attrName);

            return;
        } else if (hotfilesTreeID && node->bTree->treeID == hotfilesTreeID) {
            if (recordNumber == 0) {
                BeginSection(ctx, "Hotfiles B-Tree Index Records");
                Print(ctx, "%-3s %-12s %-12s %13s", "#", "nodeID", "fileID", "temperature");
            }
            bt_nodeid_t     next_node = *(bt_nodeid_t*)record->value;
            HotFileKey*     key       = (HotFileKey*)record->key;

            Print(ctx, "%-3u %-12u %-12u %13u", recordNumber, next_node, key->fileID, key->temperature);
            return;
        }
    }

    BeginSection(ctx, "Record ID %u (%u/%u) (length: %u bytes) (Node %u)",
                 recordNumber,
                 recordNumber + 1,
                 node->nodeDescriptor->numRecords,
                 record->recordLen,
                 node->nodeNumber
                 );

    switch (node->nodeDescriptor->kind) {
        case kBTHeaderNode:
        {
            switch (recordNumber) {
                case 0:
                {
                    PrintAttribute(ctx, "recordType", "BTHeaderRec");
                    BTHeaderRec header = *( (BTHeaderRec*) record->record );
                    PrintBTHeaderRecord(ctx, &header);
                    break;
                }

                case 1:
                {
                    PrintAttribute(ctx, "recordType", "userData (reserved)");
                    /*
                     Currently, only the Hotfiles B-Tree uses the userData record to store data.
                     */

                    if (hotfilesTreeID && node->treeID == hotfilesTreeID) {
                        HotFilesInfo hotfileInfo = *( (HotFilesInfo*) record->record);
                        swap_HotFilesInfo(&hotfileInfo);
                        PrintHotFilesInfo(ctx, &hotfileInfo);
                        break;
                    } else {
                        VisualizeData(record->record, record->recordLen);
                        break;
                    }
                }

                case 2:
                {
                    PrintAttribute(ctx, "recordType", "mapData");
                    VisualizeData(record->record, record->recordLen);
                    break;
                }
            }
            break;
        }

        case kBTMapNode:
        {
            PrintAttribute(ctx, "recordType", "mapData");
            VisualizeData(record->record, record->recordLen);
            break;
        }

        default:
        {
            bt_nodeid_t treeID = node->bTree->treeID;

            // Handle all keyed node records
            // first print the key
            switch (treeID) {
                case kHFSCatalogFileID:
                {
                    HFSPlusCatalogKey* keyStruct = (HFSPlusCatalogKey*) record->key;
                    VisualizeHFSPlusCatalogKey(ctx, keyStruct, "Catalog Key", 0);

                    if ((record->keyLen < kHFSPlusCatalogKeyMinimumLength) || (record->keyLen > kHFSPlusCatalogKeyMaximumLength))
                        goto INVALID_KEY;

                    break;
                };

                case kHFSExtentsFileID:
                {
                    HFSPlusExtentKey keyStruct = *( (HFSPlusExtentKey*) record->key );
                    VisualizeHFSPlusExtentKey(ctx, &keyStruct, "Extent Key", 0);

                    if ( (record->keyLen - sizeof(keyStruct.keyLength)) != kHFSPlusExtentKeyMaximumLength)
                        goto INVALID_KEY;

                    break;
                };

                case kHFSAttributesFileID:
                {
                    HFSPlusAttrKey* keyStruct = (HFSPlusAttrKey*) record->key;
                    VisualizeHFSPlusAttrKey(ctx, keyStruct, "Attributes Key", 0);

                    if ((record->keyLen < kHFSPlusAttrKeyMinimumLength) || (record->keyLen > kHFSPlusAttrKeyMaximumLength))
                        goto INVALID_KEY;

                    break;
                }

                default:
                {
                    if (treeID == hotfilesTreeID) {
                        HotFileKey* keyStruct = (HotFileKey*) record->key;
                        VisualizeHotFileKey(ctx, keyStruct, "Hotfile Key", 0);
                        
                        if (keyStruct->keyLength != HFC_KEYLENGTH)
                            goto INVALID_KEY;

                        break;
                    } else {
                        goto INVALID_KEY;

                    }
                }
            }
        
            // second, print the record (value)

            switch (node->nodeDescriptor->kind) {
                case kBTIndexNode:
                {
                    uint32_t* pointer = (uint32_t*) record->value;
                    PrintAttribute(ctx, "nextNodeID", "%u", *pointer);
                    break;
                }

                case kBTLeafNode:
                {
                    switch (treeID) {
                        case kHFSCatalogFileID:
                        {
                            uint16_t type = ((HFSPlusCatalogRecord*)record->value)->record_type;

                            switch (type) {
                                case kHFSPlusFileRecord:
                                {
                                    PrintHFSPlusCatalogFile(ctx, (HFSPlusCatalogFile*)record->value);
                                    break;
                                }

                                case kHFSPlusFolderRecord:
                                {
                                    PrintHFSPlusCatalogFolder(ctx, (HFSPlusCatalogFolder*)record->value);
                                    break;
                                }

                                case kHFSPlusFileThreadRecord:
                                {
                                    PrintHFSPlusFileThreadRecord(ctx, (HFSPlusCatalogThread*)record->value);
                                    break;
                                }

                                case kHFSPlusFolderThreadRecord:
                                {
                                    PrintHFSPlusFolderThreadRecord(ctx, (HFSPlusCatalogThread*)record->value);
                                    break;
                                }

                                default:
                                {
                                    PrintAttribute(ctx, "recordType", "%u (invalid)", type);
                                    VisualizeData(record->value, record->valueLen);
                                    break;
                                }
                            }
                            break;
                        }

                        case kHFSExtentsFileID:
                        {
                            HFSPlusExtentRecord* extentRecord = (HFSPlusExtentRecord*)record->value;
                            ExtentList*          list         = extentlist_make();
                            extentlist_add_record(list, *extentRecord);
                            PrintExtentList(ctx, list, 0);
                            extentlist_free(list);
                            break;
                        }

                        case kHFSAttributesFileID:
                        {
                            PrintHFSPlusAttrRecord(ctx, (HFSPlusAttrRecord*)record->value);
                        }
                            
                        default:
                        {
                            if (treeID == hotfilesTreeID) {
                                HotFileKey* keyStruct = (HotFileKey*)record->key;
                                if (keyStruct->temperature == HFC_LOOKUPTAG) {
                                    // it's a thread record
                                    BeginSection(ctx, "Hotfiles Thread Record");
                                    PrintHotFileThreadRecord(ctx, (const HotFileThreadRecord*)record->record);
                                    EndSection(ctx);
                                } else {
                                    BeginSection(ctx, "Hotfiles File Record");
                                    PrintHotFileRecord(ctx, (const HotFileRecord*)record->record);
                                    EndSection(ctx);
                                }
                            }
                        }
                    }

                    break;
                }

                default:
                {
INVALID_KEY:
                    if ((recordNumber + 1) == node->nodeDescriptor->numRecords) {
                        PrintAttribute(ctx, "recordType", "(free space)");
                    } else {
                        PrintAttribute(ctx, "recordType", "(unknown b-tree/record format)");
                        VisualizeData(record->record, record->recordLen);
                    }
                }
            }

            break;
        }
    }

    EndSection(ctx);
}

#pragma mark -

int format_hfs_timestamp(out_ctx* ctx, char* out, uint32_t timestamp, size_t length)
{
    if (timestamp > MAC_GMT_FACTOR) {
        timestamp -= MAC_GMT_FACTOR;
    } else {
        timestamp = 0; // Cannot be negative.
    }

    return format_time(ctx, out, timestamp, length);
}

