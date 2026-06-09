# Hermes Android Wrapper

Android wrapper for Meta's [Hermes](https://hermesengine.dev/) JavaScript engine, following the design pattern of [quickjs-wrapper](https://github.com/HarlonWang/quickjs-wrapper).

## Features

- 🔧 Easy-to-use Java API for running JavaScript on Android
- ⚡ Hermes engine — fast startup, low memory, AOT compilation
- 📱 Support for armeabi-v7a, arm64-v8a, x86, x86_64
- 🔄 QuickJS Wrapper-compatible API (easy migration)
- 🔗 Full Java ↔ JS bridge (objects, arrays, callbacks)
- 📦 Multi-module AAR packaging

## Project Structure

```
hermes-android-wrapper/
├── app/                    # Demo application
├── hermes-engine/          # Hermes native library (.so) loader
├── wrapper-java/           # Core JNI wrapper (platform-independent)
│   ├── src/main/cpp/       # C++ JNI bridge (JSI-based)
│   └── src/main/java/      # Java API
│       └── com/hermes/wrapper/
│           ├── HermesContext.java   # Main entry point
│           ├── JSObject.java        # JS object manipulation
│           ├── JSArray.java         # JS array manipulation
│           ├── JSFunction.java      # Call JS functions from Java
│           └── JSCallFunction.java  # Java callback interface
├── wrapper-android/        # Android-specific extensions
├── docs/                   # Documentation & test plans
│   └── test-plan.html      # Interactive test verification page
└── README.md
```

## Quick Start

### 1. Add dependency

```gradle
repositories {
    maven { url 'https://jitpack.io' }
}

dependencies {
    implementation 'com.github.walid1992.hermes-android:wrapper-android:v0.1.0'
}
```

### 2. Initialize

```java
// In Application.onCreate() or before use
HermesLoader.init(getApplicationContext());
```

### 3. Execute JavaScript

```java
HermesContext ctx = new HermesContext();
String result = ctx.eval("1 + 2 + 3");
Log.d("Hermes", result); // "6"
ctx.close();
```

## API Reference

### HermesContext

| Method | Description |
|--------|-------------|
| `eval(String code)` | Evaluate JS and return String result |
| `evalInt(String code)` | Evaluate JS and return int |
| `evalDouble(String code)` | Evaluate JS and return double |
| `evalBoolean(String code)` | Evaluate JS and return boolean |
| `execute(String script)` | Run JS script (no return value) |
| `getGlobalObject()` | Get the global JS object |
| `createNewJSObject()` | Create a new empty JS object |
| `createNewJSArray()` | Create a new empty JS array |
| `close()` | Destroy context and free resources |

### JSObject

| Method | Description |
|--------|-------------|
| `setProperty(name, String)` | Set string property |
| `setProperty(name, int)` | Set integer property |
| `setProperty(name, double)` | Set double property |
| `setProperty(name, boolean)` | Set boolean property |
| `setProperty(name, JSObject)` | Set object property |
| `setProperty(name, JSCallFunction)` | Set Java callback as JS function |
| `getString(name)` | Get property as String |
| `getInteger(name)` | Get property as int |
| `getDouble(name)` | Get property as double |
| `getBoolean(name)` | Get property as boolean |
| `getJSObject(name)` | Get property as JSObject |
| `getJSArray(name)` | Get property as JSArray |
| `getJSFunction(name)` | Get property as JSFunction |
| `hasProperty(name)` | Check if property exists |
| `release()` | Release native reference |

### JSArray (extends JSObject)

| Method | Description |
|--------|-------------|
| `length()` | Get array length |
| `get(int index)` | Get element at index |
| `set(int index, Object value)` | Set element at index |

### JSFunction (extends JSObject)

| Method | Description |
|--------|-------------|
| `call(Object... args)` | Call function, return result |
| `callVoid(Object... args)` | Call function, ignore result |

### JSCallFunction (interface)

```java
@FunctionalInterface
public interface JSCallFunction {
    Object call(Object... args);
}
```

## Usage Examples

### Set Property (Java → JS)

```java
HermesContext context = new HermesContext();
JSObject globalObj = context.getGlobalObject();
JSObject repository = context.createNewJSObject();
repository.setProperty("name", "Hermes Wrapper");
repository.setProperty("created", 2025);
repository.setProperty("version", 1.1);
repository.setProperty("signing_enabled", true);
repository.setProperty("getUrl", (JSCallFunction) args -> {
    return "https://github.com/aspect-build/hermes-wrapper";
});
globalObj.setProperty("repository", repository);
repository.release();
```

```javascript
// In JS:
repository.name;           // "Hermes Wrapper"
repository.created;        // 2025
repository.version;        // 1.1
repository.signing_enabled; // true
repository.getUrl();       // "https://github.com/aspect-build/hermes-wrapper"
```

### Get Property (JS → Java)

