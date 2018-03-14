//
//  operations.c
//  hfsinspect
//
//  Created by Adam Knight on 4/1/14.
//  Copyright (c) 2014 Adam Knight. All rights reserved.
//

#include "operations.h"

char* BTreeOptionCatalog    = "catalog";
char* BTreeOptionExtents    = "extents";
char* BTreeOptionAttributes = "attributes";
char* BTreeOptionHotfiles   = "hotfiles";

char* BTreeListOptionFile           = "file";
char* BTreeListOptionFolder         = "folder";
char* BTreeListOptionFileThread     = "filethread";
char* BTreeListOptionFolderThread   = "folderthread";
char* BTreeListOptionHotfile        = "hotfile";
char* BTreeListOptionHotfileThread  = "hotfilethread";
char* BTreeListOptionExtents        = "extents";
char* BTreeListOptionAny            = "any";

void set_mode(HIOptions* options, int mode)     { options->mode |= (1 << mode); }
void clear_mode(HIOptions* options, int mode)   { options->mode &= ~(1 << mode); }
bool check_mode(HIOptions* options, int mode)   { return (options->mode & (1 << mode)); }

void set_list_type(HIOptions* options, BTreeListType type)
{
    type == BTreeListTypeAny ? (options->list_type = type) : (options->list_type |= (1 << type));
}

void clear_list_type(HIOptions* options, BTreeListType type)
{
    if (type == BTreeListTypeAny) {
        options->list_type &= ~type;
    } else {
        options->list_type &= ~(1 << type);
    }
}
bool check_list_type(HIOptions* options, BTreeListType type)
{
    return (options->list_type & (type == BTreeListTypeAny ? type : (1 << type)));
}

