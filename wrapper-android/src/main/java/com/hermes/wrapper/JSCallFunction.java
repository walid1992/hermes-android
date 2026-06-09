package com.hermes.wrapper;

/**
 * Functional interface for JavaScript callbacks implemented in Java.
 * 
 * Usage:
 * <pre>
 *   obj.setProperty("greet", (JSCallFunction) args -> {
 *       return "Hello, " + args[0];
 *   });
 * </pre>
 * 
 * Return types supported:
 * - String
 * - Integer / Long / Double / Float (→ JS number)
 * - Boolean (→ JS boolean)
 * - JSObject / JSArray (→ JS object/array)
 * - null (→ JS null)
 */
@FunctionalInterface
public interface JSCallFunction {
    Object call(Object... args);
}
