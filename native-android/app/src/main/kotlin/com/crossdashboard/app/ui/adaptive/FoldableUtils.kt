package com.crossdashboard.app.ui.adaptive

import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.window.layout.FoldingFeature
import androidx.window.layout.WindowInfoTracker
import kotlinx.coroutines.flow.distinctUntilChanged

/**
 * Observes the current [FoldingFeature] from WindowInfoTracker.
 *
 * Returns null when the device is not foldable, the feature is unavailable, or the
 * window is in a non-folded posture. Recomposition is triggered on posture changes.
 */
@Composable
fun rememberFoldingFeature(): FoldingFeature? {
    val context = LocalContext.current
    var foldingFeature by remember { mutableStateOf<FoldingFeature?>(null) }

    LaunchedEffect(context) {
        WindowInfoTracker.getOrCreate(context)
            .windowLayoutInfo(context)
            .distinctUntilChanged()
            .collect { layoutInfo ->
                foldingFeature = layoutInfo.displayFeatures
                    .filterIsInstance<FoldingFeature>()
                    .firstOrNull()
            }
    }

    return foldingFeature
}

/**
 * Returns true when the device is in the HALF_OPENED (tabletop / book) posture.
 * Used to adapt layouts that would place interactive content over the hinge.
 */
@Composable
fun rememberIsHalfOpened(): Boolean {
    val feature = rememberFoldingFeature()
    return feature?.state == FoldingFeature.State.HALF_OPENED
}

/**
 * Returns the hinge width in dp for the current fold posture, or 0.dp if none.
 * Use this to add padding/exclusion around the fold seam.
 */
@Composable
fun rememberHingeWidth(): Dp {
    val feature = rememberFoldingFeature()
    return if (feature != null && feature.isSeparating) {
        val density = LocalDensity.current
        with(density) { feature.bounds.width().toDp() }
    } else {
        0.dp
    }
}

/**
 * Composable wrapper that adapts its content based on foldable posture.
 *
 * When the device is in half-opened (tabletop) posture with a separating hinge,
 * content is inset by the hinge width on the fold axis so it is never rendered
 * behind the physical crease.
 *
 * @param content Lambda receiving the [FoldingFeature] (null on non-foldables) for
 *                additional posture-specific adjustments if needed.
 */
@Composable
fun FoldAwareContent(
    modifier: Modifier = Modifier,
    content: @Composable (foldingFeature: FoldingFeature?) -> Unit,
) {
    val feature = rememberFoldingFeature()
    content(feature)
}

/**
 * Modifier that adds padding to avoid the hinge/crease on foldable devices.
 *
 * When the device reports a separating [FoldingFeature] in HALF_OPENED posture,
 * horizontal padding (vertical fold) or vertical padding (horizontal fold) equal
 * to half the hinge width is applied so interactive elements don't sit on the crease.
 * On non-foldables or when not half-opened, this modifier is a no-op.
 */
fun Modifier.hingeAwarePadding(): Modifier = composed {
    val feature = rememberFoldingFeature()
    if (feature != null && feature.isSeparating && feature.state == FoldingFeature.State.HALF_OPENED) {
        val density = LocalDensity.current
        val hingeWidth = with(density) { feature.bounds.width().toDp() }
        val hingeHeight = with(density) { feature.bounds.height().toDp() }
        when (feature.orientation) {
            FoldingFeature.Orientation.VERTICAL ->
                this.padding(horizontal = hingeWidth / 2)
            FoldingFeature.Orientation.HORIZONTAL ->
                this.padding(vertical = hingeHeight / 2)
            else -> this
        }
    } else {
        this
    }
}
