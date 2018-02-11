//
//  range.h
//  hfsinspect
//
//  Created by Adam Knight on 6/7/13.
//  Copyright (c) 2013 Adam Knight. All rights reserved.
//

#ifndef hfsinspect_range_h
#define hfsinspect_range_h

#include <stdbool.h>
#include <sys/types.h>

#pragma mark Range Helpers

typedef struct range {
    size_t start;
    size_t count;
} range;
typedef range* range_ptr;

range  make_range          (size_t start, size_t count);
bool   range_equal         (range a, range b);
bool   range_contains      (size_t pos, range test);
size_t range_max           (range r);
range  range_intersection  (range a, range b);
range  range_union         (range a, range b);

#endif
