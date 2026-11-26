package com.tapps.outrecomp;

import android.app.Application;
import android.util.Log;

public class OutlastApp extends Application {
    private static final String TAG = "OutlastApp";

    @Override
    public void onCreate() {
        super.onCreate();
        try {
            System.loadLibrary("outlast_native");
            Log.i(TAG, "liboutlast_native.so loaded successfully in Application");
        } catch (Throwable t) {
            Log.e(TAG, "Failed to load liboutlast_native.so in Application", t);
        }
    }
}
