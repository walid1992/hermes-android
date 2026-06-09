package com.hermes.wrapper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Represents a JavaScript Object in the Hermes runtime.
 * 
 * Objects must be released after use to avoid memory leaks.
 * If returned to JavaScript (e.g., from a JSCallFunction), do NOT release.
 * 
 * Usage:
 * <pre>
 *   JSObject obj = context.createNewJSObject();
 *   obj.setProperty("name", "Hermes");
 *   obj.setProperty("version", 1.0);
 *   globalObj.setProperty("config", obj);
 *   obj.release();
 * </pre>
 */
public class JSObject {

    protected long nativeHandle;
    protected long contextHandle;
    private boolean released = false;

    // Package-private: created by HermesContext
    JSObject(long contextHandle, long nativeHandle) {
        this.contextHandle = contextHandle;
        this.nativeHandle = nativeHandle;
    }

    // --- Set Property (typed) ---

    public void setProperty(@NonNull String name, @Nullable String value) {
        checkReleased();
        nativeSetPropertyString(contextHandle, nativeHandle, name, value);
    }

    public void setProperty(@NonNull String name, int value) {
        checkReleased();
        nativeSetPropertyInt(contextHandle, nativeHandle, name, value);
    }

    public void setProperty(@NonNull String name, double value) {
        checkReleased();
        nativeSetPropertyDouble(contextHandle, nativeHandle, name, value);
    }

    public void setProperty(@NonNull String name, boolean value) {
        checkReleased();
        nativeSetPropertyBoolean(contextHandle, nativeHandle, name, value);
    }

    public void setProperty(@NonNull String name, @NonNull JSObject value) {
        checkReleased();
        nativeSetPropertyObject(contextHandle, nativeHandle, name, value.nativeHandle);
    }

    public void setProperty(@NonNull String name, @NonNull JSCallFunction function) {
        checkReleased();
        nativeSetPropertyFunction(contextHandle, nativeHandle, name, function);
    }

    // --- Get Property (typed) ---

    @Nullable
    public String getString(@NonNull String name) {
        checkReleased();
        return nativeGetPropertyString(contextHandle, nativeHandle, name);
    }

    public int getInteger(@NonNull String name) {
        checkReleased();
        return nativeGetPropertyInt(contextHandle, nativeHandle, name);
    }

    public double getDouble(@NonNull String name) {
        checkReleased();
        return nativeGetPropertyDouble(contextHandle, nativeHandle, name);
    }

    public boolean getBoolean(@NonNull String name) {
        checkReleased();
        return nativeGetPropertyBoolean(contextHandle, nativeHandle, name);
    }

    @Nullable
    public JSObject getJSObject(@NonNull String name) {
        checkReleased();
        long handle = nativeGetPropertyObject(contextHandle, nativeHandle, name);
        if (handle == 0) return null;
        return new JSObject(contextHandle, handle);
    }

    @Nullable
    public JSArray getJSArray(@NonNull String name) {
        checkReleased();
        long handle = nativeGetPropertyObject(contextHandle, nativeHandle, name);
        if (handle == 0) return null;
        return new JSArray(contextHandle, handle);
    }

    @Nullable
    public JSFunction getJSFunction(@NonNull String name) {
        checkReleased();
        long handle = nativeGetPropertyObject(contextHandle, nativeHandle, name);
        if (handle == 0) return null;
        return new JSFunction(contextHandle, handle);
    }

    // --- Property check ---

    public boolean hasProperty(@NonNull String name) {
        checkReleased();
        return nativeHasProperty(contextHandle, nativeHandle, name);
    }

    // --- Lifecycle ---

    public void release() {
        if (!released) {
            nativeRelease(contextHandle, nativeHandle);
            released = true;
        }
    }

    public boolean isReleased() {
        return released;
    }

    protected void checkReleased() {
        if (released) {
            throw new IllegalStateException("JSObject has been released");
        }
    }

    @Override
    protected void finalize() throws Throwable {
        if (!released) {
            release();
        }
        super.finalize();
    }

    // --- Native methods ---
    private static native void nativeSetPropertyString(long ctx, long obj, String name, String value);
    private static native void nativeSetPropertyInt(long ctx, long obj, String name, int value);
    private static native void nativeSetPropertyDouble(long ctx, long obj, String name, double value);
    private static native void nativeSetPropertyBoolean(long ctx, long obj, String name, boolean value);
    private static native void nativeSetPropertyObject(long ctx, long obj, String name, long valueHandle);
    private static native void nativeSetPropertyFunction(long ctx, long obj, String name, JSCallFunction function);

    private static native String nativeGetPropertyString(long ctx, long obj, String name);
    private static native int nativeGetPropertyInt(long ctx, long obj, String name);
    private static native double nativeGetPropertyDouble(long ctx, long obj, String name);
    private static native boolean nativeGetPropertyBoolean(long ctx, long obj, String name);
    private static native long nativeGetPropertyObject(long ctx, long obj, String name);

    private static native boolean nativeHasProperty(long ctx, long obj, String name);
    private static native void nativeRelease(long ctx, long obj);
}
