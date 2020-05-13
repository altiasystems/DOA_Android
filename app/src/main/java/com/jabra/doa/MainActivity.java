package com.jabra.doa;

import androidx.appcompat.app.AppCompatActivity;

import android.app.Application;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = MainActivity.class.getSimpleName();

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        tv.setText(stringFromJNI());

        String nativeLibDir = getApplicationInfo().nativeLibraryDir;
        Log.d(TAG, "onCreate: nativeLibDir = " + nativeLibDir);

        String cacheDir = getCacheDir().getAbsolutePath();
        Log.d(TAG, "onCreate: cacheDir = " + cacheDir);

        copyAssets(getResources().getAssets(), cacheDir);

        String runTime = "GPU";
        String dlcFile = "doa_quantize.dlc";
        String inputFile = "data_0.raw";
        DOAInference doa = new DOAInference((Application) getApplicationContext(), cacheDir, dlcFile);
        doa.runInference(inputFile);

        /*
        handle = startDOA(nativeLibDir, runTime, dlcFile, inputFile, cacheDir);
        dialogHandler = new Handler() {
            public void handleMessage(Message msg) {
                switch (msg.what) {
                    case STOP_DOA:
                        stopDOA(handle);
                        break;
                }
            }
        };
        */
        // stop the recording after 10 seconds
        //Message m = dialogHandler.obtainMessage(STOP_DOA, 0, 0);
        //dialogHandler.sendMessageDelayed(m, 10000);

    }

    public static void copyAssets(AssetManager assetManager, String cacheDir)
    {
        String[] files;
        try {
            files = assetManager.list("") ;
        } catch (IOException e) {
            Log.e(TAG, "Failed to get asset file list.", e);
            return;
        }
        if (files != null) for (String filename : files) {
            InputStream in = null;
            OutputStream out = null;
            try {
                Log.d(TAG, "Copying asset file: " + filename + " to cacheDir: " + cacheDir);
                in = assetManager.open(filename);
                File outFile = new File(cacheDir, filename);
                out = new FileOutputStream(outFile);
                copyFile(in, out);
            } catch(IOException e) {
                Log.e(TAG, "Failed to copy asset file: " + filename);
            }
            finally {
                if (in != null) {
                    try {
                        in.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
                if (out != null) {
                    try {
                        out.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
            }
        }
    }

    private static void copyFile(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[1024];
        int read;
        while((read = in.read(buffer)) != -1){
            out.write(buffer, 0, read);
        }
    }
    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
    private native long startDOA(String nativeLibDir, String runTime, String dlcFile, String inputFile, String cacheDir);
    private native void stopDOA(long handle_);

    private long handle;
    public Handler dialogHandler;
    public static final int STOP_DOA=100;

}
