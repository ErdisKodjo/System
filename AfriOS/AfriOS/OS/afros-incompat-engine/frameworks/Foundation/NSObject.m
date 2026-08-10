/**
 * @file NSObject.m
 * @brief Minimal root class implementation for the AfriOS Apple
 *        compatibility layer.
 *
 * Note: AfriOS does NOT link against libobjc; this file (combined with
 * afros_apple.h's @interface NSObject declaration) is the entire root
 * class machinery.
 */

#import "afros_apple.h"

#include <stdlib.h>
#include <string.h>

/* Generous allocation size that covers NSObject plus the largest      */
/* AfriOS framework subclass. Each subclass inherits +alloc from       */
/* NSObject, so this must accommodate every subclass layout.           */
#define AFROS_NSOBJECT_ALLOC_SIZE 256

/* ------------------------------------------------------------------ */
/* NSObject                                                            */
/* ------------------------------------------------------------------ */

@implementation NSObject

+ (id)alloc {
    NSObject *o = (NSObject *)calloc(1, AFROS_NSOBJECT_ALLOC_SIZE);
    if (!o) return nil;
    o->_afros_isa = self;
    o->_afros_retain_count = 1;
    return (id)o;
}

- (id)init {
    return self;
}

- (id)copy {
    return [self copyWithZone:nil];
}

- (id)copyWithZone:(NSZone *)zone {
    (void)zone;
    /* Default shallow copy: re-alloc and memcpy.                    */
    NSObject *copy = [[self class] alloc];
    if (copy) {
        memcpy(copy, self, AFROS_NSOBJECT_ALLOC_SIZE);
        copy->_afros_retain_count = 1;
    }
    return (id)copy;
}

- (id)mutableCopy {
    return [self mutableCopyWithZone:nil];
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return [self copyWithZone:zone];
}

- (Class)class {
    return _afros_isa;
}

- (Class)superclass {
    /* Without a real runtime, we cannot follow the hierarchy.       */
    return nil;
}

- (BOOL)isKindOfClass:(Class)cls {
    (void)cls;
    return YES;
}

- (BOOL)isMemberOfClass:(Class)cls {
    return _afros_isa == cls;
}

- (BOOL)respondsToSelector:(SEL)sel {
    (void)sel;
    return NO;
}

- (id)performSelector:(SEL)sel {
    (void)sel;
    return nil;
}

- (id)performSelector:(SEL)sel withObject:(id)obj {
    (void)sel; (void)obj;
    return nil;
}

- (id)performSelector:(SEL)sel withObject:(id)obj1 withObject:(id)obj2 {
    (void)sel; (void)obj1; (void)obj2;
    return nil;
}

- (id)retain {
    _afros_retain_count++;
    return self;
}

- (void)release {
    if (_afros_retain_count > 0) {
        _afros_retain_count--;
        if (_afros_retain_count == 0) {
            [self dealloc];
        }
    }
}

- (id)autorelease {
    /* Bridge to the runtime autorelease pool.                      */
    objc_autorelease(self);
    return self;
}

- (unsigned)retainCount {
    return (unsigned)_afros_retain_count;
}

- (void)dealloc {
    /* The instance was allocated with calloc; release with free.    */
    free(self);
}

- (id)forwardingTargetForSelector:(SEL)sel {
    (void)sel;
    return nil;
}

- (NSString *)description {
    /* Without a working NSString we just return nil. Subclasses     */
    /* override this for useful output.                              */
    return nil;
}

- (NSUInteger)hash {
    return (NSUInteger)(uintptr_t)self;
}

- (BOOL)isEqual:(id)object {
    return self == object;
}

- (id)self {
    return self;
}

- (BOOL)isProxy {
    return NO;
}

- (IMP)methodForSelector:(SEL)sel {
    (void)sel;
    return (IMP)0;
}

- (void)doesNotRecognizeSelector:(SEL)sel {
    (void)sel;
    /* In a real runtime this would raise NSInvalidArgumentException. */
}

@end

/* ------------------------------------------------------------------ */
/* Class-method helpers used by other frameworks                       */
/* ------------------------------------------------------------------ */

@implementation NSObject (AfriOSHelpers)

+ (id)new {
    return [[self alloc] init];
}

+ (Class)class {
    return self;
}

+ (Class)superclass {
    return nil;
}

+ (BOOL)isSubclassOfClass:(Class)cls {
    (void)cls;
    return YES;
}

+ (BOOL)respondsToSelector:(SEL)sel {
    (void)sel;
    return NO;
}

+ (id)performSelector:(SEL)sel {
    (void)sel;
    return nil;
}

@end
