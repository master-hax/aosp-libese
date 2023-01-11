package com.android.javacard.seprovider;

/**
 * KMRkpMacKey is a marker interface and the Secure Element Provider has to implement this
 * interface. Internally, the mac key is stored as Javacard HMac key object, which will provide
 * additional security. This key is used to sign the RKP keys to create MacedPublicKeys.
 */
public interface KMRkpMacKey {}
