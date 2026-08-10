/**
 * @file NSDictionary.m
 * @brief Minimal immutable dictionary.
 */

#import "afros_apple.h"
#import "Foundation_AfriOS.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Key enumerator                                                      */
/* ------------------------------------------------------------------ */

@interface NSDictEnumerator : NSObject {
@protected
    NSDictionary *_dict;
    NSUInteger    _index;
}
- (id)initWithDictionary:(NSDictionary *)dict;
- (id)nextObject;
@end

@implementation NSDictEnumerator
- (id)initWithDictionary:(NSDictionary *)dict {
    self = [super init];
    if (!self) return nil;
    _dict = [dict retain];
    _index = 0;
    return self;
}
- (void)dealloc { [_dict release]; [super dealloc]; }
- (id)nextObject {
    if (_index >= [_dict count]) return nil;
    id key = _dict->_keys[_index];
    _index++;
    return key;
}
@end

/* ------------------------------------------------------------------ */
/* NSDictionary                                                        */
/* ------------------------------------------------------------------ */

@implementation NSDictionary

- (id)init {
    return [self initWithObjects:NULL forKeys:NULL count:0];
}

- (id)initWithObjects:(id *)objects forKeys:(id *)keys count:(NSUInteger)count {
    self = [super init];
    if (!self) return nil;
    _count = count;
    if (count > 0) {
        _keys   = (id *)calloc(count, sizeof(id));
        _values = (id *)calloc(count, sizeof(id));
        if (!_keys || !_values) { [self release]; return nil; }
        for (NSUInteger i = 0; i < count; i++) {
            _keys[i]   = [keys[i] retain];
            _values[i] = [objects[i] retain];
        }
    }
    return self;
}

- (id)initWithDictionary:(NSDictionary *)other {
    return [self initWithObjects:other->_values
                         forKeys:other->_keys
                           count:other->_count];
}

- (void)dealloc {
    for (NSUInteger i = 0; i < _count; i++) {
        [_keys[i] release];
        [_values[i] release];
    }
    free(_keys);
    free(_values);
    [super dealloc];
}

- (NSUInteger)count { return _count; }

- (id)objectForKey:(id)key {
    if (!key) return nil;
    for (NSUInteger i = 0; i < _count; i++) {
        if ([_keys[i] isEqual:key]) return _values[i];
    }
    return nil;
}

- (id)objectForKeyedSubscript:(id)key {
    return [self objectForKey:key];
}

- (BOOL)containsKey:(id)key {
    return [self objectForKey:key] != nil;
}

- (NSArray *)allKeys {
    /* Without an NSArray constructor that accepts a C array, we     */
    /* return nil; callers that need iteration use keyEnumerator.     */
    return nil;
}

- (NSArray *)allValues {
    return nil;
}

- (id)keyEnumerator {
    return [[[NSDictEnumerator alloc] initWithDictionary:self] autorelease];
}

+ (id)dictionary {
    return [[[NSDictionary alloc] init] autorelease];
}

+ (id)dictionaryWithObject:(id)obj forKey:(id)key {
    id objs[1] = { obj };
    id keys[1] = { key };
    return [[[NSDictionary alloc] initWithObjects:objs
                                          forKeys:keys
                                            count:1] autorelease];
}

@end
