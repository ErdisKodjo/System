/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * hello-hap/HelloAbility.js — Test HarmonyOS #1 : HAP Hello World.
 *
 * Ability (stage model) minimale qui log "Hello, AfriOS!" au onStart.
 */

export default {
    onStart(want) {
        console.info("Hello, AfriOS!");
        console.info("Ability onStart, want=" + JSON.stringify(want));
    },
    onActive() {
        console.info("Ability onActive");
    },
    onBackground() {
        console.info("Ability onBackground");
    },
};
