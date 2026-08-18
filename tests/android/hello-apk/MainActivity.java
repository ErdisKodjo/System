/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * hello-apk/MainActivity.java — Test Android #1 : Hello World APK.
 *
 * Activité minimale qui affiche "Hello, AfriOS!" via Log.i() et
 * System.out. Compilée en DEX, testée via
 *   dalvikvm -cp hello.dex com.afrios.Hello
 * dans afros-androsandbox.
 */

package com.afrios;

public class Hello {
    public static void main(String[] args) {
        System.out.println("Hello, AfriOS!");
        android.util.Log.i("AfriOS", "Hello, AfriOS!");
    }
}
