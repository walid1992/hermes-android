package com.hermes.wrapper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Represents a JavaScript Array in the Hermes runtime.
 * Extends JSObject with array-specific operations.
 * 
 * Usage:
 * <pre>
 *   JSArray arr = context.createNewJSArray();
 *   arr.push("hello");
 *   arr.push(42);
 *   globalObj.setProperty("list", arr);
 *   arr.release();
 * </pre>
 */
public class JSArray extends JSObject {

    // Package-private: created by HermesContext
    JSArray(long contextHandle, long nativeHandle) {
        super(contextHandle, nativeHandle);
    }

    /**
     * Get the length of the array.
     */
    public int length() {
        checkReleased();
        return nativeArrayLength(contextHandle, nativeHandle);
    }

    /**
     * Get element at index as Object (auto-typed).
     */
    @Nullable
    public Object get(int index) {
        checkReleased();
        return nativeArrayGet(contextHandle, nativeHandle, index);
    }

    /**
     * Set element at index.
     */
    public void set(int index, @Nullable Object value) {
        checkReleased();
        nativeArraySet(contextHandle, nativeHandle, index, value);
    }

    // --- Native methods ---
    private static native int nativeArrayLength(long ctx, long obj);
    private static native Object nativeArrayGet(long ctx, long obj, int index);
    private static native void nativeArraySet(long ctx, long obj, int index, Object value);
}
