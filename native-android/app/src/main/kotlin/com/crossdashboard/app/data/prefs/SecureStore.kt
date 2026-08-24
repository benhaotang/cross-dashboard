package com.crossdashboard.app.data.prefs

import android.content.Context
import android.content.SharedPreferences
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import com.google.crypto.tink.aead.AeadConfig
import dagger.hilt.android.qualifiers.ApplicationContext
import java.security.KeyStore
import javax.crypto.AEADBadTagException
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec
import javax.inject.Inject
import javax.inject.Singleton

/**
 * Secure credential storage backed by Android Keystore AES-GCM encryption.
 *
 * Architecture:
 * - A 256-bit AES-GCM key is created in the hardware-backed Android Keystore on first use.
 * - Values are encrypted with that key (12-byte random IV + 16-byte GCM tag) and stored
 *   as Base64 in a SharedPreferences file excluded from Auto Backup.
 * - The backing file `secure_credentials.xml` is excluded from cloud backup and device
 *   transfer via data_extraction_rules.xml.
 */
@Singleton
class SecureStore @Inject constructor(
    @param:ApplicationContext private val context: Context,
) {
    private val prefs: SharedPreferences by lazy {
        context.getSharedPreferences(PREFS_FILE, Context.MODE_PRIVATE)
    }

    private val secretKey: SecretKey by lazy { getOrCreateKey() }

    fun set(key: String, value: String) {
        val encrypted = encrypt(value.toByteArray(Charsets.UTF_8))
        prefs.edit().putString(key, encrypted).apply()
    }

    fun get(key: String): String? {
        val raw = prefs.getString(key, null) ?: return null
        return try {
            decrypt(raw)?.let { String(it, Charsets.UTF_8) }
        } catch (e: AEADBadTagException) {
            null
        }
    }

    fun delete(key: String) {
        prefs.edit().remove(key).apply()
    }

    fun has(key: String): Boolean = prefs.contains(key)

    fun clearAll() {
        prefs.edit().clear().apply()
    }

    // ─── AES-GCM helpers ─────────────────────────────────────────────────────

    private fun encrypt(plaintext: ByteArray): String {
        val cipher = Cipher.getInstance(AES_GCM_TRANSFORMATION)
        // Android Keystore requires randomized encryption (the default); do not supply an IV —
        // the Keystore generates one internally and exposes it via cipher.iv after doFinal.
        cipher.init(Cipher.ENCRYPT_MODE, secretKey)
        val ciphertext = cipher.doFinal(plaintext)
        val iv = cipher.iv  // Keystore-assigned 12-byte IV
        val combined = iv + ciphertext
        return android.util.Base64.encodeToString(combined, android.util.Base64.NO_WRAP)
    }

    private fun decrypt(encoded: String): ByteArray? {
        val combined = android.util.Base64.decode(encoded, android.util.Base64.NO_WRAP)
        if (combined.size <= GCM_IV_LENGTH) return null
        val iv = combined.copyOfRange(0, GCM_IV_LENGTH)
        val ciphertext = combined.copyOfRange(GCM_IV_LENGTH, combined.size)
        val cipher = Cipher.getInstance(AES_GCM_TRANSFORMATION)
        cipher.init(Cipher.DECRYPT_MODE, secretKey, GCMParameterSpec(GCM_TAG_LENGTH_BITS, iv))
        return cipher.doFinal(ciphertext)
    }

    // ─── Keystore key management ──────────────────────────────────────────────

    private fun getOrCreateKey(): SecretKey {
        val ks = KeyStore.getInstance(ANDROID_KEYSTORE).also { it.load(null) }
        ks.getKey(KEY_ALIAS, null)?.let { return it as SecretKey }

        val spec = KeyGenParameterSpec.Builder(
            KEY_ALIAS,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT,
        )
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256)
            .setUserAuthenticationRequired(false)
            .build()

        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, ANDROID_KEYSTORE)
            .also { it.init(spec) }
            .generateKey()
    }

    companion object {
        private const val ANDROID_KEYSTORE = "AndroidKeyStore"
        private const val KEY_ALIAS = "cross_dashboard_master_key"
        private const val PREFS_FILE = "secure_credentials"
        private const val AES_GCM_TRANSFORMATION = "AES/GCM/NoPadding"
        private const val GCM_IV_LENGTH = 12
        private const val GCM_TAG_LENGTH_BITS = 128
    }
}

// Credential key constants — same logical keys across Android, macOS, and Linux stores
object CredentialKey {
    const val CALDAV_SERVER = "caldav_server"
    const val CALDAV_USERNAME = "caldav_username"
    const val CALDAV_PASSWORD = "caldav_password"
    const val CALDAV_AUTH_METHOD = "caldav_auth_method"
    const val CALDAV_SELECTED_CALENDARS = "caldav_selected_calendars"
    const val CALDAV_DEFAULT_EVENT_CALENDAR = "caldav_default_event_calendar"
    const val CALDAV_DEFAULT_TASK_CALENDAR = "caldav_default_task_calendar"
    const val NEXTCLOUD_SSO_ACCOUNT = "nextcloud_sso_account"
    const val GITEA_TOKEN = "gitea_token"
    const val GITEA_INSTANCE = "gitea_instance"
    const val GITEA_REPOS = "gitea_repos"
    const val MEMOS_HOST = "memos_host"
    const val MEMOS_TOKEN = "memos_token"
    const val KARAKEEP_HOST = "karakeep_host"
    const val KARAKEEP_TOKEN = "karakeep_token"
}
