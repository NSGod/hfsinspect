//
//  apm.h
//  volumes
//
//  Created by Adam Knight on 11/7/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#ifndef volumes_apm_h
#define volumes_apm_h

#include "volume.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

#pragma mark - Structures

#define APM_SIG "PM"

// 136 bytes (512 reserved on-disk)
typedef struct APMHeader {
    uint16_t signature;
    uint16_t reserved1;
    uint32_t partition_count;
    uint32_t partition_start;
    uint32_t partition_length;
    char     name[32];
    char     type[32];
    uint32_t data_start;
    uint32_t data_length;
    uint32_t status;
    uint32_t boot_code_start;
    uint32_t boot_code_length;
    uint32_t bootloader_address;
    uint32_t reserved2;
    uint32_t boot_code_entry;
    uint32_t reserved3;
    uint32_t boot_code_checksum;
    char     processor_type[16];
//    char     reserved4[376];
} __attribute__((packed, aligned(2))) APMHeader;

typedef struct APMPartitionIdentifer {
    char    type[32];
    char    name[100];
    VolType voltype;
    VolType volsubtype;
} __attribute__((packed, aligned(2))) APMPartitionIdentifer;

static APMPartitionIdentifer APMPartitionIdentifers[] = {
    {"Apple_Boot",                  "OS X Open Firmware 3.x booter",    kVolTypeSystem,     kSysReserved},
    {"Apple_Boot_RAID",             "RAID partition",                   kVolTypeSystem,     kSysReserved},
    {"Apple_Bootstrap",             "secondary loader",                 kVolTypeSystem,     kSysReserved},
    {"Apple_Driver",                "device driver",                    kVolTypeSystem,     kSysReserved},
    {"Apple_Driver43",              "SCSI Manager 4.3 device driver",   kVolTypeSystem,     kSysReserved},
    {"Apple_Driver43_CD",           "SCSI CD-ROM device driver",        kVolTypeSystem,     kSysReserved},
    {"Apple_Driver_ATA",            "ATA device driver",                kVolTypeSystem,     kSysReserved},
    {"Apple_Driver_ATAPI",          "ATAPI device driver",              kVolTypeSystem,     kSysReserved},
    {"Apple_Driver_IOKit",          "IOKit device driver",              kVolTypeSystem,     kSysReserved},
    {"Apple_Driver_OpenFirmware",   "Open Firmware device driver",      kVolTypeSystem,     kSysReserved},
    {"Apple_Extra",                 "unused",                           kVolTypeSystem,     kSysFreeSpace},
    {"Apple_Free",                  "free space",                       kVolTypeSystem,     kSysFreeSpace},
    {"Apple_FWDriver",              "FireWire device driver",           kVolTypeSystem,     kSysReserved},
    {"Apple_HFS",                   "HFS/HFS+",                         kVolTypeUserData,   kFSTypeHFS},
    {"Apple_HFSX",                  "HFSX",                             kVolTypeUserData,   kFSTypeHFSX},
    {"Apple_Loader",                "secondary loader",                 kVolTypeSystem,     kSysReserved},
    {"Apple_MFS",                   "Macintosh File System",            kVolTypeUserData,   kFSTypeMFS},
    {"Apple_Partition_Map",         "partition map",                    kVolTypeSystem,     kPMTypeAPM},
    {"Apple_Patches",               "patch partition",                  kVolTypeSystem,     kSysReserved},
    {"Apple_PRODOS",                "ProDOS",                           kVolTypeUserData,   kTypeUnknown},
    {"Apple_Rhapsody_UFS",          "UFS",                              kVolTypeUserData,   kFSTypeUFS},
    {"Apple_Scratch",               "empty",                            kVolTypeSystem,     kSysFreeSpace},
    {"Apple_Second",                "secondary loader",                 kVolTypeSystem,     kSysReserved},
    {"Apple_UFS",                   "UFS",                              kVolTypeUserData,   kFSTypeUFS},
    {"Apple_UNIX_SVR2",             "UNIX file system",                 kVolTypeUserData,   kFSTypeUFS},
    {"Apple_Void",                  "dummy partition (empty)",          kVolTypeSystem,     kSysFreeSpace},
    {"Be_BFS",                      "BeOS BFS",                         kVolTypeUserData,   kFSTypeBeFS},

    {"", "", 0, 0}
};

static uint32_t              kAPMStatusValid               = 0x00000001;
static uint32_t              kAPMStatusAllocated           = 0x00000002;
static uint32_t              kAPMStatusInUse               = 0x00000004;
static uint32_t              kAPMStatusBootInfo            = 0x00000008;
static uint32_t              kAPMStatusReadable            = 0x00000010;
static uint32_t              kAPMStatusWritable            = 0x00000020;
static uint32_t              kAPMStatusPositionIndependent = 0x00000040;
static uint32_t              kAPMStatusChainCompatible     = 0x00000100;
static uint32_t              kAPMStatusRealDriver          = 0x00000200;
static uint32_t              kAPMStatusChainDriver         = 0x00000400;
static uint32_t              kAPMStatusAutoMount           = 0x40000000;
static uint32_t              kAPMStatusIsStartup           = 0x80000000;

#pragma GCC diagnostic pop // -Wunused-variable


#pragma mark - Functions

extern PartitionOps apm_ops;

/**
   Tests a volume to see if it contains an APM partition map.
   @return Returns -1 on error (check errno), 0 for NO, 1 for YES.
 */
int apm_test(Volume* vol) __attribute__((nonnull));

/**
   Updates a volume with sub-volumes for any defined partitions.
   @return Returns -1 on error (check errno), 0 for success.
 */
int apm_load(Volume* vol) __attribute__((nonnull));

/**
   Prints a description of the APM structure and partition information to stdout.
   @return Returns -1 on error (check errno), 0 for success.
 */
int apm_dump(Volume* vol) __attribute__((nonnull));

#endif
