/**
 * @file AVPlayer.m
 * @brief AVPlayer: play, pause, seek.
 *
 * AfriOS delegates actual playback to the host media backend; this
 * stub records the current state and notifies its delegate of state
 * changes.
 */

#import "afros_apple.h"

#include <stdlib.h>

@class AVAsset, AVPlayerItem;

@protocol AVPlayerDelegate <NSObject>
@optional
- (void)playerDidPlayToEnd:(AVPlayer *)player;
@end

typedef enum {
    AVPlayerStatusUnknown,
    AVPlayerStatusReadyToPlay,
    AVPlayerStatusFailed,
} AVPlayerStatus;

/* ------------------------------------------------------------------ */
/* Interface                                                           */
/* ------------------------------------------------------------------ */

@interface AVPlayer : NSObject {
@protected
    AVAsset     *_asset;
    AVPlayerItem *_currentItem;
    AVPlayerStatus _status;
    float        _rate;
    id<AVPlayerDelegate> _delegate;
}
+ (id)playerWithURL:(const char *)url;
+ (id)playerWithPlayerItem:(AVPlayerItem *)item;
- (id)initWithURL:(const char *)url;
- (id)initWithPlayerItem:(AVPlayerItem *)item;
- (AVPlayerStatus)status;
- (float)rate;
- (void)play;
- (void)pause;
- (void)seekToTime:(double)seconds;
- (void)replaceCurrentItemWithPlayerItem:(AVPlayerItem *)item;
- (id<AVPlayerDelegate>)delegate;
- (void)setDelegate:(id<AVPlayerDelegate>)delegate;
@end

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation AVPlayer

+ (id)playerWithURL:(const char *)url {
    return [[[AVPlayer alloc] initWithURL:url] autorelease];
}

+ (id)playerWithPlayerItem:(AVPlayerItem *)item {
    return [[[AVPlayer alloc] initWithPlayerItem:item] autorelease];
}

- (id)initWithURL:(const char *)url {
    (void)url;
    self = [super init];
    if (!self) return nil;
    _status = AVPlayerStatusUnknown;
    _rate   = 0.0f;
    return self;
}

- (id)initWithPlayerItem:(AVPlayerItem *)item {
    (void)item;
    self = [super init];
    if (!self) return nil;
    _status = AVPlayerStatusUnknown;
    _rate   = 0.0f;
    return self;
}

- (void)dealloc {
    if (_asset)        [_asset release];
    if (_currentItem)  [_currentItem release];
    if (_delegate)     [(id)_delegate release];
    [super dealloc];
}

- (AVPlayerStatus)status { return _status; }
- (float)rate            { return _rate; }

- (void)play {
    _rate = 1.0f;
    _status = AVPlayerStatusReadyToPlay;
}

- (void)pause {
    _rate = 0.0f;
}

- (void)seekToTime:(double)seconds {
    (void)seconds;
    /* Real impl: host media backend seeks the current stream.       */
}

- (void)replaceCurrentItemWithPlayerItem:(AVPlayerItem *)item {
    if (_currentItem) [_currentItem release];
    _currentItem = [item retain];
    _status = AVPlayerStatusUnknown;
}

- (id<AVPlayerDelegate>)delegate { return _delegate; }
- (void)setDelegate:(id<AVPlayerDelegate>)delegate {
    if (_delegate) [(id)_delegate release];
    _delegate = [delegate retain];
}

@end
