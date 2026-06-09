package com.hermes.wrapper.android;

import android.content.Context;
import android.util.Log;

import com.hermes.engine.HermesEngine;

/**
 * Android-specific Hermes library loader.
 * Handles loading the correct .so for the device architecture.
 */
public class HermesLoader {
    private static final String TAG = "HermesLoader";
    private static boolean initialized = false;

    /**
     * Initialize Hermes engine. Call this once in Application.onCreate() or
     * before using HermesContext.
     * 
     * @param context Application context
     */
    public static synchronized void init(Context context) {
        if (initialized) {
            return;
        }
        
        try {
            HermesEngine.ensureLoaded();
            initialized = true;
            if (HermesEngine.isHermesRuntimeLoaded()) {
                Log.i(TAG, "Hermes engine initialized successfully");
            } else {
                Log.w(TAG, "Hermes runtime not loaded; running with JNI wrapper fallback");
            }
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load Hermes native library", e);
            throw new RuntimeException("Failed to load Hermes engine", e);
        }
    }

    public static boolean isInitialized() {
        return initialized;
    }
}
