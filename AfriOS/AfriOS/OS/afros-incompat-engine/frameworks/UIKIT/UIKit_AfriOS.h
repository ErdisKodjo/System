/**
 * @file UIKit_AfriOS.h
 * @brief Umbrella declarations for the AfriOS UIKit stub framework.
 *
 * Declares the @interface blocks for every UIKit class implemented
 * in this directory. The matching @implementation blocks live in
 * the .m files alongside this header.
 */

#import "afros_apple.h"
#import "../Foundation/Foundation_AfriOS.h"

@class UIEvent, NSString, NSArray;
@class UIApplication, UIView, UIWindow, UIViewController, UIControl;

/* ------------------------------------------------------------------ */
/* UIApplication                                                       */
/* ------------------------------------------------------------------ */

@protocol UIApplicationDelegate <NSObject>
@optional
- (void)applicationDidFinishLaunching:(UIApplication *)app;
- (void)applicationWillResignActive:(UIApplication *)app;
- (void)applicationDidBecomeActive:(UIApplication *)app;
- (void)applicationWillTerminate:(UIApplication *)app;
- (BOOL)application:(UIApplication *)app
        didFinishLaunchingWithOptions:(id)opts;
@end

@interface UIApplication : NSObject {
@protected
    id<UIApplicationDelegate> _delegate;
    UIWindow   *_keyWindow;
    BOOL        _running;
    BOOL        _background;
}
+ (UIApplication *)sharedApplication;
- (id<UIApplicationDelegate>)delegate;
- (void)setDelegate:(id<UIApplicationDelegate>)delegate;
- (UIWindow *)keyWindow;
- (void)setKeyWindow:(UIWindow *)window;
- (BOOL)isRunning;
- (void)run;
- (void)terminate;
- (void)sendEvent:(UIEvent *)event;
- (void)beginBackgroundTaskWithExpirationHandler:(id)handler;
- (void)endBackgroundTask:(NSUInteger)identifier;
- (NSString *)applicationStateString;
@end

/* ------------------------------------------------------------------ */
/* UIView                                                              */
/* ------------------------------------------------------------------ */

@interface UIView : NSObject {
@protected
    CGRect      _frame;
    CGRect      _bounds;
    UIView     *_superview;
    NSArray    *_subviews;
    UIWindow   *_window;
    BOOL        _hidden;
    BOOL        _opaque;
    float       _alpha;
    NSUInteger  _tag;
}
- (id)initWithFrame:(CGRect)frame;
- (CGRect)frame;
- (void)setFrame:(CGRect)frame;
- (CGRect)bounds;
- (void)setBounds:(CGRect)bounds;
- (CGPoint)center;
- (void)setCenter:(CGPoint)center;
- (UIView *)superview;
- (NSArray *)subviews;
- (void)addSubview:(UIView *)view;
- (void)removeFromSuperview;
- (UIWindow *)window;
- (BOOL)isHidden;
- (void)setHidden:(BOOL)hidden;
- (float)alpha;
- (void)setAlpha:(float)alpha;
- (NSUInteger)tag;
- (void)setTag:(NSUInteger)tag;
- (void)setNeedsDisplay;
- (void)setNeedsLayout;
- (void)drawRect:(CGRect)rect;
- (void)layoutSubviews;
- (UIView *)viewWithTag:(NSUInteger)tag;
@end

/* ------------------------------------------------------------------ */
/* UIWindow                                                            */
/* ------------------------------------------------------------------ */

@interface UIWindow : UIView {
@protected
    UIViewController *_rootViewController;
    BOOL              _keyWindow;
    BOOL              _visible;
}
- (id)initWithFrame:(CGRect)frame;
- (void)makeKeyAndVisible;
- (void)makeKeyWindow;
- (BOOL)isKeyWindow;
- (UIViewController *)rootViewController;
- (void)setRootViewController:(UIViewController *)vc;
- (void)sendEvent:(UIEvent *)event;
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event;
- (BOOL)becomeFirstResponder;
- (BOOL)resignFirstResponder;
@end

/* ------------------------------------------------------------------ */
/* UIViewController                                                   */
/* ------------------------------------------------------------------ */

@interface UIViewController : NSObject {
@protected
    UIView              *_view;
    UIViewController    *_parentViewController;
    NSArray             *_childViewControllers;
    NSString            *_title;
    BOOL                 _appeared;
    BOOL                 _editing;
}
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil;
- (void)loadView;
- (UIView *)view;
- (void)setView:(UIView *)view;
- (NSString *)title;
- (void)setTitle:(NSString *)title;
- (UIViewController *)parentViewController;
- (NSArray *)childViewControllers;
- (void)addChildViewController:(UIViewController *)child;
- (void)removeFromParentViewController;
- (void)viewDidLoad;
- (void)viewWillAppear:(BOOL)animated;
- (void)viewDidAppear:(BOOL)animated;
- (void)viewWillDisappear:(BOOL)animated;
- (void)viewDidDisappear:(BOOL)animated;
- (void)viewWillLayoutSubviews;
- (void)viewDidLayoutSubviews;
- (BOOL)isEditing;
- (void)setEditing:(BOOL)editing;
@end

/* ------------------------------------------------------------------ */
/* UIControl                                                           */
/* ------------------------------------------------------------------ */

typedef NSUInteger UIControlEvents;
enum {
    UIControlEventTouchDown         = 1 << 0,
    UIControlEventTouchDownRepeat   = 1 << 1,
    UIControlEventTouchDragInside   = 1 << 2,
    UIControlEventTouchDragOutside  = 1 << 3,
    UIControlEventTouchDragEnter    = 1 << 4,
    UIControlEventTouchDragExit     = 1 << 5,
    UIControlEventTouchUpInside     = 1 << 6,
    UIControlEventTouchUpOutside    = 1 << 7,
    UIControlEventTouchCancel       = 1 << 8,
    UIControlEventValueChanged      = 1 << 12,
    UIControlEventEditingDidBegin   = 1 << 16,
    UIControlEventEditingChanged    = 1 << 17,
    UIControlEventEditingDidEnd     = 1 << 18,
    UIControlEventEditingDidEndOnExit= 1 << 19,
    UIControlEventAllTouchEvents    = 0x00000FFF,
    UIControlEventAllEditingEvents  = 0x000F0000,
    UIControlEventAllEvents         = 0xFFFFFFFF,
};

#define AFROS_UICTRL_MAX_TARGETS 16

typedef struct {
    id          target;
    SEL         action;
    NSUInteger  events;
} ui_target_action_t;

@interface UIControl : UIView {
@protected
    ui_target_action_t _targets[AFROS_UICTRL_MAX_TARGETS];
    NSUInteger         _target_count;
    BOOL               _enabled;
    BOOL               _selected;
    BOOL               _highlighted;
}
- (void)addTarget:(id)target action:(SEL)action forControlEvents:(UIControlEvents)events;
- (void)removeTarget:(id)target action:(SEL)action forControlEvents:(UIControlEvents)events;
- (void)sendAction:(SEL)action to:(id)target forEvent:(UIEvent *)event;
- (void)sendActionsForControlEvents:(UIControlEvents)events;
- (BOOL)isEnabled;
- (void)setEnabled:(BOOL)enabled;
- (BOOL)isSelected;
- (void)setSelected:(BOOL)selected;
- (BOOL)isHighlighted;
- (void)setHighlighted:(BOOL)highlighted;
@end
