//
//  mbr.h
//  volumes
//
//  Created by Adam Knight on 11/7/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#ifndef volumes_mbr_h
#define volumes_mbr_h

#include "volume.h"


#pragma mark - Structures

#pragma pack(push, 1)
#pragma options align=packed

typedef struct MBRCHS {
    uint8_t  head;
    uint16_t sector : 6;
    uint16_t cylinder : 10;
} MBRCHS;

#define empty_MBRCHS ((MBRCHS){ .head = 0, .sector = 0, .cylinder = 0})

typedef struct MBRPartition {
    uint8_t  status;
    MBRCHS   first_sector;
    uint8_t  type;
    MBRCHS   last_sector;
    uint32_t first_sector_lba;
    uint32_t sector_count;
} MBRPartition;

#define empty_MBRPartition ((MBRPartition){ \
        .status = 0, \
        .first_sector = empty_MBRCHS, \
        .type = 0, \
        .last_sector = empty_MBRCHS, \
        .first_sector_lba = 0, \
        .sector_count = 0 \
    })

typedef struct MBR {
    uint8_t      bootstrap[440];
    uint32_t     disk_signature;
    uint16_t     reserved0;
    MBRPartition partitions[4];
    uint8_t      signature[2];
} MBR;

#define empty_MBR ((MBR){ \
        .bootstrap = {0}, \
        .disk_signature = 0, \
        .reserved0 = 0, \
        .partitions = { empty_MBRPartition, empty_MBRPartition, empty_MBRPartition, empty_MBRPartition }, \
        .signature = {0} \
    })

#pragma pack(pop)


extern unsigned kMBRTypeGPTProtective;
extern unsigned kMBRTypeAppleBoot;
extern unsigned kMBRTypeAppleHFS;


#pragma mark - Functions

extern PartitionOps     mbr_ops;

/**
   Tests a volume to see if it contains an MBR partition map.
   @return Returns -1 on error (check errno), 0 for NO, 1 for YES.
 */
int mbr_test(Volume* vol) __attribute__((nonnull));

int mbr_load_header(Volume* vol, MBR* mbr) __attribute__((nonnull));

/**
   Updates a volume with sub-volumes for any defined partitions.
   @return Returns -1 on error (check errno), 0 for success.
 */
int mbr_load(Volume* vol) __attribute__((nonnull));

/**
   Prints a description of the MBR structure and partition information to stdout.
   @return Returns -1 on error (check errno), 0 for success.
 */
int mbr_dump(Volume* vol) __attribute__((nonnull));

/// Returns the name for the given type. If associated with a volume, pass it as the hint (or NULL).
const char* mbr_partition_type_str(uint16_t type, VolType* hint);

#endif
