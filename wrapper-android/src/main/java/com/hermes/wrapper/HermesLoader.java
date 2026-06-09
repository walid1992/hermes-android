package com.hermes.wrapper;

import com.hermes.engine.HermesEngine;

/**
 * Hermes library loader.
 * 
 * Optional convenience class. HermesContext already handles loading
 * automatically in its static block. Use this only if you want to
 * eagerly initialize (e.g., in Application.onCreate) to catch load
 * failures early.
 */
public class HermesLoader {
    private static boolean initialized = false;

    /**
     * Eagerly initialize Hermes engine.
     * Safe to call multiple times — only loads once.
     */
    public static synchronized void init() {
        if (initialized) return;
        HermesEngine.ensureLoaded();
        initialized = true;
    }

    public static boolean isInitialized() {
        return initialized;
    }
}
