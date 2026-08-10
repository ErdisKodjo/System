/*
 * coreanimation-bounce/main.m — Test iOS/macOS #4 : animation CALayer.
 *
 * Crée un CALayer, applique une animation de position (bounce),
 * commit la transaction, vérifie que ça ne crash pas.
 */

#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        CALayer *layer = [CALayer layer];
        layer.frame = CGRectMake(0, 0, 100, 100);

        CABasicAnimation *bounce = [CABasicAnimation
            animationWithKeyPath:@"position"];
        bounce.fromValue = [NSValue valueWithCGPoint:CGPointMake(50, 50)];
        bounce.toValue   = [NSValue valueWithCGPoint:CGPointMake(50, 200)];
        bounce.duration  = 0.5;
        [layer addAnimation:bounce forKey:@"bounce"];

        [CATransaction begin];
        [CATransaction setAnimationDuration:0.5];
        layer.position = CGPointMake(50, 200);
        [CATransaction commit];

        printf("Hello, AfriOS!\n");
        fflush(stdout);
    }
    return 0;
}
