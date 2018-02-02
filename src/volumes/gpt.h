//
//  gpt.h
//  volumes
//
//  Created by Adam Knight on 11/7/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#ifndef volumes_gpt_h
#define volumes_gpt_h

#include "volume.h"
#include "mbr.h"

#pragma mark - Structures

#define GPT_SIG "EFI PART"

/*
   http://developer.apple.com/library/mac/#technotes/tn2166/_index.html
   http://developer.apple.com/library/content/technotes/tn2166/_index.html
 
   GPTHeader is at LBA 1. GPTPartitionEntry array starts at header->partition_start_lba (usually LBA2).
   Backup header is at the next-to-last LBA of the disk. The partition array usually precedes this.

   There may be up to 128 partition records in the array (16KiB).
 */

// 92 bytes
typedef struct GPTHeader {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_lba;
    uint64_t last_lba;
    uuid_t   uuid;
    uint64_t partition_table_start_lba;
    uint32_t partitions_entry_count;
    uint32_t partitions_entry_size;
    uint32_t partition_table_crc;
} __attribute__((packed, aligned(2))) GPTHeader;

// 128 bytes
typedef struct GPTPartitionEntry {
    uuid_t   type_uuid;
    uuid_t   uuid;
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[36];
} __attribute__((packed, aligned(2))) GPTPartitionEntry;

typedef GPTPartitionEntry GPTPartitionRecord[128];

static uint64_t         kGPTRequired        __attribute__((unused))     = 0x0000000000000001; //0 - "required", "system disk"
static uint64_t         kGPTNoBlockIO       __attribute__((unused))     = 0x0000000000000002; //1 - "not mapped", "no block IO"
static uint64_t         kGPTLegacyBIOSBootable  __attribute__((unused)) = 0x0000000000000004; //2

// http://msdn.microsoft.com/en-us/library/windows/desktop/aa365449(v=vs.85).aspx
static uint64_t         kGPTMSReadOnly      __attribute__((unused))     = 0x1000000000000000; //60
static uint64_t         kGPTMSShadowCopy    __attribute__((unused))     = 0x2000000000000000; //61
static uint64_t         kGPTMSHidden        __attribute__((unused))     = 0x4000000000000000; //62
static uint64_t         kGPTMSNoAutomount   __attribute__((unused))     = 0x8000000000000000; //63


#pragma mark - Functions

extern PartitionOps gpt_ops;

void        gpt_swap_uuid           (uuid_t* uuid_p, const uuid_t* uuid) __attribute__((nonnull));
const char* gpt_partition_type_str  (uuid_t uuid, VolType* hint) __attribute__((nonnull(2)));

/**
   Tests a volume to see if it contains a GPT partition map.
   @return Returns -1 on error (check errno), 0 for NO, 1 for YES.
 */
int gpt_test(Volume* vol) __attribute__((nonnull));

/**
   Updates a volume with sub-volumes for any defined partitions.
   @return Returns -1 on error (check errno), 0 for success.
 */
int gpt_load(Volume* vol) __attribute__((nonnull));

/**
   Prints a description of the GPT structure and partition information to stdout.
   @return Returns -1 on error (check errno), 0 for success.
 */
int gpt_dump(Volume* vol) __attribute__((nonnull));

#endif
