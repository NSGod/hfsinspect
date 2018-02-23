//
//  hfs_endian.c
//  hfsinspect
//
//  Created by Adam Knight on 5/5/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include "hfs/hfs_endian.h"

#include "volumes/output.h"
#include "hfs/types.h"
#include "hfs/btree/btree_endian.h"
#include "hfs/catalog.h"
#include "hfsplus/hotfiles.h"
#include "logging/logging.h"    // console printing routines

void swap_HFSExtentDescriptor(HFSExtentDescriptor* record)
{
    // trace("record (%p)", record);
    Swap(record->startBlock);
    Swap(record->blockCount);
}

void swap_HFSExtentRecord(HFSExtentRecord* record)
{
    // trace("record (%p)", record);
    for(unsigned i = 0; i < kHFSExtentDensity; i++)
        swap_HFSExtentDescriptor(record[i]);
}

void swap_HFSMasterDirectoryBlock(HFSMasterDirectoryBlock* record)
{
    // trace("record (%p)", record);
    Swap16(record->drSigWord);                          /* == kHFSSigWord */
    Swap32(record->drCrDate);                           /* date and time of volume creation */
    Swap32(record->drLsMod);                            /* date and time of last modification */
    Swap16(record->drAtrb);                             /* volume attributes */
    Swap16(record->drNmFls);                            /* number of files in root folder */
    Swap16(record->drVBMSt);                            /* first block of volume bitmap */
    Swap16(record->drAllocPtr);                         /* start of next allocation search */
    Swap16(record->drNmAlBlks);                         /* number of allocation blocks in volume */
    Swap32(record->drAlBlkSiz);                         /* size (in bytes) of allocation blocks */
    Swap32(record->drClpSiz);                           /* default clump size */
    Swap16(record->drAlBlSt);                           /* first allocation block in volume */
    Swap32(record->drNxtCNID);                          /* next unused catalog node ID */
    Swap16(record->drFreeBks);                          /* number of unused allocation blocks */
    // noswap: record->drVN
    Swap32(record->drVolBkUp);                          /* date and time of last backup */
    Swap16(record->drVSeqNum);                          /* volume backup sequence number */
    Swap32(record->drWrCnt);                            /* volume write count */
    Swap32(record->drXTClpSiz);                         /* clump size for extents overflow file */
    Swap32(record->drCTClpSiz);                         /* clump size for catalog file */
    Swap16(record->drNmRtDirs);                         /* number of directories in root folder */
    Swap32(record->drFilCnt);                           /* number of files in volume */
    Swap32(record->drDirCnt);                           /* number of directories in volume */
    // noswap: record->drFndrInfo[8]; /* info used by the Finder */
    Swap16(record->drEmbedSigWord);                     /* embedded vol sig (née drVCSize) */
    swap_HFSExtentDescriptor(&record->drEmbedExtent); /* embedded volume location and size (formerly drVBMCSize and drCtlCSize) */
    Swap32(record->drXTFlSize);                         /* size of extents overflow file */
    swap_HFSExtentRecord(&record->drXTExtRec);        /* extent record for extents overflow file */
    Swap32(record->drCTFlSize);                         /* size of catalog file */
    swap_HFSExtentRecord(&record->drCTExtRec);        /* extent record for catalog file */
}

void swap_HFSPlusVolumeHeader(HFSPlusVolumeHeader* record)
{
    // trace("record (%p)", record);
    Swap16(record->signature);
    Swap16(record->version);
    Swap32(record->attributes);
    Swap32(record->lastMountedVersion);
    Swap32(record->journalInfoBlock);

    Swap32(record->createDate);
    Swap32(record->modifyDate);
    Swap32(record->backupDate);
    Swap32(record->checkedDate);

    Swap32(record->fileCount);
    Swap32(record->folderCount);

    Swap32(record->blockSize);
    Swap32(record->totalBlocks);
    Swap32(record->freeBlocks);

    Swap32(record->nextAllocation);
    Swap32(record->rsrcClumpSize);
    Swap32(record->dataClumpSize);
    Swap32(record->nextCatalogID);

    Swap32(record->writeCount);
    Swap64(record->encodingsBitmap);

    HFSPlusVolumeFinderInfo* finderInfo = (void*)&record->finderInfo;
    Swap32(finderInfo->bootDirID);
    Swap32(finderInfo->bootParentID);
    Swap32(finderInfo->openWindowDirID);
    Swap32(finderInfo->os9DirID);
    // noswap: reserved is reserved (uint32)
    Swap32(finderInfo->osXDirID);
    Swap64(finderInfo->volID);

    swap_HFSPlusForkData(&record->allocationFile);
    swap_HFSPlusForkData(&record->extentsFile);
    swap_HFSPlusForkData(&record->catalogFile);
    swap_HFSPlusForkData(&record->attributesFile);
    swap_HFSPlusForkData(&record->startupFile);
}

