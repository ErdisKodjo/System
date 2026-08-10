/**
 * @file NSArray.m
 * @brief Minimal immutable array.
 */

#import "afros_apple.h"
#import "Foundation_AfriOS.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* NSEnumerator                                                        */
/* ------------------------------------------------------------------ */

@implementation NSEnumerator
- (id)initWithArray:(NSArray *)array {
    self = [super init];
    if (!self) return nil;
    _array = [array retain];
    _index = 0;
    return self;
}
- (void)dealloc { [_array release]; [super dealloc]; }
- (id)nextObject {
    if (_index >= [_array count]) return nil;
    return [_array objectAtIndex:_index++];
}
@end

/* ------------------------------------------------------------------ */
/* NSArray                                                             */
/* ------------------------------------------------------------------ */

@implementation NSArray

- (id)init {
    return [self initWithObjects:NULL count:0];
}

- (id)initWithObjects:(id *)objects count:(NSUInteger)count {
    self = [super init];
    if (!self) return nil;
    _objects = (id *)calloc(count ? count : 1, sizeof(id));
    if (!_objects) { [self release]; return nil; }
    for (NSUInteger i = 0; i < count; i++) {
        _objects[i] = [objects[i] retain];
    }
    _count = count;
    return self;
}

- (id)initWithArray:(NSArray *)array {
    return [self initWithObjects:array->_objects count:array->_count];
}

- (void)dealloc {
    for (NSUInteger i = 0; i < _count; i++) {
        [_objects[i] release];
    }
    free(_objects);
    [super dealloc];
}

- (NSUInteger)count { return _count; }

- (id)objectAtIndex:(NSUInteger)idx {
    if (idx >= _count) return nil;
    return _objects[idx];
}

- (NSUInteger)indexOfObject:(id)obj {
    for (NSUInteger i = 0; i < _count; i++) {
        if ([_objects[i] isEqual:obj]) return i;
    }
    return NSNotFound;
}

- (BOOL)containsObject:(id)obj {
    return [self indexOfObject:obj] != NSNotFound;
}

- (id)firstObject {
    return _count > 0 ? _objects[0] : nil;
}

- (id)lastObject {
    return _count > 0 ? _objects[_count - 1] : nil;
}

- (NSString *)componentsJoinedByString:(NSString *)sep {
    (void)sep;
    /* Without a working string-concatenation chain we return nil.    */
    return nil;
}

- (id)objectEnumerator {
    return [[[NSEnumerator alloc] initWithArray:self] autorelease];
}

+ (id)array {
    return [[[NSArray alloc] init] autorelease];
}

+ (id)arrayWithObject:(id)obj {
    id arr[1] = { obj };
    return [[[NSArray alloc] initWithObjects:arr count:1] autorelease];
}

+ (id)arrayWithObjects:(id)firstObj, ... {
    id tmp[64];
    NSUInteger n = 0;
    va_list ap;
    va_start(ap, firstObj);
    id obj = firstObj;
    while (obj && n < 64) {
        tmp[n++] = obj;
        obj = va_arg(ap, id);
    }
    va_end(ap);
    return [[[NSArray alloc] initWithObjects:tmp count:n] autorelease];
}

@end
