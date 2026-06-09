package com.hermes.wrapper;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Represents a JavaScript Function in the Hermes runtime.
 * Can be called from Java with arguments.
 * 
 * Usage:
 * <pre>
 *   JSFunction fn = obj.getJSFunction("myFunc");
 *   Object result = fn.call("arg1", 42);
 *   fn.release();
 * </pre>
 */
public class JSFunction extends JSObject {

    // Package-private: created by HermesContext or JSObject.getJSFunction
    JSFunction(long contextHandle, long nativeHandle) {
        super(contextHandle, nativeHandle);
    }

    /**
     * Call the function with arguments and return the result.
     * Supported argument types: String, Integer, Long, Double, Float, Boolean, JSObject, null.
     * 
     * @return Result as String, Double, Boolean, JSObject, or null.
     */
    @Nullable
    public Object call(Object... args) {
        checkReleased();
        return nativeCall(contextHandle, nativeHandle, args);
    }

    /**
     * Call the function without caring about the return value.
     * More efficient if result is not needed - no return object allocation.
     */
    public void callVoid(Object... args) {
        checkReleased();
        nativeCallVoid(contextHandle, nativeHandle, args);
    }

    // --- Native methods ---
    private static native Object nativeCall(long ctx, long func, Object[] args);
    private static native void nativeCallVoid(long ctx, long func, Object[] args);
}
