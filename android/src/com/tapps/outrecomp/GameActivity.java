package com.tapps.outrecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Gravity;
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

        // Native Vulkan / Direct3D 9 output is presented directly to SurfaceView
        setContentView(surfaceView);
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
        if (engineStarted) {
            Log.i(TAG, "Surface recreated; engine already running, skipping relaunch");
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
    public void surfaceDestroyed(SurfaceHolder holder) {}

    public static native void initNativeLogging(String logPath, boolean verbose);
    public native int startNativeEngine(Object surface, String gameDir, int fpsCap, boolean verbose);
}
