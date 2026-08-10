/**
 * @file UIView.m
 * @brief UIView: frame, bounds, superview/subviews, drawRect,
 *        layoutSubviews.
 */

#import "afros_apple.h"
#import "UIKit_AfriOS.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation UIView

- (id)initWithFrame:(CGRect)frame {
    self = [super init];
    if (!self) return nil;
    _frame     = frame;
    _bounds    = CGRectMake(0, 0, frame.size.width, frame.size.height);
    _subviews  = [[NSArray alloc] init];
    _hidden    = NO;
    _opaque    = NO;
    _alpha     = 1.0f;
    _tag       = 0;
    return self;
}

- (id)init {
    return [self initWithFrame:CGRectMake(0, 0, 0, 0)];
}

- (void)dealloc {
    if (_subviews) [_subviews release];
    [super dealloc];
}

- (CGRect)frame    { return _frame; }
- (void)setFrame:(CGRect)frame {
    _frame = frame;
    /* Keep bounds in sync with the new size.                        */
    _bounds.size = frame.size;
    [self setNeedsLayout];
}

- (CGRect)bounds    { return _bounds; }
- (void)setBounds:(CGRect)bounds {
    _bounds = bounds;
    [self setNeedsLayout];
}

- (CGPoint)center {
    CGPoint c;
    c.x = _frame.origin.x + _frame.size.width * 0.5f;
    c.y = _frame.origin.y + _frame.size.height * 0.5f;
    return c;
}
- (void)setCenter:(CGPoint)center {
    _frame.origin.x = center.x - _frame.size.width * 0.5f;
    _frame.origin.y = center.y - _frame.size.height * 0.5f;
}

- (UIView *)superview  { return _superview; }
- (NSArray *)subviews  { return _subviews; }
- (UIWindow *)window   { return _window; }

- (void)addSubview:(UIView *)view {
    if (!view) return;
    /* Real UIKit would build a new array; stub no-ops.              */
    [view retain];
    view->_superview = self;
    view->_window    = _window;
}

- (void)removeFromSuperview {
    if (_superview) {
        /* Would remove from _superview->_subviews.                   */
        _superview = nil;
    }
    [self release];
}

- (BOOL)isHidden         { return _hidden; }
- (void)setHidden:(BOOL)h { _hidden = h; }
- (float)alpha           { return _alpha; }
- (void)setAlpha:(float)a { _alpha = a; }
- (NSUInteger)tag         { return _tag; }
- (void)setTag:(NSUInteger)t { _tag = t; }

- (void)setNeedsDisplay { /* Mark dirty in the renderer. */ }
- (void)setNeedsLayout  { /* Mark layout dirty. */ }

- (void)drawRect:(CGRect)rect {
    (void)rect;
    /* Subclasses override to draw content.                          */
}

- (void)layoutSubviews {
    /* Subclasses override to position subviews.                     */
}

- (UIView *)viewWithTag:(NSUInteger)tag {
    if (_tag == tag) return self;
    /* Walk subviews.                                                 */
    if (_subviews) {
        NSUInteger n = [_subviews count];
        for (NSUInteger i = 0; i < n; i++) {
            UIView *child = [_subviews objectAtIndex:i];
            UIView *match = [child viewWithTag:tag];
            if (match) return match;
        }
    }
    return nil;
}

@end