```javascript
// Define in JS:
var project = {
    name: 'Hermes Wrapper',
    created: 2025,
    version: 1.1,
    active: true,
    getDesc: function(prefix) { return prefix + ': JS engine wrapper'; }
};
```

```java
// Read in Java:
JSObject globalObj = context.getGlobalObject();
JSObject project = globalObj.getJSObject("project");
project.getString("name");       // "Hermes Wrapper"
project.getInteger("created");   // 2025
project.getDouble("version");    // 1.1
project.getBoolean("active");    // true

JSFunction fn = project.getJSFunction("getDesc");
Object desc = fn.call("Info");   // "Info: JS engine wrapper"
fn.release();
project.release();
```

### Java Callback Functions

```java
// Inject Java function into JS
globalObj.setProperty("nativeAdd", (JSCallFunction) args -> {
    double a = ((Number) args[0]).doubleValue();
    double b = ((Number) args[1]).doubleValue();
    return a + b;
});

// Higher-order: return a function from a function
globalObj.setProperty("createMultiplier", (JSCallFunction) args -> {
    double factor = ((Number) args[0]).doubleValue();
    return (JSCallFunction) innerArgs -> {
        double val = ((Number) innerArgs[0]).doubleValue();
        return val * factor;
    };
});

context.eval("nativeAdd(10, 20)");            // "30"
context.eval("createMultiplier(3)(7)");       // "21"
```

### JSArray

```java
// Create array in Java
JSArray colors = context.createNewJSArray();
colors.set(0, "red");
colors.set(1, "green");
colors.set(2, "blue");
globalObj.setProperty("colors", colors);
colors.release();

context.eval("colors.length");          // "3"
context.eval("colors.join(', ')");      // "red, green, blue"

// Read array from JS
context.execute("var nums = [10, 20, 30, 40, 50]");
JSArray nums = globalObj.getJSArray("nums");
int len = nums.length();                // 5
Object first = nums.get(0);            // 10.0 (Double)
nums.release();
```

### Object Release

```java
// Always release objects after use (unless returning to JS)
JSObject obj = context.createNewJSObject();
obj.setProperty("key", "value");
globalObj.setProperty("myObj", obj);
obj.release();  // Release Java-side reference

// If returning from JSCallFunction, do NOT release:
globalObj.setProperty("factory", (JSCallFunction) args -> {
    JSObject ret = context.createNewJSObject();
    ret.setProperty("created", true);
    // Do NOT call ret.release() here — JS now owns it
    return ret;
});
```

## Comparison with QuickJS Wrapper

| Feature | QuickJS Wrapper | Hermes Wrapper |
|---------|----------------|----------------|
| Engine | QuickJS | Hermes (Meta) |
| eval / execute | ✅ | ✅ |
| JSObject set/get property | ✅ | ✅ |
| JSArray | ✅ | ✅ |
| JSFunction call | ✅ | ✅ |
| JSCallFunction (Java → JS) | ✅ | ✅ |
| ByteCode compile | ✅ | 🔜 Planned |
| ESModule | ✅ | 🔜 Planned |
| Object release / GC | ✅ | ✅ |
| JIT compilation | ❌ | ✅ |
| React Native compat | ❌ | ✅ (same engine) |

## Building from Source

### Prerequisites

- Android Studio Hedgehog+
- Android SDK 34
- NDK 25+
- CMake 3.22.1+

### Build

```bash
./gradlew assembleDebug
```

APK output: `app/build/outputs/apk/debug/app-debug.apk`

### Building Hermes Engine from Source

```bash
git clone https://github.com/nicoboss/hermes.git
cd hermes
mkdir build-android && cd build-android
cmake -S .. -B . \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21 \
  -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Copy built `libhermes.so` to `hermes-engine/src/main/jniLibs/<abi>/`.

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                   Android App                        │
├─────────────────────────────────────────────────────┤
│  Java API Layer                                      │
│  ┌───────────┐ ┌────────┐ ┌─────────┐ ┌──────────┐│
│  │HermesCtx  │ │JSObject│ │JSArray  │ │JSFunction││
│  └─────┬─────┘ └───┬────┘ └────┬────┘ └─────┬────┘│
├────────┼────────────┼───────────┼────────────┼──────┤
│  JNI Bridge (C++ / hermes_context_jni.cpp)          │
│  ┌──────────────────────────────────────────────┐   │
│  │  Object Store (handle → JSI::Object mapping) │   │
│  │  Type conversion (Java ↔ JSI Value)          │   │
│  │  HostFunction bridge (JSCallFunction → JS)   │   │
│  └──────────────────────┬───────────────────────┘   │
├─────────────────────────┼───────────────────────────┤
│  Hermes Runtime (JSI API)                            │
│  ┌──────────┐ ┌───────┐ ┌────────────────────────┐ │
│  │libhermes │ │libjsi │ │libfbjni + folly + c++ │ │
│  └──────────┘ └───────┘ └────────────────────────┘ │
└─────────────────────────────────────────────────────┘
```

## License

Apache License 2.0 — Same as Hermes
