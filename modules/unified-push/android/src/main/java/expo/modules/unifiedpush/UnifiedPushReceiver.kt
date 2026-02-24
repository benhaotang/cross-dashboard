package expo.modules.unifiedpush

import android.content.Context
import org.unifiedpush.android.connector.MessagingReceiver

class UnifiedPushReceiver : MessagingReceiver() {

    override fun onNewEndpoint(context: Context, endpoint: String, instance: String) {
        UnifiedPushEventBus.emit("onNewEndpoint", mapOf("endpoint" to endpoint, "instance" to instance))
    }

    override fun onRegistrationFailed(context: Context, instance: String) {
        UnifiedPushEventBus.emit("onRegistrationFailed", mapOf("instance" to instance))
    }

    override fun onUnregistered(context: Context, instance: String) {
        UnifiedPushEventBus.emit("onUnregistered", mapOf("instance" to instance))
    }

    override fun onMessage(context: Context, message: ByteArray, instance: String) {
        val messageStr = String(message, Charsets.UTF_8)
        UnifiedPushEventBus.emit("onMessage", mapOf("message" to messageStr, "instance" to instance))
    }
}
