#!/usr/bin/env bash
set -e

SDK="/opt/android-sdk"
NDK="$SDK/ndk/27.2.12479018"
BUILD_TOOLS="$SDK/build-tools/34.0.0"
PLATFORM="$SDK/platforms/android-34/android.jar"
CLANG="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android34-clang++"
STRIP="$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"
LIBCXX="$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so"

OUTLAST_DIR="/home/ang/outlast-recomp"
SRC_DIR="$OUTLAST_DIR/android"
BUILD_ANDROID="$OUTLAST_DIR/build_android"
WORK_DIR="$OUTLAST_DIR/build/android_pkg"
OUT_DIR="/home/agy2/nextcloud/outlast_builds"

mkdir -p "$WORK_DIR/classes" "$WORK_DIR/dex" "$WORK_DIR/lib/arm64-v8a" "$OUT_DIR"

echo "=== 1. Copying NDK C++ Runtime (libc++_shared.so) & DXVK Native ==="
cp "$LIBCXX" "$WORK_DIR/lib/arm64-v8a/libc++_shared.so"
$STRIP --strip-unneeded "$WORK_DIR/lib/arm64-v8a/libc++_shared.so"
cp "$SRC_DIR/libs/arm64-v8a/libdxvk_d3d9.so" "$WORK_DIR/lib/arm64-v8a/libdxvk_d3d9.so"
$STRIP --strip-unneeded "$WORK_DIR/lib/arm64-v8a/libdxvk_d3d9.so"

echo "=== 2. Linking Native Engine ==="
$CLANG -shared -fPIC -O3 -std=c++20 \
    -static-libstdc++ \
    -Wl,-z,max-page-size=16384 \
    -Wl,--gc-sections \
    -I"$OUTLAST_DIR" \
    -I"$OUTLAST_DIR/runtime" \
    -I"$OUTLAST_DIR/include" \
    -I"$OUTLAST_DIR/runtime/core" \
    -I"$OUTLAST_DIR/runtime/win32" \
    "$SRC_DIR/jni/native_bridge.cpp" \
    -Wl,--whole-archive "$BUILD_ANDROID/librecomp_win32.a" -Wl,--no-whole-archive \
    "$BUILD_ANDROID/liboutlast_lifted.a" \
    -Wl,--whole-archive "$BUILD_ANDROID/librecomp_runtime.a" -Wl,--no-whole-archive \
    "$BUILD_ANDROID/libsoftfloat.a" \
    -o "$WORK_DIR/lib/arm64-v8a/liboutlast_native.so" \
    -llog -landroid -ldl

echo "=== 2.1 Stripping Debug Symbols from Native Library (llvm-strip --strip-all) ==="
$STRIP --strip-all "$WORK_DIR/lib/arm64-v8a/liboutlast_native.so"
ls -lh "$WORK_DIR/lib/arm64-v8a/liboutlast_native.so"

echo "=== 3. Compiling Java Classes ==="
javac --release 8 -classpath "$PLATFORM" \
    -d "$WORK_DIR/classes" \
    "$SRC_DIR/src/com/tapps/outrecomp/CrashHandler.java" \
    "$SRC_DIR/src/com/tapps/outrecomp/OutlastApp.java" \
    "$SRC_DIR/src/com/tapps/outrecomp/LauncherActivity.java" \
    "$SRC_DIR/src/com/tapps/outrecomp/VirtualGamepadView.java" \
    "$SRC_DIR/src/com/tapps/outrecomp/GameActivity.java"

echo "=== 4. Packaging JAR and converting to DEX with D8 ==="
jar --create --file "$WORK_DIR/classes.jar" -C "$WORK_DIR/classes" .
"$BUILD_TOOLS/d8" --lib "$PLATFORM" --min-api 26 --output "$WORK_DIR/dex" "$WORK_DIR/classes.jar"

echo "=== 5. Compiling Resources and Manifest with AAPT2 ==="
"$BUILD_TOOLS/aapt2" compile --dir "$SRC_DIR/res" -o "$WORK_DIR/res.zip"
"$BUILD_TOOLS/aapt2" link "$WORK_DIR/res.zip" \
    -I "$PLATFORM" \
    --manifest "$SRC_DIR/AndroidManifest.xml" \
    --min-sdk-version 26 --target-sdk-version 34 \
    --version-code 4 --version-name "1.1.2" \
    -o "$WORK_DIR/base.apk"

echo "=== 6. Integrating DEX and Stripped ARM64 Native Engines into APK ==="
cd "$WORK_DIR"
jar -uf base.apk -C dex classes.dex
jar -uf base.apk -C "$WORK_DIR" lib/arm64-v8a/libc++_shared.so
jar -uf base.apk -C "$WORK_DIR" lib/arm64-v8a/libdxvk_d3d9.so
jar -uf base.apk -C "$WORK_DIR" lib/arm64-v8a/liboutlast_native.so

echo "=== 7. Aligning and Signing APK (Zipalign & Apksigner) ==="
"$BUILD_TOOLS/zipalign" -f -p 4 base.apk aligned.apk

KEYSTORE="$SRC_DIR/debug.keystore"
if [ ! -f "$KEYSTORE" ]; then
    keytool -genkeypair -noprompt -keystore "$KEYSTORE" \
        -storepass android -keypass android -alias androiddebugkey \
        -dname "CN=Android Debug,O=Outlast Port,C=US" \
        -keyalg RSA -keysize 2048 -validity 10000
fi

FINAL_APK="$OUT_DIR/Outlast_Native_ARM64.apk"
"$BUILD_TOOLS/apksigner" sign --ks "$KEYSTORE" \
    --ks-pass pass:android --key-pass pass:android \
    --out "$FINAL_APK" aligned.apk

echo "=== 8. Verifying APK Integrity ==="
"$BUILD_TOOLS/apksigner" verify --verbose "$FINAL_APK"

echo "=========================================================="
echo "✔ Fixed Signal-Trapping APK generated successfully:"
echo "  Path: $FINAL_APK"
ls -lh "$FINAL_APK"
echo "=========================================================="
