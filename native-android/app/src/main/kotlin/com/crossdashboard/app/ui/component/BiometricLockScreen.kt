package com.crossdashboard.app.ui.component

import android.content.Context
import androidx.biometric.BiometricManager
import androidx.biometric.BiometricPrompt
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.fragment.app.FragmentActivity
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.ViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewModelScope
import com.crossdashboard.app.data.prefs.AppPreferences
import dagger.hilt.android.lifecycle.HiltViewModel
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.security.MessageDigest
import javax.inject.Inject

// ─── ViewModel ───────────────────────────────────────────────────────────────

@HiltViewModel
class BiometricViewModel @Inject constructor(
    private val prefs: AppPreferences,
) : ViewModel() {

    data class UiState(
        val pin: String = "",
        val error: String? = null,
        val biometricAvailable: Boolean = false,
    )

    private val _state = MutableStateFlow(UiState())
    val state: StateFlow<UiState> = _state.asStateFlow()

    fun appendDigit(digit: String) {
        if (_state.value.pin.length >= 6) return
        _state.update { it.copy(pin = it.pin + digit, error = null) }
    }

    fun deleteDigit() {
        _state.update { it.copy(pin = it.pin.dropLast(1), error = null) }
    }

    fun setBiometricAvailable(available: Boolean) {
        _state.update { it.copy(biometricAvailable = available) }
    }

    /** Returns true if the entered PIN matches the stored SHA-256 hash. */
    suspend fun verifyPin(): Boolean {
        val storedHash = prefs.biometricPinHashFlow.first() ?: return false
        val enteredHash = sha256(_state.value.pin)
        return enteredHash == storedHash
    }

    fun setError(msg: String) {
        _state.update { it.copy(pin = "", error = msg) }
    }

    private fun sha256(input: String): String {
        val bytes = MessageDigest.getInstance("SHA-256").digest(input.toByteArray())
        return bytes.joinToString("") { "%02x".format(it) }
    }
}

// ─── Composable ──────────────────────────────────────────────────────────────

/**
 * Full-screen lock screen shown on cold launch when the app PIN lock is enabled.
 *
 * Always shows the 6-digit PIN numpad. If [systemCredentialEnabled] is true **and**
 * biometric / device-credential hardware is available, an additional button lets the
 * user authenticate via biometrics instead of typing the PIN. The biometric prompt is
 * never triggered automatically — it only fires when the user explicitly taps that button.
 *
 * Calls [onUnlocked] after successful authentication via either path.
 */
@Composable
fun BiometricLockScreen(
    onUnlocked: () -> Unit,
    systemCredentialEnabled: Boolean = false,
    viewModel: BiometricViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsStateWithLifecycle()
    val context = LocalContext.current

    // Probe biometric hardware availability once on enter so we know whether to
    // show the "Use biometric" button (only relevant when systemCredentialEnabled).
    LaunchedEffect(Unit) {
        val bm = BiometricManager.from(context)
        val available = bm.canAuthenticate(
            BiometricManager.Authenticators.BIOMETRIC_STRONG or
                BiometricManager.Authenticators.DEVICE_CREDENTIAL
        ) == BiometricManager.BIOMETRIC_SUCCESS
        viewModel.setBiometricAvailable(available)
    }

    // Auto-verify once 6 digits have been entered
    LaunchedEffect(state.pin) {
        if (state.pin.length == 6) {
            val ok = viewModel.verifyPin()
            if (ok) onUnlocked() else viewModel.setError("Incorrect PIN")
        }
    }

    // Decide whether the biometric button should be offered
    val showBiometricButton = systemCredentialEnabled && state.biometricAvailable

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .systemBarsPadding()
                .padding(horizontal = 40.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center,
        ) {
            Text(
                text = "Cross-Dashboard",
                style = MaterialTheme.typography.headlineSmall,
                fontWeight = FontWeight.SemiBold,
            )

            Spacer(Modifier.height(8.dp))

            Text(
                text = "Enter your PIN to unlock",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            Spacer(Modifier.height(40.dp))

            PinDots(enteredCount = state.pin.length, hasError = state.error != null)

            Spacer(Modifier.height(12.dp))

            if (state.error != null) {
                Text(
                    text = state.error!!,
                    color = MaterialTheme.colorScheme.error,
                    style = MaterialTheme.typography.bodySmall,
                )
            } else {
                // Reserve the same vertical space so the numpad doesn't jump
                Spacer(Modifier.height(16.dp))
            }

            Spacer(Modifier.height(28.dp))

            Numpad(
                onDigit = viewModel::appendDigit,
                onDelete = viewModel::deleteDigit,
                // Biometric button occupies the bottom-left key slot; null hides it
                onBiometric = if (showBiometricButton) {
                    { triggerBiometricPrompt(context, onUnlocked) { viewModel.setError(it) } }
                } else null,
            )

            // Optional label below the numpad so the user knows what the ⊙ button does
            if (showBiometricButton) {
                Spacer(Modifier.height(16.dp))
                TextButton(
                    onClick = { triggerBiometricPrompt(context, onUnlocked) { viewModel.setError(it) } },
                ) {
                    Text(
                        "Use biometric / device credential",
                        style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.primary,
                    )
                }
            }
        }
    }
}

