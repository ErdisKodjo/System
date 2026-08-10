/**
 * @file UIApplication.m
 * @brief UIApplication singleton: shared instance, delegate, run loop,
 *        event dispatch.
 */

#import "afros_apple.h"
#import "UIKit_AfriOS.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Static singleton                                                    */
/* ------------------------------------------------------------------ */

static UIApplication *g_shared_app = nil;

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation UIApplication

+ (UIApplication *)sharedApplication {
    if (!g_shared_app) {
        g_shared_app = [[UIApplication alloc] init];
    }
    return g_shared_app;
}

- (id)init {
    self = [super init];
    if (!self) return nil;
    _running    = NO;
    _background = NO;
    return self;
}

- (void)dealloc {
    if (g_shared_app == self) g_shared_app = nil;
    if (_delegate) [(id)_delegate release];
    [super dealloc];
}

- (id<UIApplicationDelegate>)delegate {
    return _delegate;
}

- (void)setDelegate:(id<UIApplicationDelegate>)delegate {
    if (_delegate) [(id)_delegate release];
    _delegate = [delegate retain];
}

- (UIWindow *)keyWindow { return _keyWindow; }

- (void)setKeyWindow:(UIWindow *)window {
    if (_keyWindow) [_keyWindow release];
    _keyWindow = [window retain];
}

- (BOOL)isRunning { return _running; }

- (void)run {
    _running = YES;
    if (_delegate && [_delegate respondsToSelector:@selector(applicationDidFinishLaunching:)]) {
        [_delegate applicationDidFinishLaunching:self];
    }
    /* AfriOS does not run a real run-loop here; the host OS's        */
    /* main loop is responsible for invoking -sendEvent: when input    */
    /* arrives.                                                        */
    while (_running) {
        /* In a real implementation we would call                    */
        /* CFRunLoopRunInMode(kCFRunLoopDefaultMode, ...).            */
        break;
    }
}

- (void)terminate {
    if (_delegate && [_delegate respondsToSelector:@selector(applicationWillTerminate:)]) {
        [_delegate applicationWillTerminate:self];
    }
    _running = NO;
}

- (void)sendEvent:(UIEvent *)event {
    (void)event;
    /* Dispatch the event to the key window.                          */
    if (_keyWindow) {
        /* [_keyWindow sendEvent:event]; */
    }
}

- (void)beginBackgroundTaskWithExpirationHandler:(id)handler {
    (void)handler;
    /* Background execution is not supported; accept the call.        */
}

- (void)endBackgroundTask:(NSUInteger)identifier {
    (void)identifier;
}

- (NSString *)applicationStateString {
    if (_background) return nil;  /* would return @"UIApplicationStateBackground" */
    return nil;
}

+ (BOOL)respondsToSelector:(SEL)sel {
    (void)sel;
    return NO;
}

@end
