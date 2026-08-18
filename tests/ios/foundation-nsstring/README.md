# ios/foundation-nsstring

Test des opérations NSString de Foundation.

## Ce que fait l'app

1. `[NSString stringWithFormat:@"Hello, %@!", @"AfriOS"]`.
2. `[base length]` → doit valoir 13.
3. `[base characterAtIndex:7]` → doit valoir `'A'`.
4. `[base stringByAppendingString:@" — OK"]`.
5. Affiche le résultat.

## Ce qu'on valide

- L'implémentation `Foundation/NSString.m` de `afros-incompat-engine`.
- `stringWithFormat:` (variadique, parsing `%@`).
- `length` / `characterAtIndex:` (accès UTF-16).
- `stringByAppendingString:` (concaténation).
- Le runtime ARC (`arc_implementation.c`) ne fuit pas les références.

## Build

```bash
clang -arch arm64-apple-darwin -framework Foundation \
      -o foundation-nsstring main.m
```
