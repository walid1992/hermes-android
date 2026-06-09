# Keep all fbjni classes – required for JNI FindClass lookups at native init time
-keep class com.facebook.jni.** { *; }
-keepclassmembers class com.facebook.jni.** { *; }

# Keep Hermes wrapper classes used via JNI
-keep class com.hermes.engine.** { *; }
-keep class com.hermes.wrapper.** { *; }
