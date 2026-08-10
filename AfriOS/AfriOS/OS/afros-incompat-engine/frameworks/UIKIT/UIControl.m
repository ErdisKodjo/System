/**
 * @file UIControl.m
 * @brief UIControl: addTarget:action:forControlEvents:, sendAction:.
 */

#import "afros_apple.h"
#import "UIKit_AfriOS.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation UIControl

- (id)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    memset(_targets, 0, sizeof _targets);
    _target_count = 0;
    _enabled      = YES;
    _selected     = NO;
    _highlighted  = NO;
    return self;
}

- (void)dealloc {
    for (NSUInteger i = 0; i < _target_count; i++) {
        if (_targets[i].target) [_targets[i].target release];
    }
    [super dealloc];
}

- (void)addTarget:(id)target action:(SEL)action forControlEvents:(UIControlEvents)events {
    if (!target || !action || _target_count >= AFROS_UICTRL_MAX_TARGETS) return;
    /* If the pair already exists, OR in the events mask.            */
    for (NSUInteger i = 0; i < _target_count; i++) {
        if (_targets[i].target == target && _targets[i].action == action) {
            _targets[i].events |= events;
            return;
        }
    }
    _targets[_target_count].target = [target retain];
    _targets[_target_count].action = action;
    _targets[_target_count].events = events;
    _target_count++;
}

- (void)removeTarget:(id)target action:(SEL)action forControlEvents:(UIControlEvents)events {
    for (NSUInteger i = 0; i < _target_count; ) {
        BOOL match_target = (target == nil || _targets[i].target == target);
        BOOL match_action = (action == NULL || _targets[i].action == action);
        if (match_target && match_action) {
            _targets[i].events &= ~events;
            if (_targets[i].events == 0) {
                if (_targets[i].target) [_targets[i].target release];
                _targets[i] = _targets[--_target_count];
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
}

- (void)sendAction:(SEL)action to:(id)target forEvent:(UIEvent *)event {
    (void)event;
    if (target && action) {
        /* The runtime's objc_msg_send is too primitive to dispatch   */
        /* here; in real UIKit the UIApplication forwards via         */
        /* -[NSObject performSelector:withObject:].                  */
        [target performSelector:action withObject:self];
    }
}

- (void)sendActionsForControlEvents:(UIControlEvents)events {
    for (NSUInteger i = 0; i < _target_count; i++) {
        if (_targets[i].events & events) {
            [self sendAction:_targets[i].action
                          to:_targets[i].target
                   forEvent:nil];
        }
    }
}

- (BOOL)isEnabled    { return _enabled; }
- (void)setEnabled:(BOOL)e       { _enabled = e; }
- (BOOL)isSelected   { return _selected; }
- (void)setSelected:(BOOL)s      { _selected = s; }
- (BOOL)isHighlighted { return _highlighted; }
- (void)setHighlighted:(BOOL)h   { _highlighted = h; }

@end
