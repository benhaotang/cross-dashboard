# Nextcloud SSO
-dontobfuscate
-keep class com.nextcloud.android.sso.** { *; }
-keep class com.owncloud.android.lib.** { *; }

# Tink
-keep class com.google.crypto.tink.** { *; }

# Kotlinx Serialization
-keepattributes *Annotation*, InnerClasses
-dontnote kotlinx.serialization.AnnotationsKt
-keepclassmembers class kotlinx.serialization.json.** { *** Companion; }
-keepclasseswithmembers class **$$serializer { *; }
-keepclassmembers @kotlinx.serialization.Serializable class ** {
    *** Companion;
    *** INSTANCE;
    kotlinx.serialization.KSerializer serializer(...);
}

# Room
-keep class * extends androidx.room.RoomDatabase { *; }

# OkHttp
-dontwarn okhttp3.**
-dontwarn okio.**

# ── androidx.window (FoldableUtils / WindowLayoutInfo / FoldingFeature) ───────
# The window library ships its own consumer rules but we add explicit coverage
# for classes accessed by FoldableUtils.kt via reflection / service loader.
-keep class androidx.window.** { *; }
-keep interface androidx.window.** { *; }
-dontwarn androidx.window.**

# ── Navigation3 (Nav3 runtime) ────────────────────────────────────────────────
# Type-safe Destination sealed class subclasses are @Parcelize — keep the
# generated Creator and CREATOR fields that Parcel.readParcelable() needs.
-keep @kotlinx.parcelize.Parcelize class * implements android.os.Parcelable {
    public static final android.os.Parcelable$Creator *;
}
-keepclassmembers class * implements android.os.Parcelable {
    static ** CREATOR;
}
# Nav3 entry providers are looked up by class name at runtime.
-keep class androidx.navigation3.** { *; }
-dontwarn androidx.navigation3.**

# ── WorkManager workers ────────────────────────────────────────────────────────
# WorkManager instantiates workers via their class names stored in the DB.
-keep class * extends androidx.work.Worker { *; }
-keep class * extends androidx.work.CoroutineWorker { *; }
-keep class * extends androidx.work.ListenableWorker {
    public <init>(android.content.Context, androidx.work.WorkerParameters);
}

# ── Glance widget ──────────────────────────────────────────────────────────────
# GlanceAppWidget subclasses are referenced by the system via GlanceAppWidgetReceiver.
-keep class * extends androidx.glance.appwidget.GlanceAppWidget { *; }
-keep class * extends androidx.glance.appwidget.GlanceAppWidgetReceiver { *; }
-keep class androidx.glance.** { *; }
-dontwarn androidx.glance.**

# ── Hilt / WorkManager integration ────────────────────────────────────────────
# @HiltWorker uses generated factory classes looked up by name.
-keep class dagger.hilt.** { *; }
-keep class * extends dagger.hilt.android.internal.managers.** { *; }
-keep @dagger.hilt.android.AndroidEntryPoint class * { *; }
-keepclasseswithmembernames class * {
    @dagger.hilt.android.AndroidEntryPoint *;
}

# ── BroadcastReceivers / Services instantiated by the system ─────────────────
# These are declared in the manifest but R8 can still strip inner members.
-keep class com.crossdashboard.app.receiver.** { *; }
-keep class com.crossdashboard.app.alarm.** { *; }
-keep class com.crossdashboard.app.service.** { *; }
-keep class com.crossdashboard.app.widget.** { *; }
-keep class com.crossdashboard.app.worker.** { *; }
