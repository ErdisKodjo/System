# android/binder-roundtrip

Test IPC Binder complet (service + client).

## Ce que fait l'app

- `BinderService` s'enregistre auprès de `ServiceManager` sous le nom
  `"hello"` et répond `transact(1, …)` avec la chaîne `"Hello, AfriOS!"`.
- `BinderClient` récupère le service, fait un `transact`, lit la réponse,
  l'affiche sur stdout.

## Ce qu'on valide

- Le driver binder `binder_driver.c` de `afros-androsandbox` transporte
  des transactions inter-processus.
- Le `service_manager.cpp` enregistre et résout les services.
- `Parcel.writeString/readString` fonctionnent (sérialisation).
- Le `reference_tracker.cpp` ne fuit pas les références IBinder.

## Build (DEX)

```bash
javac BinderService.java BinderClient.java
d8 --output binder-service.dex com/afrios/BinderService.class
d8 --output binder-client.dex com/afrios/BinderClient.class
```

Le harness lance d'abord `binder-service.dex` en arrière-plan, puis
`binder-client.dex`.
