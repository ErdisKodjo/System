/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * ability-lifecycle/LifecycleAbility.js — Test HarmonyOS #2.
 *
 * Ability qui loggue chaque callback du cycle de vie HarmonyOS :
 *   onStart → onActive → onInactive → onBackground
 *   → onForeground → onStop
 */

const LOG = [];

function log(name) {
    LOG.push(name);
    console.info(name);
}

export default {
    onStart(want) {
        log("onStart");
    },
    onActive() {
        log("onActive");
    },
    onInactive() {
        log("onInactive");
    },
    onBackground() {
        log("onBackground");
    },
    onForeground() {
        log("onForeground");
    },
    onStop() {
        log("onStop");
        console.info("Hello, AfriOS!");
    },
};
