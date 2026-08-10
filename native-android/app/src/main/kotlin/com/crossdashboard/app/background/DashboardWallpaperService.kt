package com.crossdashboard.app.background

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.res.Configuration
import android.service.wallpaper.WallpaperService
import android.view.SurfaceHolder
import com.crossdashboard.app.data.prefs.AppPreferences
import dagger.hilt.android.AndroidEntryPoint
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.first
import javax.inject.Inject

@AndroidEntryPoint
class DashboardWallpaperService : WallpaperService() {
    @Inject lateinit var prefs: AppPreferences
    @Inject lateinit var builder: BackgroundContentBuilder
    private val engines = mutableSetOf<DashboardEngine>()
    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == WallpaperUpdateNotifier.ACTION) engines.toList().forEach { it.redraw() }
        }
    }

    override fun onCreate() {
        super.onCreate()
        registerReceiver(receiver, IntentFilter(WallpaperUpdateNotifier.ACTION), Context.RECEIVER_NOT_EXPORTED)
    }
    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        engines.toList().forEach { it.redraw() }
    }
    override fun onDestroy() {
        unregisterReceiver(receiver)
        engines.toList().forEach { it.destroy() }
        super.onDestroy()
    }
    override fun onCreateEngine(): Engine = DashboardEngine().also { engines += it }

    inner class DashboardEngine : Engine() {
        private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
        private var visible = false
        private var surfaceReady = false
        override fun onSurfaceCreated(holder: SurfaceHolder) { surfaceReady = true; redraw() }
        override fun onSurfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) { surfaceReady = true; redraw() }
        override fun onSurfaceRedrawNeeded(holder: SurfaceHolder) = redraw()
        override fun onSurfaceDestroyed(holder: SurfaceHolder) { surfaceReady = false }
        override fun onVisibilityChanged(value: Boolean) { visible = value; if (value) redraw() }
        override fun onDestroy() { destroy(); engines -= this; super.onDestroy() }

        fun redraw() {
            if (!surfaceReady || (!visible && !isPreview)) return
            scope.coroutineContext.cancelChildren()
            scope.launch {
                val displayContext = getDisplayContext() ?: this@DashboardWallpaperService
                val (template, orientation) = WallpaperProfileResolver.resolve(displayContext, prefs)
                val appearance = prefs.wallpaperAppearanceFlow.first()
                val dark = displayContext.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK == Configuration.UI_MODE_NIGHT_YES
                val backdropPath = appearance.imagePath(dark)
                val backdrop = backdropPath?.let { path ->
                    runCatching {
                        android.graphics.ImageDecoder.decodeBitmap(
                            android.graphics.ImageDecoder.createSource(java.io.File(path))
                        ) { decoder, _, _ -> decoder.setAllocator(android.graphics.ImageDecoder.ALLOCATOR_SOFTWARE) }
                    }.getOrNull()
                }
                val content = template?.takeIf { it.enabled }?.let { builder.build(it) }
                    ?: BackgroundContent("CROSS-DASHBOARD", "Capture Inbox or Views to begin", rows = emptyList())
                withContext(Dispatchers.Main.immediate) {
                    if (!surfaceReady) return@withContext
                    var canvas: android.graphics.Canvas? = null
                    try {
                        canvas = surfaceHolder.lockCanvas()
                        val accent = displayContext.getColor(
                            if (dark) android.R.color.system_accent1_200 else android.R.color.system_accent1_600
                        )
                        canvas?.let { BackgroundCanvasRenderer.draw(it, content, orientation, dark, appearance, backdrop, accent) }
                    } finally {
                        backdrop?.recycle()
                        canvas?.let { surfaceHolder.unlockCanvasAndPost(it) }
                    }
                }
            }
        }
        fun destroy() { scope.cancel() }
    }
}
