/*
 * hello-app/main.m — Test iOS/macOS #1 : bundle .app Hello World.
 *
 * Bundle .app minimal avec un exécutable Mach-O qui printf
 * "Hello, AfriOS!". Valide le Mach-O loader et la runtime ObjC
 * de afros-incompat-engine (couche Darling).
 */

#import <Foundation/Foundation.h>

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSLog(@"Hello, AfriOS!");
        /* Aussi sur stdout pour que le harness puisse capturer. */
        printf("Hello, AfriOS!\n");
        fflush(stdout);
    }
    return 0;
}
