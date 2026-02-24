package expo.modules.pomodoroservice

import android.app.AlarmManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings
import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition

class PomodoroServiceModule : Module() {
    override fun definition() = ModuleDefinition {
        Name("PomodoroService")

        Events("onTick", "onPhaseEnd", "onAction", "onAlarmFired")

        OnStartObserving {
            PomodoroEventBus.setListener { event, data ->
                sendEvent(event, data)
            }
        }

        OnStopObserving {
            PomodoroEventBus.setListener(null)
        }

        Function("startTimer") { seconds: Int, taskName: String, phase: String ->
            val context = appContext.reactContext ?: return@Function false
            val intent = Intent(context, PomodoroForegroundService::class.java).apply {
                action = PomodoroForegroundService.ACTION_START
                putExtra(PomodoroForegroundService.EXTRA_SECONDS, seconds)
                putExtra(PomodoroForegroundService.EXTRA_TASK_NAME, taskName)
                putExtra(PomodoroForegroundService.EXTRA_PHASE, phase)
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
            true
        }

        Function("pauseTimer") {
            val context = appContext.reactContext ?: return@Function false
            val intent = Intent(context, PomodoroForegroundService::class.java).apply {
                action = PomodoroForegroundService.ACTION_PAUSE
            }
            context.startService(intent)
            true
        }

        Function("resumeTimer") {
            val context = appContext.reactContext ?: return@Function false
            val intent = Intent(context, PomodoroForegroundService::class.java).apply {
                action = PomodoroForegroundService.ACTION_RESUME
            }
            context.startService(intent)
            true
        }

        Function("stopTimer") {
            val context = appContext.reactContext ?: return@Function false
            val intent = Intent(context, PomodoroForegroundService::class.java).apply {
                action = PomodoroForegroundService.ACTION_STOP
            }
            context.startService(intent)
            true
        }

        Function("canScheduleExactAlarms") {
            val context = appContext.reactContext ?: return@Function false
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val alarmManager = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
                alarmManager.canScheduleExactAlarms()
            } else {
                true
            }
        }

        // Opens the system "Alarms & Reminders" settings page for this app so the
        // user can grant SCHEDULE_EXACT_ALARM (Android 12+ / API 31+).
        Function("requestExactAlarmPermission") {
            val context = appContext.reactContext ?: return@Function false
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val intent = Intent(Settings.ACTION_REQUEST_SCHEDULE_EXACT_ALARM).apply {
                    data = Uri.parse("package:${context.packageName}")
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                }
                context.startActivity(intent)
                true
            } else {
                true // Permission not required on Android < 12
            }
        }
    }
}
