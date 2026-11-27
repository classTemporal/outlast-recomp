package com.tapps.outrecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.*;

import java.io.File;
import java.io.FileWriter;

public class GameActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "OutlastNative";
    private static String loadError = null;

    static {
        try {
            System.loadLibrary("c++_shared");
        } catch (Throwable ignored) {}
        try {
            System.loadLibrary("outlast_native");
            Log.i(TAG, "liboutlast_native.so loaded successfully in GameActivity static block");
        } catch (Throwable t) {
            loadError = t.toString();
            Log.e(TAG, "Failed to load outlast_native library: " + t.getMessage(), t);
        }
    }

    private SurfaceView surfaceView;
    private String gameDir;
    private int fpsCap;
    private boolean verboseLogs;
    private Handler mainHandler;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O_MR1) {
            setShowWhenLocked(true);
            setTurnScreenOn(true);
        }
        getWindow().addFlags(
            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON |
            WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED |
            WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON |
            WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD
        );
        mainHandler = new Handler(Looper.getMainLooper());

        gameDir = getIntent().getStringExtra("GAME_DIR");
        if (gameDir == null || gameDir.isEmpty()) {
            android.content.SharedPreferences p = getSharedPreferences("OutlastPrefs", MODE_PRIVATE);
            gameDir = p.getString("game_dir", "");
        }
        if (gameDir == null || gameDir.isEmpty()) {
            File standard = new File(android.os.Environment.getExternalStorageDirectory(),
                "Download/Outlast - Complete Edition (GOG) (v1.0)/Outlast - Complete Edition (GOG) (v1.0)");
            if (standard.exists()) {
                gameDir = standard.getAbsolutePath();
            }
        }
        if (gameDir == null) gameDir = "";
        fpsCap = getIntent().getIntExtra("FPS_CAP", 60);
        verboseLogs = getIntent().getBooleanExtra("VERBOSE_LOGS", false);

        File portLogs = new File(gameDir, "port/logs");
        if (!portLogs.exists()) portLogs.mkdirs();
        File logFile = new File(portLogs, "latest.log");
        String logPath = logFile.getAbsolutePath();

        File backupDir = new File(getExternalFilesDir(null), "logs");
        backupDir.mkdirs();

        CrashHandler.install(gameDir, backupDir);

        try {
            initNativeLogging(logPath, verboseLogs);
        } catch (Throwable t) {
            Log.e(TAG, "initNativeLogging call failed: " + t.getMessage());
        }

        surfaceView = new SurfaceView(this);
        surfaceView.getHolder().setKeepScreenOn(true);
        surfaceView.getHolder().addCallback(this);

        VirtualGamepadView gamepadView = new VirtualGamepadView(this, new VirtualGamepadView.Listener() {
            @Override
            public void onButtonChanged(int xinputMask, boolean down) {
                if (loadError != null) return;
                sendNativeGamepad(xinputMask, down);
                switch (xinputMask) {
                    case 0x1000: sendNativeKey(0x1c, 0x0d, down); break; // A -> Enter
                    case 0x2000: // B -> Esc
                    case 0x0010: sendNativeKey(0x01, 0x1b, down); break; // START -> Esc
                    case 0x0001: sendNativeKey(0xc8, 0x26, down); break; // D-Pad Up
                    case 0x0002: sendNativeKey(0xd0, 0x28, down); break; // D-Pad Down
                    case 0x0004: sendNativeKey(0xcb, 0x25, down); break; // D-Pad Left
                    case 0x0008: sendNativeKey(0xcd, 0x27, down); break; // D-Pad Right
                }
            }

            @Override
            public void onAxesChanged(int lt, int rt, int lx, int ly, int rx, int ry) {
                if (loadError == null) sendNativeGamepadAxes(lt, rt, lx, ly, rx, ry);
            }
        });

        FrameLayout root = new FrameLayout(this);
        root.addView(surfaceView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        root.addView(gamepadView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        setContentView(root);
    }

    // Called from Native code via JNI to update progress (no-op: raw native video presentation)
    public void onNativeLoadingProgress(int percent, String currentItem) {
        Log.i(TAG, "Engine Progress: " + percent + "% " + (currentItem != null ? currentItem : ""));
    }

    // Called from Native code via JNI when a critical file or shader is missing
    public void onNativeLoadingError(String message) {
        mainHandler.post(() -> {
            new AlertDialog.Builder(this)
                    .setTitle("Engine Loading Error")
                    .setMessage(message)
                    .setPositiveButton("OK", (d, w) -> finish())
                    .setCancelable(false)
                    .show();
        });
    }

    private static volatile boolean engineStarted = false;

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (loadError == null) updateNativeSurface(holder.getSurface());
        if (engineStarted) {
            Log.i(TAG, "Surface recreated; updated native renderer");
            return;
        }
        engineStarted = true;
        Log.i(TAG, "Surface created, launching engine worker thread");
        new Thread(null, () -> {
            try {
                int ret = startNativeEngine(holder.getSurface(), gameDir, fpsCap, verboseLogs);
                Log.i(TAG, "Native engine finished with return code: " + ret);
            } catch (Throwable t) {
                Log.e(TAG, "Error executing startNativeEngine", t);
                onNativeLoadingError("Fatal engine exception: " + t.getMessage());
            }
        }, "OutlastEngineThread", 16 * 1024 * 1024).start();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (loadError == null) updateNativeSurface(null);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (super.dispatchTouchEvent(event)) return true;
        if (loadError != null || surfaceView.getWidth() <= 0 || surfaceView.getHeight() <= 0)
            return false;
        int action;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: action = 1; break;
            case MotionEvent.ACTION_MOVE: action = 0; break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL: action = 2; break;
            default: return true;
        }
        int x = (int) (event.getX() * 1280 / surfaceView.getWidth());
        int y = (int) (event.getY() * 720 / surfaceView.getHeight());
        sendNativeTouch(x, y, action);
        return true;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int scan = 0;
        int vk = 0;
        switch (event.getKeyCode()) {
            case KeyEvent.KEYCODE_ENTER:
            case KeyEvent.KEYCODE_DPAD_CENTER:
            case KeyEvent.KEYCODE_BUTTON_A: scan = 0x1c; vk = 0x0d; break;
            case KeyEvent.KEYCODE_ESCAPE:
            case KeyEvent.KEYCODE_BACK:
            case KeyEvent.KEYCODE_BUTTON_B: scan = 0x01; vk = 0x1b; break;
            case KeyEvent.KEYCODE_SPACE: scan = 0x39; vk = 0x20; break;
            case KeyEvent.KEYCODE_DPAD_UP: scan = 0xc8; vk = 0x26; break;
            case KeyEvent.KEYCODE_DPAD_DOWN: scan = 0xd0; vk = 0x28; break;
            case KeyEvent.KEYCODE_DPAD_LEFT: scan = 0xcb; vk = 0x25; break;
            case KeyEvent.KEYCODE_DPAD_RIGHT: scan = 0xcd; vk = 0x27; break;
            default: return super.dispatchKeyEvent(event);
        }
        if (loadError != null) return super.dispatchKeyEvent(event);
        if (event.getAction() == KeyEvent.ACTION_DOWN || event.getAction() == KeyEvent.ACTION_UP) {
            int button = 0;
            switch (event.getKeyCode()) {
                case KeyEvent.KEYCODE_BUTTON_A: button = 0x1000; break;
                case KeyEvent.KEYCODE_BUTTON_B: button = 0x2000; break;
                case KeyEvent.KEYCODE_DPAD_UP: button = 0x0001; break;
                case KeyEvent.KEYCODE_DPAD_DOWN: button = 0x0002; break;
                case KeyEvent.KEYCODE_DPAD_LEFT: button = 0x0004; break;
                case KeyEvent.KEYCODE_DPAD_RIGHT: button = 0x0008; break;
            }
            if (button != 0) sendNativeGamepad(button, event.getAction() == KeyEvent.ACTION_DOWN);
            if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A ||
                    event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B) return true;
            sendNativeKey(scan, vk, event.getAction() == KeyEvent.ACTION_DOWN);
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    protected void onPause() {
        if (loadError == null) releaseNativeKeys();
        super.onPause();
    }

    public static native void initNativeLogging(String logPath, boolean verbose);
    private static native void updateNativeSurface(Object surface);
    private static native void sendNativeKey(int scan, int vk, boolean down);
    private static native void sendNativeGamepad(int button, boolean down);
    private static native void sendNativeGamepadAxes(int lt, int rt, int lx, int ly, int rx, int ry);
    private static native void sendNativeTouch(int x, int y, int action);
    private static native void releaseNativeKeys();
    public native int startNativeEngine(Object surface, String gameDir, int fpsCap, boolean verbose);
}
