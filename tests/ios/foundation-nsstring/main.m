/*
 * foundation-nsstring/main.m — Test iOS/macOS #2 : NSString operations.
 *
 * Valide stringWithFormat, length, characterAtIndex,
 * stringByAppendingString via la Foundation émulée par
 * afros-incompat-engine.
 */

#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSString *base = [NSString stringWithFormat:@"Hello, %@!",
                          @"AfriOS"];
        NSUInteger len = [base length];
        if (len != 13) {
            printf("FAIL: length=%lu expected=13\n",
                   (unsigned long)len);
            return 1;
        }

        unichar c = [base characterAtIndex:7];
        if (c != 'A') {
            printf("FAIL: charAtIndex(7)=%d expected=%d\n",
                   c, 'A');
            return 1;
        }

        NSString *full = [base stringByAppendingString:@" — OK"];
        NSLog(@"%@", full);

        printf("%s\n", [base UTF8String]);
        fflush(stdout);
    }
    return 0;
}
