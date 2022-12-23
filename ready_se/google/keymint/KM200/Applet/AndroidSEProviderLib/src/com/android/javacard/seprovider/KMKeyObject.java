package com.android.javacard.seprovider;

/**
 * This class holds the KeyObjects and its associated algorithm value. Each KMKeyObject is tied to
 * one of the crypto operations.
 */
public class KMKeyObject {
  public byte algorithm;
  public Object keyObjectInst;
}
