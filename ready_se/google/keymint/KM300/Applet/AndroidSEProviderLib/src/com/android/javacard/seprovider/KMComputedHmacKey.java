package com.android.javacard.seprovider;

/**
 * KMComputedHmacKey is a marker interface and the SE Provider has to implement this interface.
 * Internally computed key is stored as a Javacard HMAC key object, which will provide additional
 * security.
 */
public interface KMComputedHmacKey {}
