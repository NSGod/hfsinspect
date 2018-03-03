//
//  hfsplus_xattrlist.c
//  hfsinspect
//
//  Created by Mark Douma on 2/28/2018.
//  Copyright © 2018 Mark Douma. All rights reserved.
//

#include "hfsplus_xattrlist.h"
#include "logging/logging.h"    // console printing routines

XAttrList* xattrlist_make()
{
    trace("%s()", __FUNCTION__);
    XAttrList* retval = NULL;
    SALLOC(retval, sizeof(XAttrList));
    retval->tqh_first = NULL;
    retval->tqh_last  = &retval->tqh_first; // Ensure this is pointing to the value on the heap, not the stack.
    TAILQ_INIT(retval);
    return retval;
}

void xattrlist_free(XAttrList* list)
{
    XAttr* xattr = NULL;

    trace("list (%p)", (void *)list);

    while ( (xattr = TAILQ_FIRST(list)) ) {
        TAILQ_REMOVE(list, xattr, xattrs);
        SFREE(xattr->record);
        SFREE(xattr);
    }
    SFREE(list);
}

void xattrlist_add_record(XAttrList* list, const HFSPlusAttrKey* key, const HFSPlusAttrRecord* record)
{
    XAttr *newAttribute = NULL;
    SALLOC(newAttribute, sizeof(XAttr));
    newAttribute->key = *key;

    uint32_t fullRecLen = (record->recordType == kHFSPlusAttrInlineData ? sizeof(HFSPlusAttrData) - 2 + record->attrData.attrSize : sizeof(HFSPlusAttrRecord));
    SALLOC(newAttribute->record, fullRecLen);
    memcpy(newAttribute->record, record, fullRecLen);

    if (TAILQ_EMPTY(list)) {
        TAILQ_INSERT_HEAD(list, newAttribute, xattrs);
    } else {
        TAILQ_INSERT_TAIL(list, newAttribute, xattrs);
    }
}
