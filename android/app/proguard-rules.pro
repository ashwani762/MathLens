# JNI entry points
-keepclasseswithmembernames class * { native <methods>; }
-keep class com.mathlens.app.engine.MathEngine { *; }

# ONNX Runtime
-keep class ai.onnxruntime.** { *; }
