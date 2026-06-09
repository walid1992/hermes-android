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
 *   String result = ctx.eval("1 + 2");
 *   ctx.close();
 * </pre>
 */
public class HermesContext implements AutoCloseable {

    private long nativeHandle;
    private boolean closed = false;

    public HermesContext() {
        HermesEngine.ensureLoaded();
        nativeHandle = nativeCreate();
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
     * Set a global variable in the JavaScript context.
     */
    public void setGlobal(@NonNull String name, @NonNull String value) {
        checkNotClosed();
        nativeSetGlobal(nativeHandle, name, value);
    }

    /**
     * Get a global variable from the JavaScript context.
     */
    @NonNull
    public String getGlobal(@NonNull String name) {
        checkNotClosed();
        return nativeGetGlobal(nativeHandle, name);
    }

    /**
     * Execute a JavaScript script file from assets.
     */
    public void runScript(@NonNull String script) {
        checkNotClosed();
        nativeRunScript(nativeHandle, script);
    }

    /**
     * Call a JavaScript function by name with JSON arguments.
     */
    @NonNull
    public String callFunction(@NonNull String funcName, @NonNull String argsJson) {
        checkNotClosed();
        return nativeCallFunction(nativeHandle, funcName, argsJson);
    }

    @Override
    public void close() {
        if (!closed) {
            nativeDestroy(nativeHandle);
            closed = true;
        }
    }

    private void checkNotClosed() {
        if (closed) {
            throw new IllegalStateException("HermesContext has been closed");
        }
    }

    // Native methods
    private native long nativeCreate();
    private native void nativeDestroy(long handle);
    private native String nativeEval(long handle, String code);
    private native boolean nativeEvalBoolean(long handle, String code);
    private native double nativeEvalDouble(long handle, String code);
    private native int nativeEvalInt(long handle, String code);
    private native void nativeSetGlobal(long handle, String name, String value);
    private native String nativeGetGlobal(long handle, String name);
    private native void nativeRunScript(long handle, String script);
    private native String nativeCallFunction(long handle, String funcName, String argsJson);
}
