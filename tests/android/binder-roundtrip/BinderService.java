/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * binder-roundtrip/BinderService.java — Test Android #2 (partie service).
 *
 * Service qui exporte un IBinder renvoyant la chaîne "Hello, AfriOS!"
 * quand le client appelle transact(1, …). Le client vérifie la réponse.
 */

package com.afrios;

import android.os.Binder;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;

public class BinderService extends Binder {
    public static final int HELLO_CODE = 1;

    @Override
    protected boolean onTransact(int code, Parcel data,
                                  Parcel reply, int flags)
            throws RemoteException {
        if (code == HELLO_CODE) {
            data.enforceInterface("com.afrios.Hello");
            reply.writeNoException();
            reply.writeString("Hello, AfriOS!");
            return true;
        }
        return super.onTransact(code, data, reply, flags);
    }

    public static void main(String[] args) {
        BinderService svc = new BinderService();
        System.out.println("SERVICE_READY");
        /* Le service_manager de afros-androsandbox l'enregistre. */
        try {
            Thread.sleep(2000);
        } catch (InterruptedException e) {
            /* shutdown */
        }
        System.out.println("Hello, AfriOS!");
    }
}
