/*
 * Copyright (c) 2024 AfriOS. License: Apache-2.0.
 *
 * binder-roundtrip/BinderClient.java — Test Android #2 (partie client).
 *
 * Récupère le service "hello" via ServiceManager, fait transact(1, …),
 * lit la réponse et l'affiche sur stdout.
 */

package com.afrios;

import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.ServiceManager;

public class BinderClient {
    public static void main(String[] args) throws Exception {
        IBinder svc = ServiceManager.getService("hello");
        if (svc == null) {
            System.out.println("FAIL: service 'hello' not found");
            System.exit(1);
            return;
        }
        Parcel data = Parcel.obtain();
        Parcel reply = Parcel.obtain();
        data.writeInterfaceToken("com.afrios.Hello");
        try {
            svc.transact(BinderService.HELLO_CODE, data, reply, 0);
            reply.readException();
            String greeting = reply.readString();
            System.out.println(greeting);
        } catch (RemoteException e) {
            System.out.println("FAIL: " + e.getMessage());
            System.exit(1);
        } finally {
            data.recycle();
            reply.recycle();
        }
    }
}
