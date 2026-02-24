package expo.modules.pomodoroservice

/**
 * Simple in-process event bus so the ForegroundService and AlarmReceiver
 * (which have no direct reference to the Expo module) can forward events
 * to the JS layer.
 */
object PomodoroEventBus {
    private var listener: ((String, Map<String, Any>) -> Unit)? = null

    fun setListener(l: ((String, Map<String, Any>) -> Unit)?) {
        listener = l
    }

    fun emit(event: String, data: Map<String, Any>) {
        listener?.invoke(event, data)
    }
}
