/**
 * @file AVAsset.m
 * @brief AVAsset: media asset with tracks and duration.
 */

#import "afros_apple.h"

#include <stdlib.h>
#include <string.h>

@class AVAssetTrack;

/* ------------------------------------------------------------------ */
/* Interface                                                           */
/* ------------------------------------------------------------------ */

@interface AVAsset : NSObject {
@protected
    char         _url[1024];
    double       _duration;
    NSArray     *_tracks;
    BOOL         _isPlayable;
    BOOL         _isExportable;
}
+ (id)assetWithURL:(const char *)url;
- (id)initWithURL:(const char *)url;
- (const char *)url;
- (double)duration;
- (NSArray *)tracks;
- (NSArray *)tracksWithMediaType:(const char *)mediaType;
- (BOOL)isPlayable;
- (BOOL)isExportable;
@end

/* ------------------------------------------------------------------ */
/* AVAssetTrack                                                        */
/* ------------------------------------------------------------------ */

@interface AVAssetTrack : NSObject {
@protected
    int          _trackID;
    char         _mediaType[32];
    double       _duration;
    CGSize       _naturalSize;
    float        _preferredVolume;
}
- (int)trackID;
- (const char *)mediaType;
- (double)duration;
- (CGSize)naturalSize;
- (float)preferredVolume;
@end

@implementation AVAssetTrack
- (id)initWithTrackID:(int)tid mediaType:(const char *)mt {
    self = [super init];
    if (!self) return nil;
    _trackID = tid;
    if (mt) strncpy(_mediaType, mt, sizeof _mediaType - 1);
    _preferredVolume = 1.0f;
    return self;
}
- (int)trackID           { return _trackID; }
- (const char *)mediaType { return _mediaType; }
- (double)duration        { return _duration; }
- (CGSize)naturalSize     { return _naturalSize; }
- (float)preferredVolume  { return _preferredVolume; }
@end

/* ------------------------------------------------------------------ */
/* AVAsset                                                             */
/* ------------------------------------------------------------------ */

@implementation AVAsset

+ (id)assetWithURL:(const char *)url {
    return [[[AVAsset alloc] initWithURL:url] autorelease];
}

- (id)initWithURL:(const char *)url {
    self = [super init];
    if (!self) return nil;
    if (url) strncpy(_url, url, sizeof _url - 1);
    _duration     = 0.0;
    _tracks       = [[NSArray alloc] init];
    _isPlayable   = YES;
    _isExportable = NO;
    return self;
}

- (void)dealloc {
    if (_tracks) [_tracks release];
    [super dealloc];
}

- (const char *)url { return _url; }
- (double)duration  { return _duration; }
- (NSArray *)tracks { return _tracks; }

- (NSArray *)tracksWithMediaType:(const char *)mediaType {
    (void)mediaType;
    /* Real impl would filter _tracks by _mediaType. Stub: return all. */
    return _tracks;
}

- (BOOL)isPlayable   { return _isPlayable; }
- (BOOL)isExportable { return _isExportable; }

@end
