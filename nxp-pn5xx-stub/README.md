# nxp-pn5xx-stub
NXP's NFC Open Source Kernel mode stub driver

NXP PN5XX stub driver for testing purpose (applications based on libnfc-nci).
This is a stub driver to PN5xx NFC Controller devices, removing i2c bus access,
used to run on computer for error handling in libnfc-nci.
EXAMPLE: simulate case when NFC device is not plugged (=> libnfc-nci R2.1
executes 2 infinite loops in i2c read thread)

How to use it?
Directly on your computer (x86, x86_64), without i2c nor NFC device:
   make
   sudo insmod ./nxp-pn5xx-stub/pn5xx_i2c_stub.ko
   sudo chmod a+rw /dev/pn544

   then execute NXP nfcDemoApp: ./linux_libnfc-nci-R2.1/.libs/nfcDemoApp
