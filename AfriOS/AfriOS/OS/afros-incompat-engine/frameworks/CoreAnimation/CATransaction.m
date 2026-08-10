/**
 * @file CATransaction.m
 * @brief Implicit animation transaction: begin/commit, setAnimationDuration.
 *
 * AfriOS batches CALayer property changes inside a transaction and
 * hands them to the compositor on -commit. Nested transactions are
 * supported via a per-thread stack.
 */

#import "afros_apple.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define AFROS_CATRANS_MAX_DEPTH 16

typedef struct {
    double duration;
    BOOL   disable_actions;
    BOOL   in_use;
} catrans_frame_t;

static __thread catrans_frame_t g_stack[AFROS_CATRANS_MAX_DEPTH];
static __thread int g_top = 0;

/* ------------------------------------------------------------------ */
/* Interface                                                           */
/* ------------------------------------------------------------------ */

@interface CATransaction : NSObject
+ (void)begin;
+ (void)commit;
+ (void)flush;
+ (double)animationDuration;
+ (void)setAnimationDuration:(double)d;
+ (BOOL)disableActions;
+ (void)setDisableActions:(BOOL)disable;
+ (void)setCompletionBlock:(id)block;
+ (void)setAnimationTimingFunction:(id)function;
@end

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation CATransaction

+ (void)begin {
    if (g_top >= AFROS_CATRANS_MAX_DEPTH) return;
    /* Inherit values from the parent transaction.                   */
    if (g_top > 0) {
        g_stack[g_top] = g_stack[g_top - 1];
    } else {
        g_stack[g_top].duration        = 0.25;
        g_stack[g_top].disable_actions = NO;
    }
    g_stack[g_top].in_use = YES;
    g_top++;
}

+ (void)commit {
    if (g_top <= 0) return;
    g_top--;
    if (g_top == 0) {
        /* Top-level commit: pass the batched changes to the         */
        /* compositor. Real impl would invoke -display for dirty    */
        /* layers.                                                    */
    }
}

+ (void)flush {
    /* Commit any open transactions.                                 */
    while (g_top > 0) [CATransaction commit];
}

+ (double)animationDuration {
    if (g_top == 0) return 0.25;
    return g_stack[g_top - 1].duration;
}

+ (void)setAnimationDuration:(double)d {
    if (g_top == 0) [CATransaction begin];
    g_stack[g_top - 1].duration = d;
}

+ (BOOL)disableActions {
    if (g_top == 0) return NO;
    return g_stack[g_top - 1].disable_actions;
}

+ (void)setDisableActions:(BOOL)disable {
    if (g_top == 0) [CATransaction begin];
    g_stack[g_top - 1].disable_actions = disable;
}

+ (void)setCompletionBlock:(id)block {
    (void)block;
    /* Real impl would invoke the block on the main thread after    */
    /* the next commit.                                               */
}

+ (void)setAnimationTimingFunction:(id)function {
    (void)function;
    /* Stored alongside duration in the real impl.                  */
}

+ (NSUInteger)currentDepth {
    return (NSUInteger)g_top;
}

+ (void)reset {
    g_top = 0;
    memset(g_stack, 0, sizeof g_stack);
}

@end
