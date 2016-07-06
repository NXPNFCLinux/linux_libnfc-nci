linux_libnfc-nci
================
Linux NFC stack for NCI based NXP NFC Controllers.

Information about NXP NFC Controller can be found on [NXP website](http://www.nxp.com/products/identification_and_security/nfc_and_reader_ics/nfc_controller_solutions/#overview).

Further details about the stack [here](https://github.com/NXPNFCLinux/linux_libnfc-nci/blob/master/doc/AN11697%20-%20PN71x0%20Linux%20Software%20Stack%20Integration%20Guidelines.pdf).

Release version
---------------
R2.1 includes support for PN7150 NFC Controller IC and some bug fixes (refer to the [documentation](https://github.com/NXPNFCLinux/linux_libnfc-nci/blob/master/doc/AN11697%20-%20PN71x0%20Linux%20Software%20Stack%20Integration%20Guidelines.pdf) for more details).

R2.0 includes LLCP1.3 support and some bug fixes (refer to the [documentation](https://github.com/NXPNFCLinux/linux_libnfc-nci/blob/master/doc/AN11697%20-%20PN71x0%20Linux%20Software%20Stack%20Integration%20Guidelines.pdf) for more details).

R1.0 is the first official release of Linux libnfc-nci stack

Possible problems, known errors and restrictions of R2.1:
---------------------------------------------------------
LLCP1.3 support requires OpenSSL Cryptography and SSL/TLS Toolkit (version 1.0.1j or later)
