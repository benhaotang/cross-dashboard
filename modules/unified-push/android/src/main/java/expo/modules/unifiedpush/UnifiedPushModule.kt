package expo.modules.unifiedpush

import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition
import org.unifiedpush.android.connector.UnifiedPush

class UnifiedPushModule : Module() {
    override fun definition() = ModuleDefinition {
        Name("UnifiedPush")

        Events("onNewEndpoint", "onRegistrationFailed", "onUnregistered", "onMessage")

        OnStartObserving {
            UnifiedPushEventBus.setListener { event, data ->
                sendEvent(event, data)
            }
        }

        OnStopObserving {
            UnifiedPushEventBus.setListener(null)
        }

        Function("register") { instance: String ->
            val context = appContext.reactContext ?: return@Function false
            UnifiedPush.registerApp(context, instance)
            true
        }

        Function("unregister") { instance: String ->
            val context = appContext.reactContext ?: return@Function false
            UnifiedPush.unregisterApp(context, instance)
            true
        }

        Function("getDistributors") {
            val context = appContext.reactContext ?: return@Function emptyList<String>()
            UnifiedPush.getDistributors(context)
        }

        Function("saveDistributor") { distributor: String ->
            val context = appContext.reactContext ?: return@Function false
            UnifiedPush.saveDistributor(context, distributor)
            true
        }

        Function("getDistributor") {
            val context = appContext.reactContext ?: return@Function ""
            UnifiedPush.getDistributor(context)
        }
    }
}
