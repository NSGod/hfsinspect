//
//  hfsplus_xattrlist.h
//  hfsinspect
//
//  Created by Mark Douma on 2/28/2018.
//  Copyright © 2018 Mark Douma. All rights reserved.
//

#ifndef hfsplus_xattrlist_h
#define hfsplus_xattrlist_h

#if defined(__linux__)
#include <bsd/sys/queue.h>  // TAILQ
#else
#include <sys/queue.h>
#endif

#include "hfs/types.h"

typedef struct _XAttr {
    HFSPlusAttrKey      key;
    HFSPlusAttrRecord*  record;
    TAILQ_ENTRY(_XAttr) xattrs;
} XAttr;

typedef TAILQ_HEAD(_XAttrList, _XAttr) XAttrList;


// Creates the tailq head node.
XAttrList* xattrlist_make(void);

// Removes and frees all the nodes
void xattrlist_free(XAttrList* list) __attribute__((nonnull));

// Adds a key and record
void xattrlist_add_record(XAttrList* list, const HFSPlusAttrKey* key, const HFSPlusAttrRecord* record) __attribute__((nonnull));

#endif /* hfsplus_xattrlist_h */
