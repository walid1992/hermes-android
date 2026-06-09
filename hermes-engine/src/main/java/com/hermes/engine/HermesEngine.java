package com.hermes.engine;

/**
 * Hermes Engine Loader
 * Loads the native Hermes library.
 */
public class HermesEngine {
    private static boolean loaded = false;

    public static synchronized void ensureLoaded() {
        if (!loaded) {
            System.loadLibrary("hermes");
            loaded = true;
        }
    }

    public static boolean isLoaded() {
        return loaded;
    }
}