void swap_JournalInfoBlock(JournalInfoBlock* record)
{
    // trace("record (%p)", record);

    /*
       struct JournalInfoBlock {
       uint32_t	flags;
       uint32_t       device_signature[8];  // signature used to locate our device.
       uint64_t       offset;               // byte offset to the journal on the device
       uint64_t       size;                 // size in bytes of the journal
       uuid_string_t   ext_jnl_uuid;
       char            machine_serial_num[48];
       char     reserved[JIB_RESERVED_SIZE];
       } __attribute__((aligned(2), packed));
       typedef struct JournalInfoBlock JournalInfoBlock;
     */
    Swap32(record->flags);
    for(unsigned i = 0; i < 8; i++)
        Swap32(record->device_signature[i]);
    Swap64(record->offset);
    Swap64(record->size);
    // noswap: uuid_string_t is a series of char
    // noswap: machine_serial_num is a series of char
    // noswap: reserved is reserved
}

void swap_journal_header(journal_header* record)
{
    if (record->endian != ENDIAN_MAGIC) {
        Swap32(record->magic);
        Swap32(record->endian);
        Swap64(record->start);
        Swap64(record->end);
        Swap64(record->size);
        Swap32(record->blhdr_size);
        Swap32(record->checksum);
        Swap32(record->jhdr_size);
        Swap32(record->sequence_num);
    }
}

void swap_HFSPlusForkData(HFSPlusForkData* record)
{
    // trace("record (%p)", record);
    Swap64(record->logicalSize);
    Swap32(record->totalBlocks);
    Swap32(record->clumpSize);
    swap_HFSPlusExtentRecord(record->extents);
}

void swap_HFSPlusExtentRecord(HFSPlusExtentDescriptor record[])
{
    // trace("record (%p)", record);
    for(unsigned i = 0; i < kHFSPlusExtentDensity; i++)
        swap_HFSPlusExtentDescriptor(&record[i]);
}

void swap_HFSPlusExtentDescriptor(HFSPlusExtentDescriptor* record)
{
    // trace("record (%p)", record);
    Swap32(record->startBlock);
    Swap32(record->blockCount);
}

void swap_HFSPlusExtentKey(HFSPlusExtentKey* record)
{
    // trace("record (%p)", record);
//    noswap: keyLength; swapped in swap_BTreeNode
    Swap32(record->fileID);
    Swap32(record->startBlock);
}

void swap_HFSUniStr255(HFSUniStr255* unistr)
{
    // trace("unistr (%p)", unistr);
    Swap16(unistr->length);
    if (unistr->length > 256) {
        warning("Invalid HFSUniStr255 length: %d", unistr->length);
    }
    for(unsigned i = 0; i < MIN(256, unistr->length); i++)
        Swap16(unistr->unicode[i]);
}

void swap_HFSPlusBSDInfo(HFSPlusBSDInfo* record)
{
    // trace("record (%p)", record);

    Swap32(record->ownerID);
    Swap32(record->groupID);
    // noswap: adminFlags is a byte
    // noswap: ownerFlags is a byte
    Swap16(record->fileMode);
    Swap32(record->special.iNodeNum);
}

void swap_FndrDirInfo(FndrDirInfo* record)
{
    // trace("record (%p)", record);

    Swap16(record->frRect.top);
    Swap16(record->frRect.left);
    Swap16(record->frRect.bottom);
    Swap16(record->frRect.right);
    Swap16(record->frFlags);    // frFlags is documented, it's just like fdFlags
    Swap16(record->frLocation.v);
    Swap16(record->frLocation.h);
    Swap16(record->opaque);
}

void swap_FndrFileInfo(FndrFileInfo* record)
{
    // trace("record (%p)", record);

    Swap32(record->fdType);
    Swap32(record->fdCreator);
    Swap16(record->fdFlags);
    Swap16(record->fdLocation.v);
    Swap16(record->fdLocation.h);
    Swap16(record->opaque);
}

void swap_FndrOpaqueInfo(FndrOpaqueInfo* record)
{
    // trace("record (%p)", record);

    // A bunch of undocumented bytes.  Included for completeness.
}

void swap_HFSPlusCatalogKey(HFSPlusCatalogKey* record)
{
    // trace("record (%p)", record);

//    noswap: keyLength; swapped in swap_BTreeNode
    Swap32(record->parentID);
    swap_HFSUniStr255(&record->nodeName);
}

void swap_HFSPlusCatalogRecord(HFSPlusCatalogRecord* record)
{
    // trace("record (%p)", record);

    Swap16(record->record_type);
    switch (record->record_type) {
        case kHFSPlusFileRecord:
        {
            swap_HFSPlusCatalogFile(&record->catalogFile);
            break;
        }

        case kHFSPlusFolderRecord:
        {
            swap_HFSPlusCatalogFolder(&record->catalogFolder);
            break;
        }

        case kHFSPlusFileThreadRecord:
        case kHFSPlusFolderThreadRecord:
        {
            swap_HFSPlusCatalogThread(&record->catalogThread);
            break;
        }

        default:
        {
            break;
        }
    }
}

