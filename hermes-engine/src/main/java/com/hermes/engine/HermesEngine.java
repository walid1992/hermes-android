package com.hermes.engine;

/**
 * Hermes Engine Loader
 * Loads the native Hermes library and its dependencies.
 */
public class HermesEngine {
    private static boolean loaded = false;

    public static synchronized void ensureLoaded() {
        if (!loaded) {
            // Load dependencies in correct order
            System.loadLibrary("c++_shared");
            System.loadLibrary("fbjni");
            System.loadLibrary("jsi");
            System.loadLibrary("hermes");
            loaded = true;
        }
    }

    public static boolean isLoaded() {
        return loaded;
    }
}
