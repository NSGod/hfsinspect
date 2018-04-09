//
//  mbr.c
//  volumes
//
//  Created by Adam Knight on 11/7/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include "mbr.h"
#include "output.h"
#include "_endian.h"

#include "logging/logging.h"    // console printing routines


unsigned kMBRTypeGPTProtective = 0xEE;
unsigned kMBRTypeAppleBoot     = 0xAB;
unsigned kMBRTypeAppleHFS      = 0xAF;

struct MBRPartitionName {
    uint16_t type;
    bool     lba;
    char     name[100];
    VolType  voltype;
    VolType  volsubtype;
};
typedef struct MBRPartitionName MBRPartitionName;

static MBRPartitionName mbr_partition_types[] = {
    {0x00, 0, "Free",                           kVolTypeSystem,         kSysFreeSpace},
    {0x01, 0, "MS FAT12",                       kVolTypeUserData,       kFSTypeFAT12},
    {0x04, 0, "MS FAT16",                       kVolTypeUserData,       kFSTypeFAT16},
    {0x05, 0, "Extended partition (CHS)",       kVolTypePartitionMap,   kTypeUnknown},
    {0x06, 0, "MS FAT16B",                      kVolTypeUserData,       kFSTypeFAT16B},
    {0x07, 0, "MS NTFS/exFAT; IBM IFS/HPFS",    kVolTypeUserData,       kFSTypeExFAT},
    {0x08, 0, "MS FAT12/16 CHS",                kVolTypeUserData,       kFSTypeFAT12},
    {0x0b, 0, "MS FAT32 CHS",                   kVolTypeUserData,       kFSTypeFAT32},
    {0x0c, 1, "MS FAT32X LBA",                  kVolTypeUserData,       kFSTypeFAT32},
    {0x0e, 1, "MS FAT16X LBA",                  kVolTypeUserData,       kFSTypeFAT16},
    {0x0f, 1, "Extended partition (LBA)",       kVolTypePartitionMap,   kTypeUnknown},

    {0x82, 1, "Linux Swap",                     kVolTypeSystem,         kSysSwapSpace},
    {0x83, 1, "Linux Filesystem",               kVolTypeUserData,       kTypeUnknown},
    {0x85, 1, "Linux Extended Boot Record",     kVolTypeSystem,         kSysReserved},
    {0x8e, 1, "Linux LVM",                      kVolTypeSystem,         kSysReserved},

    {0x93, 1, "Linux Hidden",                   kVolTypeSystem,         kSysReserved},

    {0xa8, 1, "Apple UFS",                      kVolTypeUserData,       kFSTypeUFS},
    {0xab, 1, "Apple Boot",                     kVolTypeSystem,         kSysReserved},
    {0xaf, 1, "Apple HFS",                      kVolTypeUserData,       kFSTypeHFS},

    {0xee, 1, "GPT Protective MBR",             kVolTypePartitionMap,   kPMTypeGPT},
    {0xef, 1, "EFI system partition",           kVolTypeSystem,         kSysEFI},
    {0, 0, {'\0'}, 0, 0},
};

int mbr_load_header(Volume* vol, MBR* mbr)
{
    if ( vol_read(vol, (void*)mbr, sizeof(MBR), 0) < 0 )
        return -1;

    return 0;
}

int mbr_test(Volume* vol)
{
    debug("MBR test");

    MBR  mbr        = {{0}};
    char mbr_sig[2] = { 0x55, 0xaa };

    if ( mbr_load_header(vol, &mbr) < 0 )
        return -1;

    if (memcmp(&mbr_sig, &mbr.signature, 2) == 0) { debug("Found an MBR pmap."); return 1; }

    return 0;
}

int mbr_load(Volume* vol)
{
    debug("MBR load");

    vol->type    = kVolTypePartitionMap;
    vol->subtype = kPMTypeMBR;

    MBR mbr = {{0}};

    if ( mbr_load_header(vol, &mbr) < 0)
        return -1;

    for(unsigned i = 0; i < 4; i++) {
        if (mbr.partitions[i].type) {
            Volume*      v      = NULL;
            MBRPartition p;
            VolType      hint   = kVolTypeSystem;
            off_t        offset = 0;
            size_t       length = 0;

            p      = mbr.partitions[i];
            offset = p.first_sector_lba * vol->sector_size;
            length = p.sector_count * vol->sector_size;

            v      = vol_make_partition(vol, i, offset, length);

            char const* name = mbr_partition_type_str(p.type, &hint);
            if (name != NULL) {
                v->type = hint;
            }
        }
    }

    return 0;
}

const char* mbr_partition_type_str(uint16_t type, VolType* hint)
{
    static char type_str[100];

    for(unsigned i = 0; i < 22; i++) {
        if (mbr_partition_types[i].type == type) {
            if (hint != NULL) *hint = mbr_partition_types[i].voltype;
            (void)strlcpy(type_str, mbr_partition_types[i].name, 99);
			return type_str;
        }
    }

	(void)strlcpy(type_str, "unknown", 100);
    return type_str;
}

int mbr_dump(Volume* vol)
{
    const char* type_str = NULL;
    MBR*        mbr      = NULL;
    out_ctx*    ctx      = vol->ctx;

    debug("MBR dump");

    SALLOC(mbr, sizeof(MBR));

    if ( mbr_load_header(vol, mbr) < 0)
        return -1;

    BeginSection(ctx, "Master Boot Record");
    PrintAttribute(ctx, "signature", "0x%x%x", mbr->signature[1], mbr->signature[0]);

    for(unsigned i = 0; i < 4; i++) {
        MBRPartition* partition = &mbr->partitions[i];
        if (partition->type == 0) continue;

        type_str = mbr_partition_type_str(partition->type, NULL);

        BeginSection(ctx, "Partition %d", i + 1);
        PrintUIHex  (ctx, partition, status);
        PrintUI     (ctx, partition, first_sector.head);
        PrintUI     (ctx, partition, first_sector.cylinder);
        PrintUI     (ctx, partition, first_sector.sector);
        PrintAttribute(ctx, "type", "0x%02X (%s)", partition->type, type_str);
        PrintUI     (ctx, partition, last_sector.head);
        PrintUI     (ctx, partition, last_sector.cylinder);
        PrintUI     (ctx, partition, last_sector.sector);
        PrintUI     (ctx, partition, first_sector_lba);
        PrintUI     (ctx, partition, sector_count);
        _PrintDataLength(ctx, "(size)", (partition->sector_count * vol->sector_size));
        EndSection(ctx);
    }

    EndSection(ctx);

    return 0;
}

PartitionOps mbr_ops = {
    .name = "MBR",
    .test = mbr_test,
    .dump = mbr_dump,
    .load = mbr_load,
};
