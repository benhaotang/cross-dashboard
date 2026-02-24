package expo.modules.unifiedpush

/**
 * Simple in-process event bus so the BroadcastReceiver (which has no direct
 * reference to the Expo module) can forward events to the JS layer.
 */
object UnifiedPushEventBus {
    private var listener: ((String, Map<String, String>) -> Unit)? = null

    fun setListener(l: ((String, Map<String, String>) -> Unit)?) {
        listener = l
    }

    fun emit(event: String, data: Map<String, String>) {
        listener?.invoke(event, data)
    }
}
