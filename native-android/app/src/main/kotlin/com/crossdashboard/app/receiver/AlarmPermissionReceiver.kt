package com.crossdashboard.app.receiver

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import androidx.work.ExistingWorkPolicy
import androidx.work.WorkManager
import com.crossdashboard.app.worker.SyncWorker

/**
 * Handles [android.app.AlarmManager.ACTION_SCHEDULE_EXACT_ALARM_PERMISSION_STATE_CHANGED].
 *
 * Since we use [android.Manifest.permission.USE_EXACT_ALARM] (auto-granted, non-revocable),
 * this receiver exists as a safety net for edge cases and future-proofing. On receipt,
 * it triggers a sync which will reschedule all event alarms with the current permission state.
 *
 * Declared in AndroidManifest with the corresponding intent-filter.
 */
class AlarmPermissionReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != "android.app.action.SCHEDULE_EXACT_ALARM_PERMISSION_STATE_CHANGED") return

        // Trigger a sync to reschedule alarms now that permission may have changed
        WorkManager.getInstance(context).enqueueUniqueWork(
            SyncWorker.WORK_NAME_ONCE,
            ExistingWorkPolicy.REPLACE,
            SyncWorker.oneTimeRequest(),
        )
    }
}
