/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * activity-lifecycle/LifecycleActivity.java — Test Android #3.
 *
 * Activité qui loggue chaque callback de cycle de vie. Le harness
 * valide l'ordre via le logcat :
 *   onCreate → onStart → onResume → onPause → onStop → onDestroy
 */

package com.afrios;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

public class LifecycleActivity extends Activity {
    private static final String TAG = "AfriOS";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "onCreate");
        System.out.println("onCreate");
    }

    @Override
    protected void onStart() {
        super.onStart();
        Log.i(TAG, "onStart");
        System.out.println("onStart");
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume");
        System.out.println("onResume");
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause");
        System.out.println("onPause");
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.i(TAG, "onStop");
        System.out.println("onStop");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy");
        System.out.println("onDestroy");
        System.out.println("Hello, AfriOS!");
    }
}