void swap_HFSPlusCatalogFile(HFSPlusCatalogFile* record)
{
    // trace("record (%p)", record);

//    Swap16(record->recordType);
    Swap16(record->flags);
    Swap32(record->reserved1);
    Swap32(record->fileID);
    Swap32(record->createDate);
    Swap32(record->contentModDate);
    Swap32(record->attributeModDate);
    Swap32(record->accessDate);
    Swap32(record->backupDate);
    swap_HFSPlusBSDInfo(&record->bsdInfo);
    swap_FndrFileInfo(&record->userInfo);
    swap_FndrOpaqueInfo(&record->finderInfo);
    Swap32(record->textEncoding);
    Swap32(record->reserved2);

    swap_HFSPlusForkData(&record->dataFork);
    swap_HFSPlusForkData(&record->resourceFork);
}

void swap_HFSPlusCatalogFolder(HFSPlusCatalogFolder* record)
{
    // trace("record (%p)", record);

//    Swap16(record->recordType);
    Swap16(record->flags);
    Swap32(record->valence);
    Swap32(record->folderID);
    Swap32(record->createDate);
    Swap32(record->contentModDate);
    Swap32(record->attributeModDate);
    Swap32(record->accessDate);
    Swap32(record->backupDate);
    swap_HFSPlusBSDInfo(&record->bsdInfo);
    swap_FndrDirInfo(&record->userInfo);
    swap_FndrOpaqueInfo(&record->finderInfo);
    Swap32(record->textEncoding);
    Swap32(record->folderCount);
}

void swap_HFSPlusCatalogThread(HFSPlusCatalogThread* record)
{
    // trace("record (%p)", record);

//    Swap16(record->recordType);
    Swap32(record->reserved);
    Swap32(record->parentID);
    swap_HFSUniStr255(&record->nodeName);
}

void swap_HFSPlusAttrKey(HFSPlusAttrKey* record)
{
    // trace("record (%p)", record);
//    noswap: keyLength; swapped in swap_BTreeNode
    Swap16(record->pad);
    Swap32(record->fileID);
    Swap32(record->startBlock);
    Swap16(record->attrNameLen);
    for(unsigned i = 0; i < record->attrNameLen; i++) Swap16(record->attrName[i]);
}

void swap_HFSPlusAttrData(HFSPlusAttrData* record)
{
    // trace("record (%p)", record);
    // noswap: Swap32(record->recordType); (previously swapped)
    // noswap: Swap32(record->reserved[0]);
    // noswap: Swap32(record->reserved[1]);
    Swap32(record->attrSize);
}

void swap_HFSPlusAttrForkData(HFSPlusAttrForkData* record)
{
    // trace("record (%p)", record);
    // noswap: Swap32(record->recordType); (previously swapped)
    // noswap: Swap32(record->reserved);
    swap_HFSPlusForkData(&record->theFork);
}

void swap_HFSPlusAttrExtents(HFSPlusAttrExtents* record)
{
    // trace("record (%p)", record);
    // noswap: Swap32(record->recordType); (previously swapped)
    // noswap: Swap32(record->reserved);
    swap_HFSPlusExtentRecord(record->extents);
}

void swap_HFSPlusAttrRecord(HFSPlusAttrRecord* record)
{
    // trace("record (%p)", record);
    record->recordType = be32toh(record->recordType);
    switch (record->recordType) {
        case kHFSPlusAttrInlineData:
        {
            // InlineData is no more; use AttrData.
            swap_HFSPlusAttrData(&record->attrData);
            break;
        }

        case kHFSPlusAttrForkData:
        {
            swap_HFSPlusAttrForkData(&record->forkData);
            break;
        }

        case kHFSPlusAttrExtents:
        {
            swap_HFSPlusAttrExtents(&record->overflowExtents);
            break;
        }

        default:
        {
            critical("Unknown attribute record type: %d", record->recordType);
            break;
        }
    }
}

void swap_HotFilesInfo(HotFilesInfo* record)
{
    Swap32(record->magic);
    Swap32(record->version);
    Swap32(record->duration);
    Swap32(record->timebase);
    Swap32(record->timeleft);
    Swap32(record->threshold);
    Swap32(record->maxfileblks);
    Swap32(record->maxfilecnt);
    // noswap: uint8_t tag[32]; 
}

void swap_HotFileKey(HotFileKey* record)
{
    // noswap: keyLength; swapped in swap_BTreeNode
    // noswap: forkType is a byte
    // noswap: pad is a byte
    Swap32(record->temperature);
    Swap32(record->fileID);
}

