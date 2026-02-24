import { gcm } from '@noble/ciphers/aes.js';
import { bytesToHex, hexToBytes, randomBytes, managedNonce } from '@noble/ciphers/utils.js';
import { sha256 } from '@noble/hashes/sha2.js';
import { getCredential, setCredential, deleteCredential } from './keyring';

const ENC_PREFIX = 'enc:';
const KEY_CREDENTIAL = 'encryption_key' as const;
const CUSTOM_FLAG = 'encryption_key_custom' as const;

let cachedKey: Uint8Array | null = null;

export async function initEncryptionKey(): Promise<void> {
  const stored = await getCredential(KEY_CREDENTIAL);
  if (stored) {
    cachedKey = hexToBytes(stored);
  } else {
    cachedKey = randomBytes(32);
    await setCredential(KEY_CREDENTIAL, bytesToHex(cachedKey));
  }
}

function getKey(): Uint8Array {
  if (!cachedKey) throw new Error('Encryption key not initialized. Call initEncryptionKey() first.');
  return cachedKey;
}

export function encrypt(plaintext: string): string {
  const key = getKey();
  const aes = managedNonce(gcm)(key);
  const data = new TextEncoder().encode(plaintext);
  const sealed = aes.encrypt(data);
  return ENC_PREFIX + bytesToHex(sealed);
}

export function decrypt(stored: string): string {
  if (!stored.startsWith(ENC_PREFIX)) return stored;
  const key = getKey();
  const aes = managedNonce(gcm)(key);
  const sealed = hexToBytes(stored.slice(ENC_PREFIX.length));
  const plainBytes = aes.decrypt(sealed);
  return new TextDecoder().decode(plainBytes);
}

export function deriveKeyFromPassphrase(passphrase: string): Uint8Array {
  const encoded = new TextEncoder().encode(passphrase);
  return sha256(encoded);
}

export async function setCustomKey(passphrase: string): Promise<void> {
  const derived = deriveKeyFromPassphrase(passphrase);
  cachedKey = derived;
  await setCredential(KEY_CREDENTIAL, bytesToHex(derived));
  await setCustomKeyFlag(true);
}

export async function resetToRandomKey(): Promise<void> {
  cachedKey = randomBytes(32);
  await setCredential(KEY_CREDENTIAL, bytesToHex(cachedKey));
  await setCustomKeyFlag(false);
}

export async function hasCustomKey(): Promise<boolean> {
  const flag = await getCredential(CUSTOM_FLAG);
  return flag === 'true';
}

async function setCustomKeyFlag(value: boolean): Promise<void> {
  if (value) {
    await setCredential(CUSTOM_FLAG, 'true');
  } else {
    await deleteCredential(CUSTOM_FLAG);
  }
}

export function getEncryptionKey(): Uint8Array {
  return getKey();
}
