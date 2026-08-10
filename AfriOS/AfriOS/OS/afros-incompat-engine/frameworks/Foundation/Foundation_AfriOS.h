/**
 * @file Foundation_AfriOS.h
 * @brief Umbrella declarations for the AfriOS Foundation stub.
 *
 * Declares the @interface blocks for every Foundation class
 * implemented in this directory. The matching @implementation
 * blocks live in the .m files alongside this header.
 */

#import "afros_apple.h"

@class NSString, NSArray, NSDictionary, NSData, NSEnumerator;

/* ------------------------------------------------------------------ */
/* NSString                                                            */
/* ------------------------------------------------------------------ */

@interface NSString : NSObject {
@protected
    uint16_t *_characters;
    NSUInteger _length;
    bool      _owns_buffer;
}
- (id)initWithUTF8String:(const char *)bytes;
- (id)initWithCharacters:(const uint16_t *)chars length:(NSUInteger)len;
- (NSUInteger)length;
- (uint16_t)characterAtIndex:(NSUInteger)idx;
- (BOOL)isEqualToString:(NSString *)other;
- (NSString *)stringByAppendingString:(NSString *)suffix;
- (NSString *)substringWithRange:(NSRange)range;
- (const char *)UTF8String;
- (NSString *)description;
+ (id)string;
+ (id)stringWithUTF8String:(const char *)bytes;
+ (id)stringWithFormat:(const char *)fmt, ...;
@end

/* ------------------------------------------------------------------ */
/* NSArray                                                             */
/* ------------------------------------------------------------------ */

@interface NSArray : NSObject {
@public
    id         *_objects;
    NSUInteger  _count;
}
- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)idx;
- (NSUInteger)indexOfObject:(id)obj;
- (BOOL)containsObject:(id)obj;
- (id)firstObject;
- (id)lastObject;
- (id)initWithObjects:(id *)objects count:(NSUInteger)count;
- (id)initWithArray:(NSArray *)array;
- (NSString *)componentsJoinedByString:(NSString *)sep;
- (id)objectEnumerator;
+ (id)array;
+ (id)arrayWithObject:(id)obj;
+ (id)arrayWithObjects:(id)firstObj, ...;
@end

/* ------------------------------------------------------------------ */
/* NSDictionary                                                        */
/* ------------------------------------------------------------------ */

@interface NSDictionary : NSObject {
@public
    id         *_keys;
    id         *_values;
    NSUInteger  _count;
}
- (NSUInteger)count;
- (id)objectForKey:(id)key;
- (id)objectForKeyedSubscript:(id)key;
- (BOOL)containsKey:(id)key;
- (NSArray *)allKeys;
- (NSArray *)allValues;
- (id)initWithObjects:(id *)objects forKeys:(id *)keys count:(NSUInteger)count;
- (id)initWithDictionary:(NSDictionary *)other;
- (id)keyEnumerator;
+ (id)dictionary;
+ (id)dictionaryWithObject:(id)obj forKey:(id)key;
@end

/* ------------------------------------------------------------------ */
/* NSData                                                              */
/* ------------------------------------------------------------------ */

@interface NSData : NSObject {
@protected
    const void *_bytes;
    NSUInteger  _length;
    bool        _owns_buffer;
}
- (NSUInteger)length;
- (const void *)bytes;
- (NSData *)subdataWithRange:(NSRange)range;
- (const uint8_t *)byteAtIndex:(NSUInteger)idx;
- (id)initWithBytes:(const void *)bytes length:(NSUInteger)length;
- (id)initWithBytesNoCopy:(void *)bytes length:(NSUInteger)length;
+ (id)data;
+ (id)dataWithBytes:(const void *)bytes length:(NSUInteger)length;
+ (id)dataWithContentsOfFile:(NSString *)path;
@end

/* ------------------------------------------------------------------ */
/* NSEnumerator                                                        */
/* ------------------------------------------------------------------ */

@interface NSEnumerator : NSObject {
@protected
    NSArray    *_array;
    NSUInteger  _index;
}
- (id)initWithArray:(NSArray *)array;
- (id)nextObject;
@end
