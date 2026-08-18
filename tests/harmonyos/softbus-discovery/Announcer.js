/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * softbus-discovery/Announcer.js — Test HarmonyOS #3 (partie annonce).
 *
 * Utilise l'API SoftBus (@ohos.net.softbus) pour s'annoncer sur le
 * réseau local, écoute les demandes de discovery entrantes.
 */

import softbus from '@ohos.net.softbus';

const DEVICE_ID = "afrios-announcer-001";
const PKG_NAME   = "com.afrios.softbus";

export default {
    onStart() {
        console.info("Announcer.onStart");
        softbus.startPublishDeviceDiscovery(PKG_NAME, {
            publishId: 1,
            mode: 0, /* DISCOVER_MODE_PASSIVE */
            freq: 2, /* MID */
            capability: "ddmpCapability",
        });
        console.info("publish started");
    },
    onStop() {
        softbus.stopPublishDeviceDiscovery(PKG_NAME, 1);
        console.info("publish stopped");
        console.info("Hello, AfriOS!");
    },
};
