# Hermes Android Wrapper

Android wrapper for Meta's [Hermes](https://hermesengine.dev/) JavaScript engine, following the design pattern of [quickjs-wrapper](https://github.com/HarlonWang/quickjs-wrapper).

## Features

- 🔧 Easy-to-use Java API for running JavaScript on Android
- ⚡ Hermes JIT compilation for fast JS execution
- 📱 Support forarmeabi-v7a, arm64-v8a, x86, x86_64
- 🔄 QuickJS-compatible API pattern (easy migration)
- 📦 AAR packaging for easy integration

## Project Structure

```
hermes-android-wrapper/
├── app/                    # Demo application
├── hermes-engine/          # Hermes native library (.so) loader
├── wrapper-java/           # Core JNI wrapper (platform-independent)
│   └── src/main/cpp/       # C++ JNI bridge code
│   └── src/main/java/      # Java API (HermesContext)
├── wrapper-android/        # Android-specific extensions
└── README.md
```

## Quick Start

### 1. Add dependency

```gradle
dependencies {
    implementation project(':wrapper-android')
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

| Method | Description |
|--------|-------------|
| `eval(String code)` | Evaluate JS and return String result |
| `evalInt(String code)` | Evaluate JS and return int |
| `evalDouble(String code)` | Evaluate JS and return double |
| `evalBoolean(String code)` | Evaluate JS and return boolean |
| `setGlobal(String name, String value)` | Set a global JS variable |
| `getGlobal(String name)` | Get a global JS variable |
| `callFunction(String name, String argsJson)` | Call a JS function |
| `close()` | Destroy the context and free resources |

## Building Hermes from Source

To build Hermes natively instead of using prebuilt libraries:

```bash
# Clone Hermes
git clone https://github.com/nicoboss/hermes.git
cd hermes

# Build for Android
mkdir build-android && cd build-android
cmake -S .. -B . \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21 \
  -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)
```

Then copy the built `libhermes.so` to `hermes-engine/src/main/jniLibs/<abi>/`.

## Prebuilt Libraries

You can download prebuilt Hermes libraries from:
- [React Native releases](https://github.com/nicoboss/hermes/releases) (includes Android .so)
- Build from Hermes source (see above)

Place the `.so` files in:
```
hermes-engine/src/main/jniLibs/
├── armeabi-v7a/libhermes.so
├── arm64-v8a/libhermes.so
├── x86/libhermes.so
└── x86_64/libhermes.so
```

## Requirements

- Android Studio Hedgehog+
- Android SDK 34
- NDK 25+
- CMake 3.22.1+

## License

Apache License 2.0 - Same as Hermes
