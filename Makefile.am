lib_LTLIBRARIES = libnfc_nci_linux.la

sbin_PROGRAMS = nfcDemoApp 
nfcDemoApp_DEPENDENCIES = libnfc_nci_linux.la

LDFLAGS = -Bstatic 

if LLCP1_3
LDFLAGS += -L$(openssldir)/lib -lcrypto -lssl
endif

nfcDemoApp_FLAGS = -I$(srcdir)/demoapp -I$(srcdir)/src/include 

AM_CPPFLAGS = \
	-I$(srcdir)/src/include \
	$(INCLUDE_PARAMS) \
	$(libnfc_nci_linux_la_FLAGS) \
	$(nfcDemoApp_FLAGS)

if LLCP1_3
AM_CPPFLAGS += \
	-I$(openssldir)/include
endif
	
configdir = ${sysconfdir}

if PN7120
config_DATA = conf/PN7120/libnfc-brcm.conf conf/PN7120/libnfc-nxp.conf
endif

if PN7150
config_DATA = conf/PN7150/libnfc-brcm.conf conf/PN7150/libnfc-nxp.conf
endif

LIBNFC_NCI_INCLUDE := \
	src/libnfc-nci/include \
	src/libnfc-nci/gki/ulinux \
	src/libnfc-nci/gki/common \
	src/libnfc-nci/nfa/include \
	src/libnfc-nci/nfa/int \
	src/libnfc-nci/nfc/include \
	src/libnfc-nci/nfc/int \
	src/libnfc-nci/hal/include \
	src/libnfc-nci/hal/int

LIBNFC_NCI_SOURCE := \
	src/libnfc-nci/nfa/rw/nfa_rw_main.c \
	src/libnfc-nci/nfa/rw/nfa_rw_api.c \
	src/libnfc-nci/nfa/rw/nfa_rw_act.c \
	src/libnfc-nci/nfa/hci/nfa_hci_ci.c \
	src/libnfc-nci/nfa/hci/nfa_hci_api.c \
	src/libnfc-nci/nfa/hci/nfa_hci_act.c \
	src/libnfc-nci/nfa/hci/nfa_hci_main.c \
	src/libnfc-nci/nfa/hci/nfa_hci_utils.c \
	src/libnfc-nci/nfa/ce/nfa_ce_act.c \
	src/libnfc-nci/nfa/ce/nfa_ce_main.c \
	src/libnfc-nci/nfa/ce/nfa_ce_api.c \
	src/libnfc-nci/nfa/cho/nfa_cho_api.c \
	src/libnfc-nci/nfa/cho/nfa_cho_main.c \
	src/libnfc-nci/nfa/cho/nfa_cho_sm.c \
	src/libnfc-nci/nfa/cho/nfa_cho_util.c \
	src/libnfc-nci/nfa/p2p/nfa_p2p_act.c \
	src/libnfc-nci/nfa/p2p/nfa_p2p_api.c \
	src/libnfc-nci/nfa/p2p/nfa_p2p_main.c \
	src/libnfc-nci/nfa/snep/nfa_snep_act.c \
	src/libnfc-nci/nfa/snep/nfa_snep_api.c \
	src/libnfc-nci/nfa/snep/nfa_snep_default.c \
	src/libnfc-nci/nfa/snep/nfa_snep_main.c \
	src/libnfc-nci/nfa/sys/nfa_sys_cback.c \
	src/libnfc-nci/nfa/sys/nfa_sys_ptim.c \
	src/libnfc-nci/nfa/sys/nfa_sys_cfg.c \
	src/libnfc-nci/nfa/sys/nfa_sys_main.c \
	src/libnfc-nci/nfa/dm/nfa_dm_discover.c \
	src/libnfc-nci/nfa/dm/nfa_dm_act.c \
	src/libnfc-nci/nfa/dm/nfa_dm_main.c \
	src/libnfc-nci/nfa/dm/nfa_dm_ndef.c \
	src/libnfc-nci/nfa/dm/nfa_dm_api.c \
	src/libnfc-nci/nfa/dm/nfa_dm_cfg.c \
	src/libnfc-nci/nfa/ee/nfa_ee_api.c \
	src/libnfc-nci/nfa/ee/nfa_ee_main.c \
	src/libnfc-nci/nfa/ee/nfa_ee_act.c \
	src/libnfc-nci/adaptation/libmain.c \
	src/libnfc-nci/gki/common/gki_debug.c \
	src/libnfc-nci/gki/common/gki_time.c \
	src/libnfc-nci/gki/common/gki_buffer.c \
	src/libnfc-nci/gki/ulinux/gki_ulinux.c \
	src/libnfc-nci/nfc/cipher/cipher.c \
	src/libnfc-nci/nfc/llcp/llcp_sdp.c \
	src/libnfc-nci/nfc/llcp/llcp_link.c \
	src/libnfc-nci/nfc/llcp/llcp_api.c \
	src/libnfc-nci/nfc/llcp/llcp_util.c \
	src/libnfc-nci/nfc/llcp/llcp_dlc.c \
	src/libnfc-nci/nfc/llcp/llcp_main.c \
	src/libnfc-nci/nfc/nfc/nfc_vs.c \
	src/libnfc-nci/nfc/nfc/nfc_task.c \
	src/libnfc-nci/nfc/nfc/nfc_main.c \
	src/libnfc-nci/nfc/nfc/nfc_ncif.c \
	src/libnfc-nci/nfc/nfc/nfc_test.c \
	src/libnfc-nci/nfc/nfc/nfc_ee.c \
	src/libnfc-nci/nfc/nfc/nfc_utils.c \
	src/libnfc-nci/nfc/nci/nci_hrcv.c \
	src/libnfc-nci/nfc/nci/nci_hmsgs.c \
	src/libnfc-nci/nfc/tags/rw_i93.c \
	src/libnfc-nci/nfc/tags/rw_t4t.c \
	src/libnfc-nci/nfc/tags/tags_int.c \
	src/libnfc-nci/nfc/tags/ce_t3t.c \
	src/libnfc-nci/nfc/tags/rw_t2t.c \
	src/libnfc-nci/nfc/tags/rw_t2t_ndef.c \
	src/libnfc-nci/nfc/tags/rw_t1t.c \
	src/libnfc-nci/nfc/tags/ce_t4t.c \
	src/libnfc-nci/nfc/tags/rw_main.c \
	src/libnfc-nci/nfc/tags/rw_t3t.c \
	src/libnfc-nci/nfc/tags/ce_main.c \
	src/libnfc-nci/nfc/tags/rw_t1t_ndef.c \
	src/libnfc-nci/nfc/ndef/ndef_utils.c \
	src/libnfc-nci/nfc/ndef/ndef_cho_utils.c \
	src/libnfc-nci/adaptation/NfcAdaptation.cpp \
	src/libnfc-nci/adaptation/config.cpp \
	src/libnfc-nci/adaptation/OverrideLog.cpp \
	src/libnfc-nci/adaptation/android_logmsg.cpp \
	src/libnfc-nci/adaptation/CrcChecksum.cpp