@Composable
private fun PinDots(enteredCount: Int, hasError: Boolean) {
    val color = if (hasError) MaterialTheme.colorScheme.error else MaterialTheme.colorScheme.primary
    Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
        repeat(6) { index ->
            Box(
                modifier = Modifier
                    .size(14.dp)
                    .clip(CircleShape)
                    .background(if (index < enteredCount) color else color.copy(alpha = 0.2f))
            )
        }
    }
}

@Composable
private fun Numpad(
    onDigit: (String) -> Unit,
    onDelete: () -> Unit,
    onBiometric: (() -> Unit)?,
) {
    val rows = listOf(
        listOf("1", "2", "3"),
        listOf("4", "5", "6"),
        listOf("7", "8", "9"),
    )
    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
        rows.forEach { row ->
            Row(horizontalArrangement = Arrangement.spacedBy(24.dp)) {
                row.forEach { digit ->
                    NumpadButton(label = digit, onClick = { onDigit(digit) })
                }
            }
        }
        // Bottom row: biometric | 0 | delete
        Row(horizontalArrangement = Arrangement.spacedBy(24.dp)) {
            if (onBiometric != null) {
                NumpadButton(label = "⊙", onClick = onBiometric)
            } else {
                Spacer(Modifier.size(72.dp))
            }
            NumpadButton(label = "0", onClick = { onDigit("0") })
            NumpadButton(label = "⌫", onClick = onDelete)
        }
    }
}

@Composable
private fun NumpadButton(label: String, onClick: () -> Unit) {
    Box(
        modifier = Modifier
            .size(72.dp)
            .clip(CircleShape)
            .border(1.dp, MaterialTheme.colorScheme.outlineVariant, CircleShape)
            .clickable(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text = label,
            fontSize = 24.sp,
            fontWeight = FontWeight.Medium,
            color = MaterialTheme.colorScheme.onSurface,
        )
    }
}

private fun triggerBiometricPrompt(
    context: Context,
    onSuccess: () -> Unit,
    onError: (String) -> Unit,
) {
    val activity = context as? FragmentActivity ?: return
    val executor = ContextCompat.getMainExecutor(context)
    val prompt = BiometricPrompt(activity, executor, object : BiometricPrompt.AuthenticationCallback() {
        override fun onAuthenticationSucceeded(result: BiometricPrompt.AuthenticationResult) {
            onSuccess()
        }

        override fun onAuthenticationError(errorCode: Int, errString: CharSequence) {
            if (errorCode != BiometricPrompt.ERROR_USER_CANCELED &&
                errorCode != BiometricPrompt.ERROR_NEGATIVE_BUTTON
            ) {
                onError(errString.toString())
            }
        }

        override fun onAuthenticationFailed() {
            onError("Authentication failed")
        }
    })

    val info = BiometricPrompt.PromptInfo.Builder()
        .setTitle("Unlock Cross-Dashboard")
        .setAllowedAuthenticators(
            BiometricManager.Authenticators.BIOMETRIC_STRONG or
                BiometricManager.Authenticators.DEVICE_CREDENTIAL
        )
        .build()

    prompt.authenticate(info)
}
