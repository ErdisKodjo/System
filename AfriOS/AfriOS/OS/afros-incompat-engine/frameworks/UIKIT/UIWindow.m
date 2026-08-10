/**
 * @file UIWindow.m
 * @brief UIWindow: makeKeyAndVisible, rootViewController, sendEvent.
 */

#import "afros_apple.h"
#import "UIKit_AfriOS.h"

#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation UIWindow

- (id)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    _keyWindow = NO;
    _visible   = NO;
    return self;
}

- (void)dealloc {
    if (_rootViewController) [_rootViewController release];
    [super dealloc];
}

- (void)makeKeyAndVisible {
    [self makeKeyWindow];
    _visible = YES;
    [self setHidden:NO];
}

- (void)makeKeyWindow {
    /* In a real UIKit we'd resign the previous key window.          */
    _keyWindow = YES;
}

- (BOOL)isKeyWindow { return _keyWindow; }

- (UIViewController *)rootViewController {
    return _rootViewController;
}

- (void)setRootViewController:(UIViewController *)vc {
    if (_rootViewController) [_rootViewController release];
    _rootViewController = [vc retain];
}

- (void)sendEvent:(UIEvent *)event {
    (void)event;
    /* Dispatch touch events to the appropriate hit-tested view.      */
    /* In the AfriOS stub this is delegated to the host compositor.    */
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event {
    (void)event;
    if (!CGRectContainsPoint([self bounds], point)) {
        return nil;
    }
    /* Stub: return self. Real impl walks subviews in reverse Z-order. */
    return self;
}

- (BOOL)becomeFirstResponder { return YES; }
- (BOOL)resignFirstResponder  { return YES; }

@end
