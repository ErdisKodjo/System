/**
 * @file NSData.m
 * @brief Minimal immutable byte buffer.
 */

#import "afros_apple.h"
#import "Foundation_AfriOS.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation NSData

- (id)initWithBytes:(const void *)bytes length:(NSUInteger)length {
    self = [super init];
    if (!self) return nil;
    if (length == 0 || !bytes) {
        _bytes = NULL;
        _length = 0;
        return self;
    }
    void *copy = malloc(length);
    if (!copy) { [self release]; return nil; }
    memcpy(copy, bytes, length);
    _bytes      = copy;
    _length     = length;
    _owns_buffer = true;
    return self;
}

- (id)initWithBytesNoCopy:(void *)bytes length:(NSUInteger)length {
    self = [super init];
    if (!self) return nil;
    _bytes       = bytes;
    _length      = length;
    _owns_buffer = false;
    return self;
}

- (void)dealloc {
    if (_owns_buffer && _bytes) free((void *)_bytes);
    [super dealloc];
}

- (NSUInteger)length { return _length; }

- (const void *)bytes { return _bytes; }

- (NSData *)subdataWithRange:(NSRange)range {
    if (!_bytes || range.location >= _length) return nil;
    NSUInteger end = range.location + range.length;
    if (end > _length) end = _length;
    NSUInteger n = end - range.location;
    return [[[NSData alloc] initWithBytes:(const uint8_t *)_bytes
                                    + range.location
                                   length:n] autorelease];
}

- (const uint8_t *)byteAtIndex:(NSUInteger)idx {
    if (!_bytes || idx >= _length) return NULL;
    return (const uint8_t *)_bytes + idx;
}

- (NSString *)description {
    return nil;
}

+ (id)data {
    return [[[NSData alloc] init] autorelease];
}

+ (id)dataWithBytes:(const void *)bytes length:(NSUInteger)length {
    return [[[NSData alloc] initWithBytes:bytes length:length] autorelease];
}

+ (id)dataWithContentsOfFile:(NSString *)path {
    (void)path;
    /* File I/O is delegated to the host OS; stubbed out here.       */
    return nil;
}

@end
