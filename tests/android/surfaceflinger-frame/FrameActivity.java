/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * surfaceflinger-frame/FrameActivity.java — Test Android #4.
 *
 * Dessine un pixel rouge via Surface, puis affiche "Hello, AfriOS!".
 * Le harness prend un screenshot et vérifie qu'un pixel rouge est
 * présent (validation visuelle optionnelle).
 */

package com.afrios;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Canvas;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.util.Log;

public class FrameActivity extends Activity
        implements SurfaceHolder.Callback {
    private static final String TAG = "AfriOS";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.i(TAG, "FrameActivity.onCreate");
        SurfaceView sv = new SurfaceView(this);
        sv.getHolder().addCallback(this);
        setContentView(sv);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Canvas c = holder.lockCanvas();
        if (c != null) {
            Paint p = new Paint();
            p.setColor(Color.RED);
            c.drawColor(Color.BLACK);
            c.drawCircle(50, 50, 20, p);
            holder.unlockCanvasAndPost(c);
            Log.i(TAG, "frame drawn");
            System.out.println("Hello, AfriOS!");
        } else {
            Log.e(TAG, "lockCanvas returned null");
            System.out.println("FAIL: null canvas");
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder h, int f, int w, int ht) {}

    @Override
    public void surfaceDestroyed(SurfaceHolder h) {}
}