HALIMPL_INCLUDE := \
	src/halimpl/include \
	src/halimpl/pn54x/utils \
	src/halimpl/pn54x/inc \
	src/halimpl/pn54x/common \
	src/halimpl/pn54x/dnld \
	src/halimpl/pn54x/hal \
	src/halimpl/pn54x/log \
	src/halimpl/pn54x/tml \
	src/halimpl/pn54x/self-test

HALIMPL_SOURCE := \
	src/halimpl/pn54x/tml/phDal4Nfc_messageQueueLib.c \
	src/halimpl/pn54x/tml/phTmlNfc_i2c.c \
	src/halimpl/pn54x/tml/phTmlNfc.c \
	src/halimpl/pn54x/tml/phOsalNfc_Timer.c \
	src/halimpl/pn54x/dnld/phNxpNciHal_Dnld.c \
	src/halimpl/pn54x/dnld/phDnldNfc_Internal.c \
	src/halimpl/pn54x/dnld/phDnldNfc.c \
	src/halimpl/pn54x/dnld/phDnldNfc_Utils.c \
	src/halimpl/pn54x/log/phNxpLog.c \
	src/halimpl/pn54x/self-test/phNxpNciHal_SelfTest.c \
	src/halimpl/pn54x/hal/phNxpNciHal_NfcDepSWPrio.c \
	src/halimpl/pn54x/hal/phNxpNciHal_dta.c \
	src/halimpl/pn54x/hal/phNxpNciHal_ext.c \
	src/halimpl/pn54x/hal/phNxpNciHal_Kovio.c \
	src/halimpl/pn54x/hal/phNxpNciHal.c \
	src/halimpl/pn54x/utils/phNxpNciHal_utils.c \
	src/halimpl/pn54x/utils/phNxpConfig.cpp

