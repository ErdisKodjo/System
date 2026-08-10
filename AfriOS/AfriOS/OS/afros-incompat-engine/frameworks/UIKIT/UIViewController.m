/**
 * @file UIViewController.m
 * @brief UIViewController: view, viewDidLoad, viewWillAppear, lifecycle.
 */

#import "afros_apple.h"
#import "UIKit_AfriOS.h"

#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Implementation                                                      */
/* ------------------------------------------------------------------ */

@implementation UIViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    (void)nibNameOrNil; (void)nibBundleOrNil;
    self = [super init];
    if (!self) return nil;
    _childViewControllers = [[NSArray alloc] init];
    _appeared = NO;
    _editing  = NO;
    return self;
}

- (id)init {
    return [self initWithNibName:nil bundle:nil];
}

- (void)dealloc {
    if (_view)                  [_view release];
    if (_childViewControllers)  [_childViewControllers release];
    if (_title)                 [_title release];
    [super dealloc];
}

- (void)loadView {
    /* Subclasses override to instantiate the view hierarchy.         */
    if (!_view) {
        _view = [[UIView alloc] initWithFrame:CGRectMake(0, 0, 320, 480)];
    }
}

- (UIView *)view {
    if (!_view) [self loadView];
    return _view;
}

- (void)setView:(UIView *)view {
    if (_view) [_view release];
    _view = [view retain];
}

- (NSString *)title { return _title; }
- (void)setTitle:(NSString *)title {
    if (_title) [_title release];
    _title = [title retain];
}

- (UIViewController *)parentViewController { return _parentViewController; }
- (NSArray *)childViewControllers { return _childViewControllers; }

- (void)addChildViewController:(UIViewController *)child {
    (void)child;
    /* In real UIKit we'd rebuild the array. Stub no-ops.             */
}

- (void)removeFromParentViewController {
    _parentViewController = nil;
}

- (void)viewDidLoad {
    /* Subclasses override to perform additional setup.              */
}

- (void)viewWillAppear:(BOOL)animated {
    (void)animated;
    if (!_appeared) {
        _appeared = YES;
        [self viewDidLoad];
    }
}

- (void)viewDidAppear:(BOOL)animated {
    (void)animated;
}

- (void)viewWillDisappear:(BOOL)animated {
    (void)animated;
}

- (void)viewDidDisappear:(BOOL)animated {
    (void)animated;
    _appeared = NO;
}

- (void)viewWillLayoutSubviews { /* override point */ }
- (void)viewDidLayoutSubviews  { /* override point */ }

- (BOOL)isEditing         { return _editing; }
- (void)setEditing:(BOOL)e { _editing = e; }

@end
