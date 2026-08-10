/**
 * @file CALayer.m
 * @brief CoreAnimation layer: frame, bounds, position, sublayers,
 *        drawInContext:.
 *
 * Layers are the rendering primitive used by Core Animation. AfriOS
 * layers carry geometry and a list of sublayers; compositing is done
 * by the host compositor.
 */

#import "afros_apple.h"

#include <stdlib.h>
#include <string.h>

@class CALayer, CAAnimation;

/* AfriOS does not ship NSMutableArray. We use a small fixed-size    */
/* C array of sublayers and a count instead.                          */
#define AFROS_LAYER_MAX_SUBLAYERS 32

/* ------------------------------------------------------------------ */
/* Interface                                                           */
/* ------------------------------------------------------------------ */

@interface CALayer : NSObject {
@protected
    CALayer     *_superlayer;
    CALayer     *_sublayers[AFROS_LAYER_MAX_SUBLAYERS];
    NSUInteger  _sublayer_count;
    CGRect       _frame;
    CGRect       _bounds;
    CGPoint      _position;
    CGPoint      _anchorPoint;
    float        _opacity;
    BOOL         _hidden;
    BOOL         _doubleSided;
    BOOL         _masksToBounds;
    CGColor     *_backgroundColor;
    NSString    *_name;
}
- (id)init;
- (CGRect)frame;
- (void)setFrame:(CGRect)frame;
- (CGRect)bounds;
- (void)setBounds:(CGRect)bounds;
- (CGPoint)position;
- (void)setPosition:(CGPoint)position;
- (CGPoint)anchorPoint;
- (void)setAnchorPoint:(CGPoint)anchorPoint;
- (float)opacity;
- (void)setOpacity:(float)opacity;
- (BOOL)isHidden;
- (void)setHidden:(BOOL)hidden;
- (BOOL)masksToBounds;
- (void)setMasksToBounds:(BOOL)masksToBounds;
- (CALayer *)superlayer;
- (NSArray *)sublayers;
- (void)addSublayer:(CALayer *)layer;
- (void)removeFromSuperlayer;
- (CGColor *)backgroundColor;
- (void)setBackgroundColor:(CGColor *)color;
- (void)setNeedsDisplay;
- (void)setNeedsLayout;
- (void)drawInContext:(CGContext *)ctx;
- (void)layoutSublayers;
@end

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation CALayer

- (id)init {
    self = [super init];
    if (!self) return nil;
    _anchorPoint = CGPointMake(0.5f, 0.5f);
    _opacity     = 1.0f;
    _hidden      = NO;
    _doubleSided = YES;
    _masksToBounds = NO;
    return self;
}

- (void)dealloc {
    for (NSUInteger i = 0; i < _sublayer_count; i++) {
        [_sublayers[i] release];
    }
    if (_backgroundColor)  CGColorRelease(_backgroundColor);
    if (_name)             [_name release];
    [super dealloc];
}

- (CGRect)frame    { return _frame; }
- (void)setFrame:(CGRect)frame {
    _frame = frame;
    _bounds.origin = CGPointMake(0, 0);
    _bounds.size   = frame.size;
    _position = CGPointMake(frame.origin.x + frame.size.width * _anchorPoint.x,
                            frame.origin.y + frame.size.height * _anchorPoint.y);
}

- (CGRect)bounds    { return _bounds; }
- (void)setBounds:(CGRect)bounds { _bounds = bounds; }

- (CGPoint)position { return _position; }
- (void)setPosition:(CGPoint)position {
    _position = position;
    _frame.origin.x = position.x - _bounds.size.width * _anchorPoint.x;
    _frame.origin.y = position.y - _bounds.size.height * _anchorPoint.y;
}

- (CGPoint)anchorPoint    { return _anchorPoint; }
- (void)setAnchorPoint:(CGPoint)anchorPoint {
    _anchorPoint = anchorPoint;
}

- (float)opacity         { return _opacity; }
- (void)setOpacity:(float)o { _opacity = o; }
- (BOOL)isHidden         { return _hidden; }
- (void)setHidden:(BOOL)h { _hidden = h; }
- (BOOL)masksToBounds    { return _masksToBounds; }
- (void)setMasksToBounds:(BOOL)m { _masksToBounds = m; }

- (CALayer *)superlayer  { return _superlayer; }
- (NSArray *)sublayers   { return nil; /* not exposed; iterate via sublayerCount */ }
- (NSUInteger)sublayerCount { return _sublayer_count; }
- (CALayer *)sublayerAtIndex:(NSUInteger)i {
    return i < _sublayer_count ? _sublayers[i] : nil;
}

- (void)addSublayer:(CALayer *)layer {
    if (!layer) return;
    if (_sublayer_count >= AFROS_LAYER_MAX_SUBLAYERS) return;
    _sublayers[_sublayer_count++] = [layer retain];
    layer->_superlayer = self;
}

- (void)removeFromSuperlayer {
    if (_superlayer) {
        CALayer *parent = _superlayer;
        for (NSUInteger i = 0; i < parent->_sublayer_count; i++) {
            if (parent->_sublayers[i] == self) {
                [parent->_sublayers[i] release];
                parent->_sublayers[i] = parent->_sublayers[parent->_sublayer_count - 1];
                parent->_sublayer_count--;
                break;
            }
        }
    }
    _superlayer = nil;
}

- (CGColor *)backgroundColor { return _backgroundColor; }
- (void)setBackgroundColor:(CGColor *)color {
    if (_backgroundColor) CGColorRelease(_backgroundColor);
    _backgroundColor = color ? CGColorRetain(color) : NULL;
}

- (void)setNeedsDisplay { /* mark dirty */ }
- (void)setNeedsLayout  { /* mark layout dirty */ }

- (void)drawInContext:(CGContext *)ctx {
    if (ctx && _backgroundColor) {
        CGContextSetFillColor(ctx, _backgroundColor);
        CGContextFillRect(ctx, _bounds);
    }
}

- (void)layoutSublayers {
    /* Subclasses override to position sublayers.                   */
}

@end
