# avb_storage

avb_storage is a JavaCard applet and libese-based interface that provides an example
implementation of the libavb stored rollback index requirements for tamper evident
storage.

# Applet functionality

## Functionality

The com.android.verifiedboot.BootStorage exposes a number of non-volatile storage
locations with pre-defined access control policies. There are two 32-byte configuration registers,
eight 8-byte storage registers, and two 32-byte bound registers.

Storage locations 0x00 and 0x01 are configuration registers. They are reset
to 32 0s when a privileged boot signal is provided (see BOOT_PRIV). From there, they may
be extended at any time with 32 bytes of additional data.  The result of an extension is
that they will contain the value of:

    SHA256(last_CR || 32-bytes-of-data)

Storage locations 0x02 - 0x09 are storage registers.  They are 8-byte persistent
arrays that may only be updated with a privileged boot signal is provided.

Storage locations 0xa and 0xb are bound registers.  They are 65-byte locations. 64 bytes may
be set by the caller.  The first 32 bytes are an arbitrary value provider by the caller.
The second 32 bytes are an arbitrary value provided by the caller which binds future updating
of the first 32 bytes.  The second 32 are bound to one of the configuration registers dependent
on the value of the last byte.

Bound registers start bound to a CR0 value of 0x0[32]. This means that they may be updated only
when CR0 is 0x00 .. 0x00 (_or_ if the privileged boot signal is asserted).

## Interface

The applet exposes class 0x80 with LOAD(0x01), EXTEND(0x02), and STORE(0x03) instructions.

Any storage location may be LOADed at any time.
Configuration registers may be EXTENDed at any time.
Registers 0x02 - 0x0b may be STOREd to according to the policy mentioned above.

### Load

The load command looks like:

    [Class][Instruction][Storage slot]
         80           01     0x00-0x0b

And it will return the contents of the given storage location.


### Extend

...

### Store

...


## Privileged boot signal

The privileged boot signal may be a GPIO (non-standard) or it may be set using a prenegotiated
secret with another part of the system.  The requirement is that the privileged boot
signal only be assert-able when the system is in the bootloader.  Once the HLOS has booted,
all storage locations should not be STORE-able.


# Applet identifier and use

The applet identifier is in the Google RID: A000000476. It's PIX is 504958454C424F4F540100
which is PIXELBOOT[0100].  The package is PIXELBOOT[0000]. It is necessary to use your
own RID and PIX if the applet will be used for a non-testing/development purpose.

# Example applet testing using GlobalPlatformPro

## Set and load a storage register value

Set register 0x02 to 010203...08:

    ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80030200080102030405060708
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (91ms) 9000
    A>> T=1 (4+0008) 80030200 08 0102030405060708
    A<< (0000+2) (58ms) 6985
    A>> T=1 (4+0000) 00A40400 00
    A<< (0018+2) (60ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)

Read it back to confirm storage:

    $ ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80010200
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (58ms) 9000
    A>> T=1 (4+0000) 80010200
    A<< (0008+2) (59ms) 0102030405060708 9000
    A>> T=1 (4+0000) 00A40400 00
    A<< (0018+2) (100ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)

## Extend a configuration register with a 32-byte value

Extend the value with 32 bytes (0x20) of the pattern 112233...:
    $ ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80020000201122334455667788990011223344556677889900112233445566778899001122
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (59ms) 9000
    A>> T=1 (4+0032) 80020000 20 1122334455667788990011223344556677889900112233445566778899001122
    A<< (0032+2) (99ms) 7C707B54A48389F1198610FFEB8D27642B5CBE47B95129882D505566114DA3D1 9000
    A>> T=1 (4+0000) 00A40400 00
    A<< (0018+2) (60ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)

Then read it back:

    $ ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80010000
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (58ms) 9000
    A>> T=1 (4+0000) 80010000
    A<< (0032+2) (60ms) 7C707B54A48389F1198610FFEB8D27642B5CBE47B95129882D505566114DA3D1 9000
    A>> T=1 (4+0000) 00A40400 00
    A<< (0018+2) (59ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)


## Set and load a bound storage location


    wad@z600:~/nexus/globalplatform/GlobalPlatformPro$ ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80030a004011223344556677889900112233445566778899001122334455667788990011227c707b54a48389f1198610ffeb8d27642b5cbe47b95129882d505566114da3d1
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (67ms) 9000
    A>> T=1 (4+0064) 80030A00 40 11223344556677889900112233445566778899001122334455667788990011227C707B54A48389F1198610FFEB8D27642B5CBE47B95129882D505566114DA3D1
    A<< (0000+2) (96ms) 9000
    A>> T=1 (4+0000) 00A40400 00 
    A<< (0018+2) (60ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)

    wad@z600:~/nexus/globalplatform/GlobalPlatformPro$ ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80010a00
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (65ms) 9000
    A>> T=1 (4+0000) 80010A00
    A<< (0032+2) (59ms) 1122334455667788990011223344556677889900112233445566778899001122 9000
    A>> T=1 (4+0000) 00A40400 00 
    A<< (0018+2) (60ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)

Try to update it without BOOT_PRIV and without a matching bound value in 0x00:

    $ ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80030a0040112233445566778899001122334455667788990011223344556677889900FFFF7c707b54a48389f1198610ffeb8d27642b5cbe47b95129882d505566114da3d1
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (58ms) 9000
    A>> T=1 (4+0064) 80030A00 40 112233445566778899001122334455667788990011223344556677889900FFFF7C707B54A48389F1198610FFEB8D27642B5CBE47B95129882D505566114DA3D1
    A<< (0000+2) (60ms) 6985
    A>> T=1 (4+0000) 00A40400 00
    A<< (0018+2) (59ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)


Extend CR0 to the correct value (extend with 1122... to get to the expected value):

    $ ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80020000201122334455667788990011223344556677889900112233445566778899001122
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (58ms) 9000
    A>> T=1 (4+0032) 80020000 20 1122334455667788990011223344556677889900112233445566778899001122
    A<< (0032+2) (60ms) 7C707B54A48389F1198610FFEB8D27642B5CBE47B95129882D505566114DA3D1 9000
    A>> T=1 (4+0000) 00A40400 00
    A<< (0018+2) (60ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)

And now it is mutable:

    $ ${JAVA_HOME}/bin/java -jar gp.jar   -d  -a 00A4040010A000000476504958454C424F4F540100 -a 80030a0040112233445566778899001122334455667788990011223344556677889900FFFF7c707b54a48389f1198610ffeb8d27642b5cbe47b95129882d505566114da3d1
    GlobalPlatformPro v0.3.9-24-g7e62155
    Running on Linux 3.13.0-77-generic amd64, Java 1.8.0_111 by Oracle Corporation
    # Detected readers from JNA2PCSC
    [*] Virtual PCD 00 00
    SCardConnect("Virtual PCD 00 00", T=*) -> T=1, 3BF81300008131FE
    SCardBeginTransaction("Virtual PCD 00 00")
    A>> T=1 (4+0016) 00A40400 10 A000000476504958454C424F4F540100
    A<< (0000+2) (68ms) 9000
    A>> T=1 (4+0064) 80030A00 40 112233445566778899001122334455667788990011223344556677889900FFFF7C707B54A48389F1198610FFEB8D27642B5CBE47B95129882D505566114DA3D1
    A<< (0000+2) (96ms) 9000
    A>> T=1 (4+0000) 00A40400 00 
    A<< (0018+2) (61ms) 6F108408A000000151000000A5049F6501FF 9000
    SCardEndTransaction()
    SCardDisconnect("Virtual PCD 00 00", true)
