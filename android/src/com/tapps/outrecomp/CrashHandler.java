package com.tapps.outrecomp;

import android.util.Log;
import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.io.StringWriter;

public class CrashHandler {
    private static final String TAG = "OutlastCrashHandler";

    public static void install(String gameDir, File backupDir) {
        File portLogs = new File(gameDir, "port/logs");
        if (!portLogs.exists()) portLogs.mkdirs();

        Thread.setDefaultUncaughtExceptionHandler((thread, throwable) -> {
            Log.e(TAG, "FATAL UNCAUGHT EXCEPTION in thread " + thread.getName(), throwable);
            try {
                StringWriter sw = new StringWriter();
                throwable.printStackTrace(new PrintWriter(sw));
                String trace = sw.toString();

                File crashFile = new File(portLogs, "crash.log");
                FileWriter fw = new FileWriter(crashFile, true);
                fw.write("=== FATAL UNCAUGHT JAVA/JNI CRASH ===\n");
                fw.write("Thread: " + thread.getName() + " (ID: " + thread.getId() + ")\n");
                fw.write(trace);
                fw.write("\n=====================================\n");
                fw.flush();
                fw.close();

                if (backupDir != null) {
                    FileWriter fw2 = new FileWriter(new File(backupDir, "crash.log"), true);
                    fw2.write("=== FATAL UNCAUGHT JAVA/JNI CRASH ===\n" + trace + "\n");
                    fw2.flush();
                    fw2.close();
                }
            } catch (Exception ignored) {}
            android.os.Process.killProcess(android.os.Process.myPid());
            System.exit(10);
        });
    }
}
