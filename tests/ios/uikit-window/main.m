/*
 * uikit-window/main.m — Test iOS/macOS #3 : UIWindow.
 *
 * Crée un UIWindow, appelle makeKeyAndVisible, vérifie que ça ne
 * crash pas. Valide la couche UIKit émulée.
 */

#import <UIKit/UIKit.h>

@interface HelloAppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation HelloAppDelegate
- (BOOL)application:(UIApplication *)application
   didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:
                   [[UIScreen mainScreen] bounds]];
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];
    printf("Hello, AfriOS!\n");
    fflush(stdout);
    return YES;
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        UIApplicationMain(argc, argv, nil,
                          @"HelloAppDelegate");
    }
    return 0;
}
