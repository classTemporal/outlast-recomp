package com.tapps.outrecomp;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.text.InputType;
import android.view.Gravity;
import android.widget.*;

import java.io.File;
import java.io.FileInputStream;

public class LauncherActivity extends Activity {
    private static final String PREF_NAME = "outlast_config";
    private static final String KEY_GAME_DIR = "game_dir";
    private static final String KEY_FPS_CAP = "fps_cap";
    private static final String KEY_VERBOSE_LOGS = "verbose_logs";

    private SharedPreferences prefs;
    private TextView pathText;
    private TextView statusText;
    private EditText fpsInput;
    private CheckBox verboseCheck;
    private Button startBtn;
    private String currentGameDir = "";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        prefs = getSharedPreferences(PREF_NAME, MODE_PRIVATE);

        checkPermissions();

        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.parseColor("#121212"));
        scroll.setFillViewport(true);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(48, 36, 48, 36);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        scroll.addView(root);

        // Title
        TextView title = new TextView(this);
        title.setText("OUTLAST NATIVE");
        title.setTextSize(32);
        title.setTextColor(Color.parseColor("#E50914"));
        title.setTypeface(null, Typeface.BOLD);
        title.setGravity(Gravity.CENTER);
        title.setPadding(0, 0, 0, 8);
        root.addView(title);

        TextView subTitle = new TextView(this);
        subTitle.setText("Static AOT Port (ARM64 Vulkan 1.1) • BYO Assets Launcher");
        subTitle.setTextSize(14);
        subTitle.setTextColor(Color.parseColor("#AAAAAA"));
        subTitle.setGravity(Gravity.CENTER);
        subTitle.setPadding(0, 0, 0, 32);
        root.addView(subTitle);

        // Path section container
        LinearLayout pathCard = new LinearLayout(this);
        pathCard.setOrientation(LinearLayout.VERTICAL);
        pathCard.setBackgroundColor(Color.parseColor("#1E1E1E"));
        pathCard.setPadding(32, 24, 32, 24);
        root.addView(pathCard);

        TextView lblFolder = new TextView(this);
        lblFolder.setText("Outlast Game Directory (PC Win64):");
        lblFolder.setTextSize(16);
        lblFolder.setTextColor(Color.WHITE);
        lblFolder.setTypeface(null, Typeface.BOLD);
        pathCard.addView(lblFolder);

        pathText = new TextView(this);
        pathText.setTextSize(13);
        pathText.setTextColor(Color.parseColor("#00E676"));
        pathText.setPadding(0, 8, 0, 16);
        pathCard.addView(pathText);

        Button browseBtn = new Button(this);
        browseBtn.setText("Select Game Folder...");
        browseBtn.setBackgroundColor(Color.parseColor("#333333"));
        browseBtn.setTextColor(Color.WHITE);
        browseBtn.setOnClickListener(v -> pickFolder());
        pathCard.addView(browseBtn);

        // Status view
        statusText = new TextView(this);
        statusText.setTextSize(14);
        statusText.setPadding(0, 16, 0, 8);
        pathCard.addView(statusText);

        currentGameDir = prefs.getString(KEY_GAME_DIR, "");

        // Settings Container
        LinearLayout settingsCard = new LinearLayout(this);
        settingsCard.setOrientation(LinearLayout.VERTICAL);
        settingsCard.setBackgroundColor(Color.parseColor("#1E1E1E"));
        settingsCard.setPadding(32, 24, 32, 24);
        LinearLayout.LayoutParams scParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        scParams.setMargins(0, 24, 0, 24);
        settingsCard.setLayoutParams(scParams);
        root.addView(settingsCard);

        TextView lblSettings = new TextView(this);
        lblSettings.setText("Graphics & System Settings:");
        lblSettings.setTextSize(16);
        lblSettings.setTextColor(Color.WHITE);
        lblSettings.setTypeface(null, Typeface.BOLD);
        lblSettings.setPadding(0, 0, 0, 16);
        settingsCard.addView(lblSettings);

        // FPS Limit Row (Custom input editable)
        LinearLayout fpsRow = new LinearLayout(this);
        fpsRow.setOrientation(LinearLayout.HORIZONTAL);
        fpsRow.setGravity(Gravity.CENTER_VERTICAL);
        
        TextView fpsLbl = new TextView(this);
        fpsLbl.setText("Custom FPS Limit: ");
        fpsLbl.setTextColor(Color.parseColor("#DDDDDD"));
        fpsLbl.setTextSize(14);
        fpsRow.addView(fpsLbl);

        fpsInput = new EditText(this);
        fpsInput.setInputType(InputType.TYPE_CLASS_NUMBER);
        fpsInput.setText(String.valueOf(prefs.getInt(KEY_FPS_CAP, 60)));
        fpsInput.setTextColor(Color.WHITE);
        fpsInput.setBackgroundColor(Color.parseColor("#2A2A2A"));
        fpsInput.setPadding(16, 8, 16, 8);
        fpsInput.setWidth(140);
        fpsRow.addView(fpsInput);

        TextView fpsUnit = new TextView(this);
        fpsUnit.setText(" fps (e.g. 30, 40, 60, 120)");
        fpsUnit.setTextColor(Color.GRAY);
        fpsUnit.setTextSize(12);
        fpsUnit.setPadding(8, 0, 0, 0);
        fpsRow.addView(fpsUnit);

        settingsCard.addView(fpsRow);

        // Verbose Logging Checkbox (Default: OFF for maximum native performance)
        verboseCheck = new CheckBox(this);
        verboseCheck.setText("Enable Verbose Diagnostic Logging (Performance Impact)");
        verboseCheck.setTextColor(Color.parseColor("#FFD54F"));
        verboseCheck.setChecked(prefs.getBoolean(KEY_VERBOSE_LOGS, false));
        verboseCheck.setPadding(0, 16, 0, 16);
        settingsCard.addView(verboseCheck);


        // Action Buttons Row
        LinearLayout actionsRow = new LinearLayout(this);
        actionsRow.setOrientation(LinearLayout.VERTICAL);
        root.addView(actionsRow);

        startBtn = new Button(this);
        startBtn.setText("▶ LAUNCH OUTLAST");
        startBtn.setTextSize(18);
        startBtn.setTypeface(null, Typeface.BOLD);
        startBtn.setBackgroundColor(Color.parseColor("#E50914"));
        startBtn.setTextColor(Color.WHITE);
        startBtn.setPadding(0, 24, 0, 24);
        startBtn.setOnClickListener(v -> launchGame());
        actionsRow.addView(startBtn);

        Button logsBtn = new Button(this);
        logsBtn.setText("View Diagnostic Logs (/port/logs/)");
        logsBtn.setBackgroundColor(Color.parseColor("#222222"));
        logsBtn.setTextColor(Color.parseColor("#4CAF50"));
        logsBtn.setPadding(0, 16, 0, 16);
        LinearLayout.LayoutParams lbParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lbParams.setMargins(0, 16, 0, 0);
        logsBtn.setLayoutParams(lbParams);
        logsBtn.setOnClickListener(v -> showLogsDialog());
        actionsRow.addView(logsBtn);

        updateStatus();
        setContentView(scroll);
    }

    private void checkPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                try {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                    intent.setData(Uri.parse("package:" + getPackageName()));
                    startActivity(intent);
                } catch (Exception e) {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                    startActivity(intent);
                }
            }
        }
    }

    private void pickFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, 200);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == 200 && resultCode == RESULT_OK && data != null) {
            Uri treeUri = data.getData();
            if (treeUri != null) {
                try {
                    getContentResolver().takePersistableUriPermission(treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                } catch (Exception ignored) {}
                String path = treeUri.getPath();
                if (path != null && path.contains(":")) {
                    String sub = path.substring(path.indexOf(":") + 1);
                    currentGameDir = new File(Environment.getExternalStorageDirectory(), sub).getAbsolutePath();
                } else {
                    currentGameDir = treeUri.toString();
                }
                prefs.edit().putString(KEY_GAME_DIR, currentGameDir).apply();
                updateStatus();
            }
        }
    }

    private void updateStatus() {
        if (currentGameDir == null || currentGameDir.isEmpty()) {
            pathText.setText("Not selected");
            statusText.setText("⚠ Please select your Outlast directory (containing Engine and OLGame).");
            statusText.setTextColor(Color.parseColor("#FF9800"));
            startBtn.setEnabled(false);
            return;
        }

        pathText.setText(currentGameDir);
        File root = new File(currentGameDir);
        File olgame = new File(root, "OLGame");
        File engine = new File(root, "Engine");
        File ini = new File(olgame, "Config/DefaultEngine.ini");

        StringBuilder sb = new StringBuilder();
        boolean valid = true;

        if (!root.exists()) {
            sb.append("❌ Directory does not exist on storage.\n");
            valid = false;
        } else {
            sb.append("✔ Storage directory accessible\n");
            if (olgame.exists()) sb.append("✔ OLGame found\n");
            else { sb.append("⚠ Missing OLGame/ directory\n"); valid = false; }

            if (engine.exists()) sb.append("✔ Engine found\n");
            else { sb.append("⚠ Missing Engine/ directory\n"); }

            if (ini.exists()) sb.append("✔ DefaultEngine.ini detected\n");
            else sb.append("ℹ Fallback internal configurations will be applied\n");
        }

        statusText.setText(sb.toString().trim());
        statusText.setTextColor(valid ? Color.parseColor("#00E676") : Color.parseColor("#FF5252"));
        startBtn.setEnabled(true);
    }

    private void launchGame() {
        int fps = 60;
        try {
            fps = Integer.parseInt(fpsInput.getText().toString().trim());
        } catch (Exception ignored) {}
        boolean verbose = verboseCheck.isChecked();

        prefs.edit()
                .putInt(KEY_FPS_CAP, fps)
                .putBoolean(KEY_VERBOSE_LOGS, verbose)
                .apply();

        Intent intent = new Intent(this, GameActivity.class);
        intent.putExtra("GAME_DIR", currentGameDir);
        intent.putExtra("FPS_CAP", fps);
        intent.putExtra("VERBOSE_LOGS", verbose);
        startActivity(intent);
    }

    private void showLogsDialog() {
        File logFile = new File(currentGameDir, "port/logs/latest.log");
        File crashFile = new File(currentGameDir, "port/logs/crash.log");
        File backupCrash = new File(getExternalFilesDir(null), "logs/crash.log");
        File backupLog = new File(getExternalFilesDir(null), "logs/latest.log");

        String content = "No logs recorded yet.";
        if (crashFile.exists() && crashFile.length() > 0) {
            content = "=== CRASH DETECTED (/port/logs/crash.log) ===\n" + readFile(crashFile) + "\n\n=== RECENT LOG ===\n" + readFile(logFile);
        } else if (backupCrash.exists() && backupCrash.length() > 0) {
            content = "=== CRASH DETECTED (App Storage) ===\n" + readFile(backupCrash) + "\n\n=== RECENT LOG ===\n" + readFile(backupLog);
        } else if (logFile.exists() && logFile.length() > 0) {
            content = readFile(logFile);
        } else if (backupLog.exists() && backupLog.length() > 0) {
            content = readFile(backupLog);
        }

        ScrollView sv = new ScrollView(this);
        TextView tv = new TextView(this);
        tv.setText(content);
        tv.setTextSize(12);
        tv.setTextColor(Color.GREEN);
        tv.setBackgroundColor(Color.BLACK);
        tv.setPadding(24, 24, 24, 24);
        sv.addView(tv);

        new android.app.AlertDialog.Builder(this)
                .setTitle("Diagnostic Logs (/port/logs/)")
                .setView(sv)
                .setPositiveButton("Close", null)
                .show();
    }

    private String readFile(File f) {
        if (!f.exists()) return "";
        try (FileInputStream fis = new FileInputStream(f)) {
            byte[] data = new byte[(int) Math.min(f.length(), 65536)];
            int read = fis.read(data);
            return new String(data, 0, read);
        } catch (Exception e) {
            return "Error reading log: " + e.getMessage();
        }
    }
}
