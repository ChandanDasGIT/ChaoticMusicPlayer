package io.github.chandandasgit.tunedchaos;

import android.content.Context;

import com.chaquo.python.PyObject;
import com.chaquo.python.Python;
import com.chaquo.python.android.AndroidPlatform;

import java.util.List;

// yt-dlp for the app's C++ side (backend/system/process/process_android.cpp):
// runs it in the app's Python (src/main/python/ytdlp_bridge.py).
public final class YtDlpRunner {
    private YtDlpRunner() {}

    // args[0]: "yt-dlp" or a newer copy's path; returns {exit code, output, errors}.
    public static String[] run(Context context, String[] args) {
        synchronized (YtDlpRunner.class) {
            if (!Python.isStarted())
                Python.start(new AndroidPlatform(context));
        }
        PyObject result = Python.getInstance().getModule("ytdlp_bridge").callAttr("run", (Object) args);
        List<PyObject> parts = result.asList();
        return new String[] { parts.get(0).toString(), parts.get(1).toString(), parts.get(2).toString() };
    }
}
