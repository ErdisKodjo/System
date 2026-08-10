# android/hello-apk

Test Android minimal — Hello World DEX.

## Ce que fait l'app

- `com.afrios.Hello.main()` : `System.out.println("Hello, AfriOS!")`
  + `Log.i("AfriOS", "Hello, AfriOS!")`.

## Ce qu'on valide

- Le chargeur DEX de `afros-androsandbox`.
- Le runtime ART (interpréteur ou JIT) exécute du bytecode simple.
- `System.out` est redirigé vers stdout de l'hôte.
- `android.util.Log` ne crash pas (sink vers stderr ou logcat simulé).

## Build (DEX)

```bash
# 1. Compiler en .class (javac)
javac MainActivity.java
# 2. Convertir en DEX (d8 ou dx)
d8 --output hello.dex com/afrios/Hello.class
```

Si `javac` ou `d8` est absent, le binaire `hello.dex` est supposé
précompilé et fourni à côté.
