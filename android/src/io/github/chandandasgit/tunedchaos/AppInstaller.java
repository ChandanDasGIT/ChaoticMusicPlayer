package io.github.chandandasgit.tunedchaos;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;

import androidx.core.content.FileProvider;

import java.io.File;

// Installing a new version of the app, for wrapper/update/appupdater.cpp.
public final class AppInstaller {
    private AppInstaller() {}

    // In the external cache: the one cache folder that Qt's FileProvider paths
    // (qtprovider_paths.xml) cover. "" if there is none right now.
    public static String apkFolder(Context context) {
        File cache = context.getExternalCacheDir();
        return cache == null ? "" : new File(cache, "update").getPath();
    }

    public static boolean canInstall(Context context) {
        return context.getPackageManager().canRequestPackageInstalls();
    }

    // Android's "Install unknown apps" page, for this app.
    public static boolean openInstallSettings(Context context) {
        try {
            Intent intent = new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                                       Uri.parse("package:" + context.getPackageName()));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
            return true;
        } catch (RuntimeException e) {
            return false;
        }
    }

    public static boolean install(Context context, String path) {
        try {
            Uri uri = FileProvider.getUriForFile(context, context.getPackageName() + ".qtprovider", new File(path));
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setDataAndType(uri, "application/vnd.android.package-archive");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(intent);
            return true;
        } catch (RuntimeException e) {
            return false;
        }
    }
}
