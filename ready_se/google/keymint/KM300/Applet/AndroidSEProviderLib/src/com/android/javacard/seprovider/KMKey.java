package com.android.javacard.seprovider;

/**
 * KMKey is a marker interface and the SE Provider has to implement this interface. KMAESKey,
 * KMECDeviceUniqueKey, KMECPrivateKey and KMHmacKey implements this interface. Internally, keys are
 * stored as a Javacard key objects, which will provide additional security by avoiding side channel
 * attacks.
 */
public interface KMKey {

  short getPublicKey(byte[] buf, short offset);
}
