//
//  apm.c
//  volumes
//
//  Created by Adam Knight on 11/7/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#include "apm.h"
#include "logging/logging.h"    // console printing routines

#include "volumes/_endian.h"
#include "volumes/output.h"

#pragma mark Interface (Private)

void swap_APMHeader(APMHeader* record) __attribute__((nonnull));
int  apm_get_header(Volume* vol, APMHeader* header, unsigned partition_number) __attribute__((nonnull));
int  apm_get_volume_header(Volume* vol, APMHeader* header) __attribute__((nonnull));

#pragma mark Implementation

void swap_APMHeader(APMHeader* record)
{
    Swap16(record->signature);
    Swap16(record->reserved1);
    Swap32(record->partition_count);
    Swap32(record->partition_start);
    Swap32(record->partition_length);
//    char name[32];
//    char type[32];
    Swap32(record->data_start);
    Swap32(record->data_length);
    Swap32(record->status);
    Swap32(record->boot_code_start);
    Swap32(record->boot_code_length);
    Swap32(record->bootloader_address);
    Swap32(record->reserved2);
    Swap32(record->boot_code_entry);
    Swap32(record->reserved3);
    Swap32(record->boot_code_checksum);
//    char processor_type[16];
}

int apm_get_header(Volume* vol, APMHeader* header, unsigned partition_number)
{
    size_t  block_size = 0;
    ssize_t bytes      = 0;

    block_size = vol->sector_size;
    bytes      = vol_read(vol, header, sizeof(APMHeader), ((off_t)block_size * partition_number));

    if (bytes < 0) return -1;

    swap_APMHeader(header);

    return 0;
}

int apm_get_volume_header(Volume* vol, APMHeader* header)
{
    return apm_get_header(vol, header, 1);
}

/**
   Tests a volume to see if it contains an APM partition map.
   Note that APM volumes can have block sizes of 512, 1K or 2K.
   @return Returns -1 on error (check errno), 0 for NO, 1 for YES.
 */
int apm_test(Volume* vol)
{
    char* buf = NULL; // char for pointer math

    debug("APM test");

    SALLOC(buf, 4096);

    if ( vol_read(vol, buf, 4096, 0) < 0 ) {
        SFREE(buf);
        return -1;
    }

    int result = 0;

    if ( memcmp(buf+512, APM_SIG, 2) == 0 ) {
        vol->sector_size = 512;
        result           = 1;
        goto OUT;
    }

    if ( memcmp(buf+1024, APM_SIG, 2) == 0 ) {
        vol->sector_size = 1024;
        result           = 1;
        goto OUT;
    }

    if ( memcmp(buf+2048, APM_SIG, 2) == 0 ) {
        vol->sector_size = 2048;
        result           = 1;
        goto OUT;
    }

OUT:
    SFREE(buf);
    return result;
}

/**
   Prints a description of the APM structure and partition information to stdout.
   @return Returns -1 on error (check errno), 0 for success.
 */
int apm_dump(Volume* vol)
{
    unsigned   partitionID = 1;
    APMHeader* header      = NULL;
    out_ctx*   ctx         = vol->ctx;

    debug("APM dump");

    SALLOC(header, sizeof(APMHeader));

    bool prevWideAtts = ctx->wideAttributes;
    ctx->wideAttributes = true;

    BeginSection(ctx, "Apple Partition Map");

    while (1) {
        char str[33] = "";

        if (apm_get_header(vol, header, partitionID) == -1) {
            SFREE(header);
            perror("get APM header");
            return -1;
        }

        BeginSection(ctx, "Partition %d", partitionID);
        PrintUIChar         (ctx, header, signature);
        PrintUI             (ctx, header, partition_count);
        PrintUI             (ctx, header, partition_start);
        PrintUI             (ctx, header, partition_length);
        _PrintDataLength    (ctx, "partition_length (bytes)", (uint64_t)header->partition_length * vol->sector_size);

        memcpy(str, &header->name, 32); str[32] = '\0';
        PrintAttribute(ctx, "name", "%s", str);

        memcpy(str, &header->type, 32); str[32] = '\0';
        for (unsigned i = 0; APMPartitionIdentifers[i].type[0] != '\0'; i++) {
            APMPartitionIdentifer identifier = APMPartitionIdentifers[i];
            if ( (strncasecmp((char*)&header->type, identifier.type, 32) == 0) ) {
                PrintAttribute(ctx, "type", "%s (%s)", identifier.name, identifier.type);
            }
        }

        PrintUI             (ctx, header, data_start);
        PrintUI             (ctx, header, data_length);
        _PrintDataLength    (ctx, "data_length (bytes)", (uint64_t)header->data_length * vol->sector_size);

        PrintUIBinary       (ctx, header, status);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusValid);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusAllocated);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusInUse);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusBootInfo);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusReadable);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusWritable);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusPositionIndependent);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusChainCompatible);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusRealDriver);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusChainDriver);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusAutoMount);
        PrintUIFlagIfMatch  (ctx, header->status, kAPMStatusIsStartup);

        PrintUI             (ctx, header, boot_code_start);
        PrintUI             (ctx, header, boot_code_length);
        _PrintDataLength(ctx, "boot_code_length (bytes)", (uint64_t)header->boot_code_length * vol->sector_size);
        PrintUI             (ctx, header, bootloader_address);
        PrintUI             (ctx, header, boot_code_entry);
        PrintUI             (ctx, header, boot_code_checksum);

        memcpy(str, &header->processor_type, 16); str[16] = '\0';
        PrintAttribute(ctx, "processor_type", "'%s'", str);
        EndSection(ctx);

        if (partitionID >= header->partition_count)
            break;
        else
            partitionID++;
    }

    EndSection(ctx); // Apple Partition Map
    ctx->wideAttributes = prevWideAtts;

    if (header != NULL)
        SFREE(header);

    return 0;
}

/**
   Updates a volume with sub-volumes for any defined partitions.
   @return Returns -1 on error (check errno), 0 for success.
 */
int apm_load(Volume* vol)
{
    debug("APM load");

    vol->type    = kVolTypePartitionMap;
    vol->subtype = kPMTypeAPM;

    APMHeader header      = {0};
    unsigned  partitionID = 1;

    do {
        if ( apm_get_header(vol, &header, partitionID) < 0 )
            return -1;

        uint32_t    sector_size = vol->sector_size;
        off_t       offset      = (off_t)header.partition_start * sector_size;
        uint64_t    length      = (uint64_t)header.partition_length * sector_size;

        Volume* partition   = vol_make_partition(vol, partitionID - 1, offset, length);
        partition->sector_size  = sector_size;
        partition->sector_count = header.partition_length;

        memcpy(partition->desc, &header.name, 32);
        memcpy(partition->native_desc, &header.type, 32);

        for (unsigned i = 0; APMPartitionIdentifers[i].type[0] != '\0'; i++) {
            APMPartitionIdentifer identifier = APMPartitionIdentifers[i];
            if ( (strncasecmp((char*)&header.type, identifier.type, 32) == 0) ) {
                partition->type = identifier.voltype;
            }
        }
    } while (++partitionID <= header.partition_count);

    return 0;
}

PartitionOps apm_ops = {
    .name = "Apple Partition Map",
    .test = apm_test,
    .dump = apm_dump,
    .load = apm_load,
};