SERVICE_INCLUDE := \
	src/service/interface \
	src/service/utils \
	src/service/extns/inc \
	src/service/extns/src/common \
	src/service/extns/src/mifare

SERVICE_SOURCE := \
	src/service/utils/Mutex.cpp \
	src/service/utils/IntervalTimer.cpp \
	src/service/utils/CondVar.cpp \
	src/service/interface/nativeNfcTag.cpp \
	src/service/interface/NfcTag.cpp \
	src/service/interface/nativeNdef.cpp \
	src/service/interface/nativeNfcHandover.cpp \
	src/service/interface/nativeNfcManager.cpp \
	src/service/interface/nativeNfcSnep.cpp \
	src/service/interface/nativeNfcLlcp.cpp \
	src/service/interface/RoutingManager.cpp \
	src/service/extns/src/mifare/phFriNfc_SmtCrdFmt.c \
	src/service/extns/src/mifare/phNxpExtns_MifareStd.c \
	src/service/extns/src/mifare/phFriNfc_MifStdFormat.c \
	src/service/extns/src/mifare/phFriNfc_MifareStdMap.c \
	src/service/extns/src/phNxpExtns.c \
	src/service/linux_nfc_api.c \
	src/service/linux_nfc_factory_api.c

nfcDemoApp_SOURCES := \
		demoapp/main.c \
		demoapp/tools.c

libnfc_nci_linux_la_SOURCES = \
	$(LIBNFC_NCI_SOURCE) \
	$(HALIMPL_SOURCE) \
	$(SERVICE_SOURCE)

include_HEADERS = src/include/linux_nfc_api.h src/include/linux_nfc_factory_api.h

INCLUDE_PARAMS :=$(LIBNFC_NCI_INCLUDE:%=-I$(srcdir)/%)
INCLUDE_PARAMS +=$(HALIMPL_INCLUDE:%=-I$(srcdir)/%)
INCLUDE_PARAMS +=$(SERVICE_INCLUDE:%=-I$(srcdir)/%)

#variables for NFC_NXP_CHIP_TYPE
PN547C2 := 1
PN548C2 := 2
PN551C2 := 3

libnfc_nci_linux_la_FLAGS = 
libnfc_nci_linux_la_FLAGS += -DPN547C2=1
libnfc_nci_linux_la_FLAGS += -DPN548C2=2
libnfc_nci_linux_la_FLAGS += -DPN551C2=3

libnfc_nci_linux_la_FLAGS += -pthread -w -g
libnfc_nci_linux_la_FLAGS += -DNFC_NXP_NOT_OPEN_INCLUDED=TRUE
libnfc_nci_linux_la_FLAGS += -DNXP_HW_SELF_TEST
libnfc_nci_linux_la_FLAGS += -DDEBUG
libnfc_nci_linux_la_FLAGS += -DNXP_NFC_NATIVE_ENABLE_HCE=TRUE

if LLCP1_3
libnfc_nci_linux_la_FLAGS += -DNFC_NXP_LLCP_SECURED_P2P=TRUE
else
libnfc_nci_linux_la_FLAGS += -DNFC_NXP_LLCP_SECURED_P2P=FALSE
endif

if PN7120
libnfc_nci_linux_la_FLAGS += -DNFC_NXP_CHIP_TYPE=PN547C2 -DNXP_CHIP_NAME=\"pn7120\"
libnfc_nci_linux_la_FLAGS += -DPH_NCI_NXP_HAL_ENABLE_FW_DOWNLOAD=FALSE
endif

if PN7150
libnfc_nci_linux_la_FLAGS += -DNFC_NXP_CHIP_TYPE=PN548C2 -DNXP_CHIP_NAME=\"pn7150\"
libnfc_nci_linux_la_FLAGS += -DPH_NCI_NXP_HAL_ENABLE_FW_DOWNLOAD=FALSE
endif

libnfc_nci_linux_la_LIBADD = $(AC_LIBOBJS)
libnfc_nci_linux_la_LDFLAGS :=
libnfc_nci_linux_la_LDFLAGS +=-DPN547C2=1
libnfc_nci_linux_la_LDFLAGS +=-DPN548C2=2
libnfc_nci_linux_la_LDFLAGS +=-DPN551C2=3
libnfc_nci_linux_la_LDFLAGS += -shared -pthread -ldl -lrt -fPIC -release 1 -versionnfo 0:0:0

nfcDemoApp_LDFLAGS = -pthread -ldl -lrt -lnfc_nci_linux


