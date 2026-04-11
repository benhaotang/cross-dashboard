package com.crossdashboard.app.receiver

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import androidx.work.ExistingWorkPolicy
import androidx.work.WorkManager
import com.crossdashboard.app.worker.SyncWorker

/**
 * Enqueues a one-shot [SyncWorker] on device boot and after our package is replaced
 * (app update). This ensures event alarms are rescheduled after a reboot or update,
 * since AlarmManager alarms do not survive reboots.
 */
class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action !in setOf(
                Intent.ACTION_BOOT_COMPLETED,
                Intent.ACTION_MY_PACKAGE_REPLACED,
            )
        ) return

        WorkManager.getInstance(context).enqueueUniqueWork(
            SyncWorker.WORK_NAME_ONCE,
            ExistingWorkPolicy.REPLACE,
            SyncWorker.oneTimeRequest(),
        )
    }
}
