/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * softbus-discovery/Discoverer.js — Test HarmonyOS #3 (partie découverte).
 *
 * Lance une découverte SoftBus, attend de trouver l'announcer,
 * affiche "Hello, AfriOS!".
 */

import softbus from '@ohos.net.softbus';

const PKG_NAME = "com.afrios.softbus";

export default {
    onStart() {
        console.info("Discoverer.onStart");
        softbus.on("deviceDiscovery", (info) => {
            console.info("discovered: " + JSON.stringify(info));
            console.info("Hello, AfriOS!");
            softbus.stopDiscoveryDevice(PKG_NAME);
        });
        softbus.startDiscoveryDevice(PKG_NAME, {
            subscribeId: 1,
            mode: 0, /* DISCOVER_MODE_PASSIVE */
            medium: 2, /* AUTO */
            freq: 2, /* MID */
            capability: "ddmpCapability",
        });
    },
    onStop() {
        softbus.stopDiscoveryDevice(PKG_NAME);
    },
};
