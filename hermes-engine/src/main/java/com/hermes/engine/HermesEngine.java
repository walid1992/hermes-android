package com.hermes.engine;

/**
 * Hermes Engine Loader
 * Loads the native Hermes library and its dependencies.
 */
public class HermesEngine {
    private static boolean loaded = false;

    public static synchronized void ensureLoaded() {
        if (!loaded) {
            // fbjni loads its own native lib + Java classes
            // This triggers System.loadLibrary("fbjni") internally
            System.loadLibrary("fbjni");
            // Load Hermes and JSI
            System.loadLibrary("jsi");
            System.loadLibrary("hermes");
            loaded = true;
        }
    }

    public static boolean isLoaded() {
        return loaded;
    }
}
