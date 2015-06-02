/******************************************************************************
 *
 *  Copyright (C) 2009-2014 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2013-2014 NXP Semiconductors
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/


/******************************************************************************
 *
 *  defines NCI interface messages (for DH)
 *
 ******************************************************************************/
#ifndef NFC_NCI_HMSGS_H
#define NFC_NCI_HMSGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nci_defs.h"


BOOLEAN nci_proc_core_rsp (BT_HDR *p_msg);
void nci_proc_rf_management_rsp (BT_HDR *p_msg);
void nci_proc_ee_management_rsp (BT_HDR *p_msg);
void nci_proc_core_ntf (BT_HDR *p_msg);
void nci_proc_rf_management_ntf (BT_HDR *p_msg);
void nci_proc_ee_management_ntf (BT_HDR *p_msg);
void nci_proc_prop_rsp (BT_HDR *p_msg);
#if(NFC_NXP_NOT_OPEN_INCLUDED == TRUE)
void nci_proc_prop_nxp_rsp (BT_HDR *p_msg);
#endif
void nci_proc_prop_ntf (BT_HDR *p_msg);


UINT8 nci_snd_core_reset (UINT8 reset_type);
UINT8 nci_snd_core_init (void);
UINT8 nci_snd_core_get_config (UINT8 *param_ids, UINT8 num_ids);
UINT8 nci_snd_core_set_config (UINT8 *p_param_tlvs, UINT8 tlv_size);

UINT8 nci_snd_core_conn_create (UINT8 dest_type, UINT8 num_tlv, UINT8 tlv_size, UINT8 *p_param_tlvs);
UINT8 nci_snd_core_conn_close (UINT8 conn_id);



UINT8 nci_snd_discover_cmd (UINT8 num, tNCI_DISCOVER_PARAMS *p_param);

UINT8 nci_snd_discover_select_cmd (UINT8 rf_disc_id, UINT8 protocol, UINT8 rf_interface);
UINT8 nci_snd_deactivate_cmd (UINT8 de_act_type );
UINT8 nci_snd_discover_map_cmd (UINT8 num, tNCI_DISCOVER_MAPS *p_maps);
UINT8 nci_snd_t3t_polling (UINT16 system_code, UINT8 rc, UINT8 tsn);
UINT8 nci_snd_parameter_update_cmd (UINT8 *p_param_tlvs, UINT8 tlv_size);

#if ((NFC_NFCEE_INCLUDED == TRUE) && (NFC_RW_ONLY == FALSE))
UINT8 nci_snd_nfcee_discover (UINT8 discover_action);
UINT8 nci_snd_nfcee_mode_set (UINT8 nfcee_id, UINT8 nfcee_mode);
UINT8 nci_snd_set_routing_cmd (BOOLEAN more, UINT8 num_tlv, UINT8 tlv_size, UINT8 *p_param_tlvs);
UINT8 nci_snd_get_routing_cmd (void);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* NFC_NCI_MSGS_H */
