/**
 * @file NSString.m
 * @brief Minimal immutable UTF-16 string implementation.
 */

#import "afros_apple.h"
#import "Foundation_AfriOS.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation NSString

- (id)initWithUTF8String:(const char *)bytes {
    self = [super init];
    if (!self) return nil;
    if (!bytes) { _length = 0; _characters = NULL; return self; }
    /* Convert UTF-8 to UTF-16 (BMP-only, ASCII fast path).          */
    size_t n = strlen(bytes);
    _characters = (uint16_t *)calloc(n ? n : 1, sizeof(uint16_t));
    if (!_characters) { [self release]; return nil; }
    _owns_buffer = true;
    for (size_t i = 0; i < n; i++) {
        unsigned char b = (unsigned char)bytes[i];
        if (b < 0x80) {
            _characters[i] = (uint16_t)b;
        } else {
            /* Naive: just copy the low byte.                       */
            _characters[i] = (uint16_t)b;
        }
    }
    _length = (NSUInteger)n;
    return self;
}

- (id)initWithCharacters:(const uint16_t *)chars length:(NSUInteger)len {
    self = [super init];
    if (!self) return nil;
    if (len == 0 || !chars) {
        _characters = NULL;
        _length = 0;
        return self;
    }
    _characters = (uint16_t *)calloc(len, sizeof(uint16_t));
    if (!_characters) { [self release]; return nil; }
    memcpy(_characters, chars, len * sizeof(uint16_t));
    _length = len;
    _owns_buffer = true;
    return self;
}

- (void)dealloc {
    if (_owns_buffer && _characters) free(_characters);
    [super dealloc];
}

- (NSUInteger)length {
    return _length;
}

- (uint16_t)characterAtIndex:(NSUInteger)idx {
    if (idx >= _length || !_characters) return 0;
    return _characters[idx];
}

- (BOOL)isEqualToString:(NSString *)other {
    if (!other) return NO;
    if ([other length] != _length) return NO;
    for (NSUInteger i = 0; i < _length; i++) {
        if (_characters[i] != [other characterAtIndex:i]) return NO;
    }
    return YES;
}

- (BOOL)isEqual:(id)object {
    if (self == object) return YES;
    if (!object) return NO;
    /* Best-effort: assume object is an NSString.                    */
    return [self isEqualToString:(NSString *)object];
}

- (NSUInteger)hash {
    /* FNV-1a over the UTF-16 code units.                            */
    NSUInteger h = 2166136261u;
    for (NSUInteger i = 0; i < _length; i++) {
        h ^= _characters[i];
        h *= 16777619u;
    }
    return h;
}

- (NSString *)stringByAppendingString:(NSString *)suffix {
    if (!suffix) return self;
    NSUInteger n1 = _length;
    NSUInteger n2 = [suffix length];
    uint16_t *combined = (uint16_t *)calloc(n1 + n2 ? n1 + n2 : 1,
                                            sizeof(uint16_t));
    if (!combined) return nil;
    for (NSUInteger i = 0; i < n1; i++) combined[i] = _characters[i];
    for (NSUInteger i = 0; i < n2; i++)
        combined[n1 + i] = [suffix characterAtIndex:i];
    NSString *result = [[NSString alloc] initWithCharacters:combined
                                                     length:n1 + n2];
    free(combined);
    return [result autorelease];
}

- (NSString *)substringWithRange:(NSRange)range {
    if (range.location > _length) return nil;
    if (range.location + range.length > _length) {
        range.length = _length - range.location;
    }
    return [[[NSString alloc] initWithCharacters:_characters + range.location
                                          length:range.length] autorelease];
}

- (const char *)UTF8String {
    /* ASCII-only fast path; produce a NUL-terminated buffer.        */
    char *buf = (char *)malloc(_length + 1);
    if (!buf) return NULL;
    for (NSUInteger i = 0; i < _length; i++) {
        uint16_t c = _characters ? _characters[i] : 0;
        buf[i] = (c < 0x80) ? (char)c : '?';
    }
    buf[_length] = '\0';
    /* Leak intentionally: callers expect a stable pointer.          */
    return (const char *)buf;
}

- (NSString *)description {
    return self;
}

+ (id)string {
    return [[NSString alloc] init];
}

+ (id)stringWithUTF8String:(const char *)bytes {
    return [[[NSString alloc] initWithUTF8String:bytes] autorelease];
}

+ (id)stringWithFormat:(const char *)fmt, ... {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return [NSString stringWithUTF8String:buf];
}

@end
