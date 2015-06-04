linux_libnfc-nci
================
Linux NFC stack for NCI based NXP NFC Controllers.

Information about NXP NFC Controller can be found on [NXP website](http://www.nxp.com/products/identification_and_security/nfc_and_reader_ics/nfc_controller_solutions/#overview).

Further details about the stack [here](doc/AN11697 - PN7120 Linux Sofware Stack Integration Guidelines.pdf).

Release version
---------------
R0.4 is the first official delivery of the Linux libnfc-nci stack.

All targeted features available but not fully tested. R1.0 with full test is planned in mid-Q3 2015 (intermediate release may be delivered in case of major issue reported).

Possible problems, known errors and restrictions of R0.4:
---------------------------------------------------------
* RF stuck observed during P2P communication (low occurrence).
* Current logic is not checking the polling loop status before starting SNEP server or HCE.
