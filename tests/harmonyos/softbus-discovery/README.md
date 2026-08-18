# harmonyos/softbus-discovery

Test de découverte SoftBus entre deux devices simulés.

## Ce que fait l'app

- `Announcer.js` : `startPublishDeviceDiscovery` s'annonce sur le
  réseau local via SoftBus (mDNS / CoAP / BLE simulés).
- `Discoverer.js` : `startDiscoveryDevice` écoute, sur
  `"deviceDiscovery"` reçoit l'info de l'announcer, affiche
  `"Hello, AfriOS!"`, arrête la discovery.

## Ce qu'on valide

- `softbus/discovery/mdns_discovery.c` / `coap_discovery.c` /
  `ble_discovery.c` de `afros-harmonygate` découvrent un pair.
- `device_discovery.c` du `device_manager` enregistre le pair trouvé.
- Le binding `@ohos.net.softbus` est exposé au runtime JS.

## Build (HAP)

```bash
mkdir -p abilities
cp Announcer.js Discoverer.js abilities/
zip -r softbus.hap module.json abilities/
```

Le harness lance `Announcer` en arrière-plan puis `Discoverer`. La
validation principale est l'apparition de `"Hello, AfriOS!"` dans
stdout du Discoverer (qui ne l'affiche que s'il a reçu l'événement
`deviceDiscovery`).
