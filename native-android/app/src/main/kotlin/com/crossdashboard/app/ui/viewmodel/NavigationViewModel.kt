package com.crossdashboard.app.ui.viewmodel

import androidx.lifecycle.ViewModel
import com.crossdashboard.app.ui.component.CalendarColorResolver
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject

/**
 * Thin ViewModel whose sole purpose is to expose the [CalendarColorResolver] singleton
 * into the Compose tree via [hiltViewModel] at the navigation host level.
 *
 * This avoids threading the resolver down through every screen's parameter list
 * while still honouring Hilt's DI graph.
 */
@HiltViewModel
class NavigationViewModel @Inject constructor(
    val colorResolver: CalendarColorResolver,
) : ViewModel()
