package com.android.javacard.seprovider;

/**
 * KMRkpMacKey is a marker interface and the SE Provider has to implement this interface. Internally
 * the mac key is stored as a Javacard HMAC key object, which will provide additional security. This
 * key is used to the sign the RKP keys to create MacedPublicKeys.
 */
public interface KMRkpMacKey {}
