/***************************************************************************
 * Copyright (C) 2017 Kaloyan Raev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/
package name.raev.kaloyan.hellostorj;

import android.app.Activity;
import android.app.NotificationManager;
import android.content.Context;
import android.os.Process;
import android.support.design.widget.Snackbar;
import android.support.v4.app.NotificationCompat;

import java.io.File;

import name.raev.kaloyan.hellostorj.jni.Bucket;
import name.raev.kaloyan.hellostorj.jni.Storj;
import name.raev.kaloyan.hellostorj.jni.callbacks.UploadFileCallback;

class FileUploader implements UploadFileCallback {

    private Activity mActivity;
    private Bucket mBucket;
    private String mFilePath;

    private NotificationManager mNotifyManager;
    private NotificationCompat.Builder mBuilder;

    FileUploader(Activity activity, Bucket bucket, String filePath) {
        mActivity = activity;
        mBucket = bucket;
        mFilePath = filePath;
    }

    public void upload() {
        // show snackbar for user to watch for upload notifications
        Snackbar.make(mActivity.findViewById(R.id.browse_list),
                R.string.upload_in_progress,
                Snackbar.LENGTH_LONG).show();
        // init the upload notification
        mNotifyManager = (NotificationManager) mActivity.getSystemService(Context.NOTIFICATION_SERVICE);
        mBuilder = new NotificationCompat.Builder(mActivity)
                .setSmallIcon(android.R.drawable.stat_sys_upload)
                .setContentTitle(new File(mFilePath).getName())
                .setContentText(mActivity.getResources().getString(R.string.app_name))
                .setProgress(0, 0, true);
        mNotifyManager.notify(mFilePath.hashCode(), mBuilder.build());
        // trigger the upload
        new Thread() {
            @Override
            public void run() {
                Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
                Storj.getInstance().upload(mBucket, mFilePath, FileUploader.this);
            }
        }.start();
    }

    @Override
    public void onProgress(String filePath, double progress, long uploadedBytes, long totalBytes) {
        mBuilder.setProgress(100, (int) (progress * 100), false);
        mNotifyManager.notify(filePath.hashCode(), mBuilder.build());
    }

    @Override
    public void onComplete(String filePath) {
        mBuilder.setProgress(0, 0, false)
                .setSmallIcon(android.R.drawable.stat_sys_upload_done)
                .setContentText(mActivity.getResources().getString(R.string.upload_complete));
        mNotifyManager.notify(filePath.hashCode(), mBuilder.build());
    }

    @Override
    public void onError(String filePath, String message) {
        mBuilder.setProgress(0, 0, false)
                .setSmallIcon(android.R.drawable.stat_notify_error)
                .setContentText(message);
        mNotifyManager.notify(filePath.hashCode(), mBuilder.build());
    }
}
