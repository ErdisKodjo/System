/**
 * @file AVAudioSession.m
 * @brief AVAudioSession: category, mode, active.
 *
 * AfriOS exposes a single global session backed by the host audio
 * subsystem; categories that imply routing (e.g. AirPlay) are mapped
 * to the default route.
 */

#import "afros_apple.h"

#include <stdlib.h>
#include <string.h>

/* Well-known category constants (subset).                            */
static const char *const AVAudioSessionCategoryAmbient       = "AVAudioSessionCategoryAmbient";
static const char *const AVAudioSessionCategorySoloAmbient   = "AVAudioSessionCategorySoloAmbient";
static const char *const AVAudioSessionCategoryPlayback      = "AVAudioSessionCategoryPlayback";
static const char *const AVAudioSessionCategoryRecord        = "AVAudioSessionCategoryRecord";
static const char *const AVAudioSessionCategoryPlayAndRecord = "AVAudioSessionCategoryPlayAndRecord";

typedef enum {
    AVAudioSessionCategoryPlaybackModeDefault,
    AVAudioSessionCategoryPlaybackModeSpokenAudio,
    AVAudioSessionCategoryPlaybackModeMovieRecording,
} AVAudioSessionMode;

/* ------------------------------------------------------------------ */
/* Interface                                                           */
/* ------------------------------------------------------------------ */

@interface AVAudioSession : NSObject {
@protected
    const char *_category;
    AVAudioSessionMode _mode;
    BOOL        _active;
    BOOL        _otherAudioPlaying;
}
+ (AVAudioSession *)sharedInstance;
- (const char *)category;
- (BOOL)setCategory:(const char *)category error:(id *)error;
- (AVAudioSessionMode)mode;
- (BOOL)setMode:(AVAudioSessionMode)mode error:(id *)error;
- (BOOL)isActive;
- (BOOL)setActive:(BOOL)active error:(id *)error;
- (BOOL)isOtherAudioPlaying;
@end

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

static AVAudioSession *g_shared = nil;

@implementation AVAudioSession

+ (AVAudioSession *)sharedInstance {
    if (!g_shared) {
        g_shared = [[AVAudioSession alloc] init];
    }
    return g_shared;
}

- (id)init {
    self = [super init];
    if (!self) return nil;
    _category           = AVAudioSessionCategorySoloAmbient;
    _mode               = AVAudioSessionCategoryPlaybackModeDefault;
    _active             = NO;
    _otherAudioPlaying  = NO;
    return self;
}

- (void)dealloc {
    if (g_shared == self) g_shared = nil;
    [super dealloc];
}

- (const char *)category { return _category; }

- (BOOL)setCategory:(const char *)category error:(id *)error {
    (void)error;
    if (!category) return NO;
    /* Validate against known categories.                             */
    if (strcmp(category, AVAudioSessionCategoryAmbient) == 0 ||
        strcmp(category, AVAudioSessionCategorySoloAmbient) == 0 ||
        strcmp(category, AVAudioSessionCategoryPlayback) == 0 ||
        strcmp(category, AVAudioSessionCategoryRecord) == 0 ||
        strcmp(category, AVAudioSessionCategoryPlayAndRecord) == 0) {
        _category = category;
        return YES;
    }
    return NO;
}

- (AVAudioSessionMode)mode { return _mode; }

- (BOOL)setMode:(AVAudioSessionMode)mode error:(id *)error {
    (void)error;
    _mode = mode;
    return YES;
}

- (BOOL)isActive { return _active; }

- (BOOL)setActive:(BOOL)active error:(id *)error {
    (void)error;
    _active = active;
    return YES;
}

- (BOOL)isOtherAudioPlaying { return _otherAudioPlaying; }

@end
