//
//  hfs.c
//  hfsinspect
//
//  Created by Adam Knight on 5/5/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include "hfs.h"

#pragma mark Volume Abstractions

int hfs_load_mbd(Volume *vol, HFSMasterDirectoryBlock *mdb)
{
    if ( vol_read(vol, (Bytes)mdb, sizeof(HFSMasterDirectoryBlock), 1024) < 0)
        return -1;
    
    swap_HFSMasterDirectoryBlock(mdb);
    
    return 0;
}

int hfs_load_header(Volume *vol, HFSPlusVolumeHeader *vh)
{
    if ( vol_read(vol, (Bytes)vh, sizeof(HFSPlusVolumeHeader), 1024) < 0)
        return -1;
    
    swap_HFSPlusVolumeHeader(vh);
    
    return 0;
}

int hfs_attach(HFS* hfs, Volume *vol)
{
    if (hfs == NULL || vol == NULL) { errno = EINVAL; return -1; }
    
    int type;
    int result;
    
    // Test to see if we support the volume.
    result = hfs_test(vol);
    if (result < 0) return -1;
    
    type = (unsigned)result;
    
    if (type != kFSTypeHFSPlus && type != kFSTypeWrappedHFSPlus && type != kFSTypeHFSX) {
        errno = EINVAL;
        return -1;
    }
    
    // Clear the HFSVolume struct (hope you didn't need that)
    ZERO_STRUCT(*hfs);
    
    // Handle wrapped volumes.
    if (type == kFSTypeWrappedHFSPlus) {
        HFSMasterDirectoryBlock mdb; ZERO_STRUCT(mdb);
        if ( hfs_load_mbd(vol, &mdb) < 0)
            return -1;
        
        hfs->offset = (mdb.drAlBlSt * 512) + (mdb.drEmbedExtent.startBlock * mdb.drAlBlkSiz);
    }
    
    // Load the volume header.
    if ( hfs_load_header(vol, &hfs->vh) < 0 )
        return -1;
    
    // Update the HFSVolume struct.
    hfs->vol = vol;
    hfs->block_size = hfs->vh.blockSize;
    hfs->block_count = hfs->vh.totalBlocks;
    
    hfs->offset += vol->offset;
    hfs->length = (vol->length ? vol->length : hfs->block_size * hfs->block_count);
    
    return 0;
}

/**
 Tests to see if a volume is HFS or not.
 @return Returns -1 on failure or a VolumeSubtype constant representing the detected filesystem.
 */
int hfs_test(Volume *vol)
{
    int type = kTypeUnknown;
    
    // First, test for HFS or wrapped HFS+ volumes.
    HFSMasterDirectoryBlock *mdb = calloc(1, 512);
    
    if ( hfs_load_mbd(vol, mdb) < 0) {
        FREE(mdb);
        return -1;
    }

    if (mdb->drSigWord == kHFSSigWord && mdb->drEmbedSigWord == kHFSPlusSigWord) {
        info("Found a wrapped HFS+ volume");
        type = kFSTypeWrappedHFSPlus;
        
    } else if (mdb->drSigWord == kHFSSigWord) {
        info("Found an HFS volume");
        type = kFSTypeHFS;
    }
    
    FREE(mdb);
    
    if (type != kTypeUnknown)
        return type;
    
    // Now test for a modern HFS+ volume.
    HFSPlusVolumeHeader vh;
    
    if ( hfs_load_header(vol, &vh) < 0 )
        return -1;
    
    if (vh.signature == kHFSPlusSigWord || vh.signature == kHFSXSigWord) {
        info("Found an HFS+ volume");
        return kFSTypeHFSPlus;
    }
    
    info("Unknown volume type");
    
    return type;
}

/** returns the first HFS+ volume in a tree of volumes */
Volume* hfs_find(Volume* vol)
{
    Volume *result = NULL;
    int test = hfs_test(vol);

    if ( (test) & (kFSTypeHFSPlus | kFSTypeWrappedHFSPlus)) {
        result = vol;
    } else if (vol->partition_count) {
        FOR_UNTIL(i, vol->partition_count) {
            result = hfs_find(vol->partitions[i]);
            if (result != NULL)
                break;
        }
    }
    
    return result;
}

int hfs_close(HFS *hfs) {
    debug("Closing volume.");
    int result = vol_close(hfs->vol);
    return result;
}

#pragma mark Volume Structures

bool hfs_get_HFSMasterDirectoryBlock(HFSMasterDirectoryBlock* vh, const HFS* hfs)
{
    if (hfs->vol) {
        char* buffer;
        ALLOC(buffer, 2048)
        
        ssize_t size = hfs_read(buffer, hfs, 2048, 0);
        
        if (size < 1) {
            perror("read");
            critical("Cannot read volume.");
            FREE(buffer);
            return -1;
        }
        
        *vh = *(HFSMasterDirectoryBlock*)(buffer+1024);
        FREE(buffer);
        
        swap_HFSMasterDirectoryBlock(vh);
        
        return true;
    }
    
    return false;
}

bool hfs_get_HFSPlusVolumeHeader(HFSPlusVolumeHeader* vh, const HFS* hfs)
{
    if (hfs->vol) {
        char* buffer;
        ALLOC(buffer, 2048)
        
        ssize_t size = hfs_read(buffer, hfs, 2048, 0);
        
        if (size < 1) {
            perror("read");
            critical("Cannot read volume.");
            FREE(buffer);
            return -1;
        }
        
        *vh = *(HFSPlusVolumeHeader*)(buffer+1024);
        FREE(buffer);
        
        swap_HFSPlusVolumeHeader(vh);
        
        return true;
    }
    
    return false;
}

bool hfs_get_JournalInfoBlock(JournalInfoBlock* block, const HFS* hfs)
{
    if (hfs->vh.journalInfoBlock) {
        char* buffer;
        ALLOC(buffer, hfs->block_size);
        
        ssize_t read = hfs_read_blocks(buffer, hfs, 1, hfs->vh.journalInfoBlock);
        if (read < 0) {
            perror("read");
            critical("Read error when fetching journal info block");
        } else if (read < 1) {
            critical("Didn't read the whole journal info block!");
        }
        *block = *(JournalInfoBlock*)buffer; // copies
        FREE(buffer);
        
        swap_JournalInfoBlock(block);
        return true;
    }
    
    return false;
}

bool hfs_get_journalheader(journal_header *header, JournalInfoBlock info, const HFS* hfs)
{
    if (info.flags & kJIJournalInFSMask && info.offset) {
        ssize_t nbytes = hfs_read(header, hfs, sizeof(journal_header), info.offset);
        if (nbytes < 0) {
            perror("hfs_read");
            return false;
        }
        return true;
    }
    
    return false;
}
