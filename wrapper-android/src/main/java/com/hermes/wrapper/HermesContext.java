package com.hermes.wrapper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.hermes.engine.HermesEngine;

/**
 * Hermes JavaScript Context
 * 
 * Main entry point for executing JavaScript code using the Hermes engine.
 * Mirrors the API pattern from QuickJSContext for easy migration.
 * 
 * Usage:
 * <pre>
 *   HermesContext ctx = new HermesContext();
 *   JSObject global = ctx.getGlobalObject();
 *   JSObject obj = ctx.createNewJSObject();
 *   obj.setProperty("name", "Hermes");
 *   global.setProperty("config", obj);
 *   String result = ctx.eval("config.name");
 *   obj.release();
 *   ctx.close();
 * </pre>
 */
public class HermesContext implements AutoCloseable {

    private long nativeHandle;
    private boolean closed = false;

    static {
        // Load hermes_wrapper native library (contains our JNI implementations)
        HermesEngine.ensureLoaded();         // load dependencies first: fbjni, jsi, hermes
        System.loadLibrary("hermes_wrapper"); // then our own JNI bridge
    }

    public HermesContext() {
        nativeHandle = nativeCreate();
    }

    /**
     * Get the global object of the JavaScript context.
     * DO NOT release the returned object - it is owned by the context.
     */
    @NonNull
    public JSObject getGlobalObject() {
        checkNotClosed();
        long handle = nativeGetGlobalObject(nativeHandle);
        return new JSObject(nativeHandle, handle);
    }

    /**
     * Create a new empty JavaScript object.
     * Must be released after use unless returned to JavaScript.
     */
    @NonNull
    public JSObject createNewJSObject() {
        checkNotClosed();
        long handle = nativeCreateObject(nativeHandle);
        return new JSObject(nativeHandle, handle);
    }

    /**
     * Create a new empty JavaScript array.
     * Must be released after use unless returned to JavaScript.
     */
    @NonNull
    public JSArray createNewJSArray() {
        checkNotClosed();
        long handle = nativeCreateArray(nativeHandle);
        return new JSArray(nativeHandle, handle);
    }

    /**
     * Evaluate JavaScript code and return the result as a String.
     */
    @NonNull
    public String eval(@NonNull String code) {
        checkNotClosed();
        return nativeEval(nativeHandle, code);
    }

    /**
     * Evaluate JavaScript and return a boolean.
     */
    public boolean evalBoolean(@NonNull String code) {
        checkNotClosed();
        return nativeEvalBoolean(nativeHandle, code);
    }

    /**
     * Evaluate JavaScript and return a double.
     */
    public double evalDouble(@NonNull String code) {
        checkNotClosed();
        return nativeEvalDouble(nativeHandle, code);
    }

    /**
     * Evaluate JavaScript and return an int.
     */
    public int evalInt(@NonNull String code) {
        checkNotClosed();
        return nativeEvalInt(nativeHandle, code);
    }

    /**
     * Execute a JavaScript script (no return value).
     */
    public void execute(@NonNull String script) {
        checkNotClosed();
        nativeRunScript(nativeHandle, script);
    }

    @Override
    public void close() {
        if (!closed) {
            nativeDestroy(nativeHandle);
            closed = true;
        }
    }

    public boolean isClosed() {
        return closed;
    }

    /**
     * Get native context handle. Used by JSObject/JSArray/JSFunction internally.
     */
    long getNativeHandle() {
        return nativeHandle;
    }

    private void checkNotClosed() {
        if (closed) {
            throw new IllegalStateException("HermesContext has been closed");
        }
    }

    // --- Native methods ---
    private native long nativeCreate();
    private native void nativeDestroy(long handle);
    private native long nativeGetGlobalObject(long handle);
    private native long nativeCreateObject(long handle);
    private native long nativeCreateArray(long handle);
    private native String nativeEval(long handle, String code);
    private native boolean nativeEvalBoolean(long handle, String code);
    private native double nativeEvalDouble(long handle, String code);
    private native int nativeEvalInt(long handle, String code);
    private native void nativeRunScript(long handle, String script);
}
