# ios/hello-app

Test iOS/macOS minimal — bundle .app Hello World.

## Ce que fait l'app

- `main.m` : `@autoreleasepool { NSLog(...); printf("Hello, AfriOS!"); }`.
- Compilé en Mach-O arm64 (`clang -arch arm64-apple-darwin`).

## Ce qu'on valide

- Le `macho_parser.c` + `loader.c` de `afros-incompat-engine` chargent
  un Mach-O simple.
- La runtime ObjC (`objc_runtime.c`) initialise un `@autoreleasepool`.
- `NSLog` ne crash pas (redirigée vers stderr/stdout).
- `printf` via la libc Darwin fonctionne.

## Build

```bash
clang -arch arm64-apple-darwin -framework Foundation \
      -o hello-app main.m
```

Si `clang` cross-Darwin n'est pas installé, le binaire `hello-app` est
supposé précompilé. Le runtime Darling peut aussi charger un Mach-O
x86_64 si plus simple à produire.
