![](https://www.firaconsortium.org/themes/custom/uwb2019/logo.svg)

# Introduction
Based on [UWB technology](https://en.wikipedia.org/wiki/Ultra-wideband),  [FiRa Consortium](https://www.firaconsortium.org/about/consortium) has defined the Common Service & Management Layer Technical Specification to provide UWB-based services. One of the components of this ecosystem is Secure Element and corresponding FiRa Applet. This document describes 'required system setup and artifacts information of FiRa applet'

### Requirements
- Jcop (JCOP SE 5.2 R2.02.1-0.21-1.3-1.5.148) or Jcard simulator (3.0.5)
- Global platform API 2.3.0
- javacard 3.0.5 / compiler compliance level 1.5
- Eclipse or Intellj IDE

### Referred documents
- FiRa CSML V1.0.0
- Global platform scp11 V1.3
- Global platform scp03 V1.2
- Design documents FiRa

### memory requirement and package information
```
FiRa Applet
Code size to load: 17093 bytes
Code Size on card: 15007 bytes
Transient memory : 5637 bytes
AID: A00000086746415000
Pkg AID: a0000008670304
```
```
FiRa Service applet
Code size to load: 120 bytes
Code Size on card: 24 bytes
Transient memory : 0 bytes
AID:
Pkg AID: a00000086709
```
```
Secure Channel Interface
Code size to load: 20963 bytes
Code Size on card: 17847 bytes
Transient memory : 7224 bytes (for one channel then 744 for every channel for context allocation)
Pkg AID: a00000086706
```
```
BER
Code size to load: 2271 bytes
Code Size on card: 1878 bytes
Transient memory : 268 bytes
Pkg AID: a0000008675303
```

