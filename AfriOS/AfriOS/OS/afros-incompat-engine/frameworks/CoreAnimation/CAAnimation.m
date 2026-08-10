/**
 * @file CAAnimation.m
 * @brief CAAnimation base class: duration, timing function, delegate.
 *
 * Concrete subclasses (CABasicAnimation, CAKeyframeAnimation) are not
 * provided; AfriOS's compositor consumes the public properties
 * declared here.
 */

#import "afros_apple.h"

#include <stdlib.h>

@protocol CAAnimationDelegate <NSObject>
@optional
- (void)animationDidStart:(CAAnimation *)anim;
- (void)animationDidStop:(CAAnimation *)anim finished:(BOOL)flag;
@end

typedef enum {
    kCAAnimationLinear,
    kCAAnimationDiscrete,
    kCAAnimationEaseIn,
    kCAAnimationEaseOut,
    kCAAnimationEaseInOut,
} CAAnimationTimingFunction;

/* ------------------------------------------------------------------ */
/* Interface                                                           */
/* ------------------------------------------------------------------ */

@interface CAAnimation : NSObject {
@protected
    double                     _duration;
    float                      _speed;
    double                     _beginTime;
    float                      _repeatCount;
    double                     _repeatDuration;
    BOOL                       _autoreverses;
    BOOL                       _removedOnCompletion;
    CAAnimationTimingFunction  _timing;
    id<CAAnimationDelegate>    _delegate;
    NSString                  *_keyPath;
}
- (double)duration;
- (void)setDuration:(double)d;
- (float)speed;
- (void)setSpeed:(float)s;
- (double)beginTime;
- (void)setBeginTime:(double)t;
- (float)repeatCount;
- (void)setRepeatCount:(float)c;
- (BOOL)autoreverses;
- (void)setAutoreverses:(BOOL)a;
- (BOOL)removedOnCompletion;
- (void)setRemovedOnCompletion:(BOOL)r;
- (CAAnimationTimingFunction)timingFunction;
- (void)setTimingFunction:(CAAnimationTimingFunction)t;
- (id<CAAnimationDelegate>)delegate;
- (void)setDelegate:(id<CAAnimationDelegate>)delegate;
- (NSString *)keyPath;
- (void)setKeyPath:(NSString *)keyPath;
- (BOOL)isAdditive;
- (void)setAdditive:(BOOL)additive;
@end

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation CAAnimation

- (id)init {
    self = [super init];
    if (!self) return nil;
    _duration            = 0.25;     /* default iOS duration        */
    _speed               = 1.0f;
    _beginTime           = 0.0;
    _repeatCount         = 0.0f;
    _repeatDuration      = 0.0;
    _autoreverses        = NO;
    _removedOnCompletion = YES;
    _timing              = kCAAnimationEaseInOut;
    return self;
}

- (void)dealloc {
    if (_delegate) [(id)_delegate release];
    if (_keyPath)  [_keyPath release];
    [super dealloc];
}

- (double)duration              { return _duration; }
- (void)setDuration:(double)d    { _duration = d; }
- (float)speed                   { return _speed; }
- (void)setSpeed:(float)s        { _speed = s; }
- (double)beginTime              { return _beginTime; }
- (void)setBeginTime:(double)t   { _beginTime = t; }
- (float)repeatCount             { return _repeatCount; }
- (void)setRepeatCount:(float)c  { _repeatCount = c; }
- (BOOL)autoreverses             { return _autoreverses; }
- (void)setAutoreverses:(BOOL)a  { _autoreverses = a; }
- (BOOL)removedOnCompletion      { return _removedOnCompletion; }
- (void)setRemovedOnCompletion:(BOOL)r { _removedOnCompletion = r; }
- (CAAnimationTimingFunction)timingFunction { return _timing; }
- (void)setTimingFunction:(CAAnimationTimingFunction)t { _timing = t; }
- (id<CAAnimationDelegate>)delegate { return _delegate; }
- (void)setDelegate:(id<CAAnimationDelegate>)delegate {
    if (_delegate) [(id)_delegate release];
    _delegate = [delegate retain];
}
- (NSString *)keyPath            { return _keyPath; }
- (void)setKeyPath:(NSString *)keyPath {
    if (_keyPath) [_keyPath release];
    _keyPath = [keyPath retain];
}
- (BOOL)isAdditive               { return NO; }
- (void)setAdditive:(BOOL)additive { (void)additive; }

+ (id)animation {
    return [[[CAAnimation alloc] init] autorelease];
}

@end
