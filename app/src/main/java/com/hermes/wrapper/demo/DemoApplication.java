package com.hermes.wrapper.demo;

import android.content.Context;

import androidx.multidex.MultiDex;
import androidx.multidex.MultiDexApplication;

/**
 * Custom Application class that installs MultiDex before any class loading occurs.
 * This ensures fbjni's HybridData and related classes (which may land in classes2.dex)
 * are accessible at startup via JNI FindClass / Java ClassLoader lookups.
 */
public class DemoApplication extends MultiDexApplication {

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(base);
        MultiDex.install(this);
    }
}
