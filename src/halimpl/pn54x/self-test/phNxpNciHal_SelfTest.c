/*
 * Copyright (C) 2015 NXP Semiconductors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef NXP_HW_SELF_TEST

#include <phNxpNciHal_SelfTest.h>
#include <phNxpLog.h>
#include <pthread.h>
#include <phOsalNfc_Timer.h>
#include <phNxpConfig.h>

/* Timeout value to wait for response from PN54X */
#define HAL_WRITE_RSP_TIMEOUT (2000)
#define HAL_WRITE_MAX_RETRY     (10)
#define CORE_INIT_NCI2_0 false

#define NXP_NFCC_RESET_RSP_LEN  (0x11U) //for PN553 and PN557 (0x10U) else (0x11U)
#define NFCC_EXP_NTF_LEN_NULL    0x00
#define NFCC_EXP_NTF_DATA_NULL   0x00
#define NFCC_CMD_NCI20_LEN       0x05
#define NFCC_CMD_NCI10_LEN       0x03
#define NFCC_EXP_RES_DATA_BYTE2 ((phNxpNciHal_getChipType() == pn547C2) ? 0x17 : 0x19)
#define NXP_NFCC_HW_ANTENNA_LOOP4_SELF_TEST (0x01U) //for PN553 and PN557 (0x00U) else (0x01U)
/******************* Structures and definitions *******************************/

typedef uint8_t (*st_validator_t)(nci_data_t *exp, phTmlNfc_TransactInfo_t *act);

phAntenna_St_Resp_t phAntenna_resp;

typedef struct nci_test_data
{
    nci_data_t cmd;
    nci_data_t exp_rsp;
    nci_data_t exp_ntf;
    st_validator_t rsp_validator;
    st_validator_t ntf_validator;

} nci_test_data_t;

typedef struct selftest_hdlr
{
    bool_t wait_for_ntf;
    uint8_t nci_version;
    phTmlNfc_TransactInfo_t mTransInfo;
} selftest_hdlr_t;
selftest_hdlr_t mSelfTestHdlr;

/******************* Global variables *****************************************/

static int thread_running = 0;
static uint32_t timeoutTimerId = 0;
static int hal_write_timer_fired = 0;

/* TML Context */
extern phTmlNfc_Context_t *gpphTmlNfc_Context;

/* Global HAL Ref */
extern phNxpNciHal_Control_t nxpncihal_ctrl;

/* Driver parameters */
phLibNfc_sConfig_t   gDrvCfg;
const static uint8_t nfcc_core_reset_nci20_rsp[] = NCI_CORE_RESET_NCI20_RSP;
const static uint8_t nfcc_core_reset_nci20_ntf[] = NCI_CORE_RESET_NCI20_NTF;
const static uint8_t nfcc_core_init_nci20_cmd[] = NCI_CORE_INIT_NCI20_CMD;
const static uint8_t nfcc_core_init_nci20_rsp[] = NCI_CORE_INIT_NCI20_RSP;
const static uint8_t nfcc_core_init_nci10_cmd[] = NCI_CORE_INIT_NCI10_CMD;
uint8_t nfcc_power_mgt_cmd[] = SYSTEM_SET_POWERMGT_CMD;
uint8_t nfcc_power_mgt_rsp[] = SYSTEM_SET_POWERMGT_RSP;

NFCSTATUS gtxldo_status = NFCSTATUS_FAILED;
NFCSTATUS gagc_value_status = NFCSTATUS_FAILED;
NFCSTATUS gagc_nfcld_status = NFCSTATUS_FAILED;
NFCSTATUS gagc_differential_status = NFCSTATUS_FAILED;

static uint8_t st_validator_testEquals(nci_data_t* exp,
                                       phTmlNfc_TransactInfo_t* act);
static uint8_t st_validator_null(nci_data_t *exp, phTmlNfc_TransactInfo_t *act);
static uint8_t st_validator_testSWP1_vltg(nci_data_t* exp,
        phTmlNfc_TransactInfo_t* act);
static uint8_t st_validator_testAntenna_Txldo(nci_data_t* exp,
        phTmlNfc_TransactInfo_t* act);
static uint8_t st_validator_testAntenna_AgcVal(nci_data_t* exp,
        phTmlNfc_TransactInfo_t* act);
static uint8_t st_validator_testAntenna_AgcVal_FixedNfcLd(
    nci_data_t* exp, phTmlNfc_TransactInfo_t* act);
static uint8_t st_validator_testAntenna_AgcVal_Differential(
    nci_data_t* exp, phTmlNfc_TransactInfo_t* act);

NFCSTATUS phNxpNciHal_getPrbsCmd(phNxpNfc_PrbsType_t prbs_type,
                                 phNxpNfc_PrbsHwType_t hw_prbs_type,
                                 uint8_t tech, uint8_t bitrate,
                                 uint8_t* prbs_cmd, uint8_t prbs_cmd_len);

static NFCSTATUS phNxpNciHal_initialize_chipType();

/* Test data to validate SWP line 2*/
static nci_test_data_t swp2_test_data[] =
{
    {
        {
            0x04, {0x20, 0x00, 0x01, 0x00} /* cmd */
        },
        {
            0x06, {0x40, 0x00, 0x03, 0x00, NXP_NFCC_RESET_RSP_LEN, 0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x20,0x01,0x00}
        },
        {
            0x4, {0x40, 0x01, 0x19, 0x00}
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x2F,0x02,0x00} /* cmd */
        },
        {
            0x04, {0x4F,0x02,0x05,0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x04, {0x2F,0x3E,0x01,0x01} /* cmd */
        },
        {
            0x04, {0x4F,0x3E,0x01,0x00} /* exp_rsp */
        },
        {
            0x04, {0x6F,0x3E,0x02,0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_testEquals
    },

};

/* Test data to validate SWP line 1*/
static nci_test_data_t swp1_test_data[] =
{
    {
        {
            0x04, {0x20, 0x00, 0x01, 0x00} /* cmd */
        },
        {
            0x06, {0x40, 0x00, 0x03, 0x00, NXP_NFCC_RESET_RSP_LEN, 0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x20,0x01,0x00}
        },
        {
            0x4, {0x40,0x01,0x19,0x00 } /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x2F,0x02,0x00} /* cmd */
        },
        {
            0x04, {0x4F,0x02,0x05,0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x04, {0x2F,0x3E,0x01,0x00} /* cmd */
        },
        {
            0x04, {0x4F,0x3E,0x01,0x00} /* exp_rsp */
        },
        {
            0x04, {0x6F,0x3E,0x02,0x00} /* ext_ntf */
        },

        st_validator_testEquals, /* validator */
        st_validator_testSWP1_vltg
    },
};

static nci_test_data_t prbs_test_data[10] =
{
    {
        {
            0x04, {0x20,0x00,0x01,0x00} /* cmd */
        },
        {
            0x06, {0x40, 0x00, 0x03, 0x00, NXP_NFCC_RESET_RSP_LEN, 0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x20,0x01,0x00} /* cmd */
        },
        {
            0x4, {0x40,0x01,0x19,0x00 } /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    }
};

/* for rf field test, first requires to disable the standby mode */
static nci_test_data_t rf_field_on_test_data[10] =
{
    {
        {
            0x04, {0x20,0x00,0x01,0x00} /* cmd */
        },
        {
            0x06, {0x40, 0x00, 0x03, 0x00, NXP_NFCC_RESET_RSP_LEN, 0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x20,0x01,0x00} /* cmd */
        },
        {
            0x4, {0x40,0x01,0x19,0x00 } /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
};

static nci_test_data_t rf_field_off_test_data[10] =
{
    {
        {
            0x04, {0x20,0x00,0x01,0x00} /* cmd */
        },
        {
            0x06, {0x40, 0x00, 0x03, 0x00, NXP_NFCC_RESET_RSP_LEN, 0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x20,0x01,0x00} /* cmd */
        },
        {
            0x4, {0x40, 0x01, 0x19, 0x00}                          /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
};

/* Download pin test data 1 */
static nci_test_data_t download_pin_test_data1[] =
{
    {
        {
            0x04, {0x20, 0x00, 0x01, 0x00} /* cmd */
        },
        {
            0x06, {0x40, 0x00, 0x03, 0x00, NXP_NFCC_RESET_RSP_LEN, 0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
};

/* Download pin test data 2 */
static nci_test_data_t download_pin_test_data2[] =
{
    {
        {
            0x08, {0x00, 0x04, 0xD0, 0x11, 0x00, 0x00, 0x5B, 0x46} /* cmd */
        },
        {
            0x08, {0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x87, 0x16} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
};
/* Antenna self test data*/
static nci_test_data_t antenna_self_test_data[10] =
{
    {
        {
            0x04, {0x20,0x00,0x01,0x00} /* cmd */
        },
        {
            0x06, {0x40, 0x00, 0x03, 0x00, NXP_NFCC_RESET_RSP_LEN, 0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x20,0x01,0x00} /* cmd */
        },
        {
            0x4, {0x40, 0x01, 0x19, 0x00}                          /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    },
    {
        {
            0x03, {0x2F,0x02,0x00} /* cmd */
        },
        {
            0x04, {0x4F,0x02,0x05,0x00} /* exp_rsp */
        },
        {
            0x00, {0x00} /* ext_ntf */
        },
        st_validator_testEquals, /* validator */
        st_validator_null
    }
};

/************** Self test functions ***************************************/

static uint8_t st_validator_testEquals(nci_data_t* exp,
                                       phTmlNfc_TransactInfo_t* act);
static void hal_write_cb(void *pContext, phTmlNfc_TransactInfo_t *pInfo);
static void hal_write_rsp_timeout_cb(uint32_t TimerId, void *pContext);
static void hal_read_cb(void *pContext, phTmlNfc_TransactInfo_t *pInfo);

/*******************************************************************************
 **
 ** Function         st_validator_null
 **
 ** Description      Null Validator
 **
 ** Returns          One
 **
 *******************************************************************************/
static uint8_t st_validator_null(nci_data_t* exp,
                                 phTmlNfc_TransactInfo_t* act)
{
    uint8_t result = 0;

    if (NULL == exp || NULL == act)
    {
        return result;
    }

    if (exp->len <= act->wLength &&
            (memcmp(exp->p_data, act->pBuff, exp->len) == 0))
    {
        result = 1;
    }
    return result;
}

/*******************************************************************************
 **
 ** Function         st_validator_testSWP1_vltg
 **
 ** Description      Validator function to validate swp1 connection.
 **
 ** Returns          One if successful otherwise Zero.
 **
 *******************************************************************************/
static uint8_t st_validator_testSWP1_vltg(nci_data_t* exp,
        phTmlNfc_TransactInfo_t* act)
{
    uint8_t result = 0;

    if (NULL == exp || NULL == act)
    {
        return result;
    }

    if( (act->wLength == 0x05) &&
            (memcmp(exp->p_data, act->pBuff, exp->len) == 0))
    {
        if (act->pBuff[4] == 0x01 || act->pBuff[4] == 0x02)
        {
            result = 1;
        }
    }

    return result;
}

/*******************************************************************************
 **
 ** Function         st_validator_testAntenna_Txldo
 **
 ** Description      Validator function to validate Antenna TxLDO current
 **                  measurement.
 **
 ** Returns          One if successful otherwise Zero.
 **
 *******************************************************************************/
static uint8_t st_validator_testAntenna_Txldo(nci_data_t* exp,
        phTmlNfc_TransactInfo_t* act)
{
    uint8_t result = 0;
    uint8_t mesuredrange =0;
    long measured_val = 0;
    int tolerance = 0;

    if (NULL == exp || NULL == act)
    {
        return result;
    }

    NXPLOG_NCIHAL_D("st_validator_testAntenna_Txldo = 0x%x", act->pBuff[3]);
    if (0x05 == act->pBuff[2])
    {
        if (NFCSTATUS_SUCCESS == act->pBuff[3])
        {
            result = 1;
            NXPLOG_NCIHAL_D("Antenna: TxLDO current measured raw value in mA : 0x%x",
                            act->pBuff[4]);
            if (0x00 == act->pBuff[5])
            {
                NXPLOG_NCIHAL_D("Measured range : 0x00 = 50 - 100 mA");
                measured_val = ((0.40 * act->pBuff[4]) + 50);
                NXPLOG_NCIHAL_D("TxLDO current absolute value in mA = %ld",
                                measured_val);
            }
            else
            {
                NXPLOG_NCIHAL_D("Measured range : 0x01 = 20 - 70 mA");
                measured_val = ((0.40 * act->pBuff[4]) + 20);
                NXPLOG_NCIHAL_D("TxLDO current absolute value in mA = %ld",
                                measured_val);
            }

            tolerance = (phAntenna_resp.wTxdoMeasuredRangeMax  *
                         phAntenna_resp.wTxdoMeasuredTolerance) /
                        100;
            if ((measured_val <= phAntenna_resp.wTxdoMeasuredRangeMax + tolerance))
            {
                tolerance = (phAntenna_resp.wTxdoMeasuredRangeMin *
                             phAntenna_resp.wTxdoMeasuredTolerance) /
                            100;
                if ((measured_val >=
                        phAntenna_resp.wTxdoMeasuredRangeMin - tolerance))
                {
                    gtxldo_status = NFCSTATUS_SUCCESS;
                    NXPLOG_NCIHAL_E("Test Antenna Response for TxLDO measurement PASS");
                }
                else
                {
                    gtxldo_status = NFCSTATUS_FAILED;
                    NXPLOG_NCIHAL_E("Test Antenna Response for TxLDO measurement FAIL");
                }
            }
            else
            {
                gtxldo_status = NFCSTATUS_FAILED;
                NXPLOG_NCIHAL_E("Test Antenna Response for TxLDO measurement FAIL");
            }
        }
        else
        {
            gtxldo_status = NFCSTATUS_FAILED;
            NXPLOG_NCIHAL_E(
                "Test Antenna Response for TxLDO measurement failed: Invalid status");
        }

    }
    else
    {
        gtxldo_status = NFCSTATUS_FAILED;
        NXPLOG_NCIHAL_E(
            "Test Antenna Response for TxLDO measurement failed: Invalid payload "
            "length");
    }

    return result;
}

/*******************************************************************************
 **
 ** Function         st_validator_testAntenna_AgcVal
 **
 ** Description      Validator function reads AGC value of antenna and print the
 **                  info
 **
 ** Returns          One if successful otherwise Zero.
 **
 *******************************************************************************/
static uint8_t st_validator_testAntenna_AgcVal(nci_data_t* exp,
        phTmlNfc_TransactInfo_t* act)
{
    uint8_t result = 0;
    int agc_tolerance = 0;
    long agc_val = 0;

    if (NULL == exp || NULL == act)
    {
        return result;
    }

    if (0x05 == act->pBuff[2])
    {
        if (NFCSTATUS_SUCCESS == act->pBuff[3])
        {
            result = 1;
            agc_tolerance =
                (phAntenna_resp.wAgcValue * phAntenna_resp.wAgcValueTolerance) / 100;
            agc_val =  ((act->pBuff[5] << 8) | (act->pBuff[4]));
            NXPLOG_NCIHAL_D("AGC value : %ld", agc_val);
            if(((phAntenna_resp.wAgcValue - agc_tolerance) <= agc_val) &&
                    (agc_val <= (phAntenna_resp.wAgcValue + agc_tolerance)))
            {
                gagc_value_status = NFCSTATUS_SUCCESS;
                NXPLOG_NCIHAL_E("Test Antenna Response for AGC Values  PASS");
            }
            else
            {
                gagc_value_status = NFCSTATUS_FAILED;
                NXPLOG_NCIHAL_E("Test Antenna Response for AGC Values  FAIL");
            }
        }
        else
        {
            gagc_value_status = NFCSTATUS_FAILED;
            NXPLOG_NCIHAL_E("Test Antenna Response for AGC Values  FAIL");
        }
    }
    else
    {
        gagc_value_status = NFCSTATUS_FAILED;
        NXPLOG_NCIHAL_E(
            "Test Antenna Response for AGC value failed: Invalid payload length");
    }

    return result;
}
/*******************************************************************************
 **
 ** Function         st_validator_testAntenna_AgcVal_FixedNfcLd
 **
 ** Description      Validator function reads and print AGC value of
 **                  antenna with fixed NFCLD
 **
 ** Returns          One if successful otherwise Zero.
 **
 *******************************************************************************/
static uint8_t st_validator_testAntenna_AgcVal_FixedNfcLd(
    nci_data_t* exp, phTmlNfc_TransactInfo_t* act)
{
    uint8_t result = 0;
    int agc_nfcld_tolerance = 0;
    long agc_nfcld = 0;

    if (NULL == exp || NULL == act)
    {
        return result;
    }

    if (0x05 == act->pBuff[2])
    {
        if (NFCSTATUS_SUCCESS == act->pBuff[3])
        {
            result = 1;
            agc_nfcld_tolerance = (phAntenna_resp.wAgcValuewithfixedNFCLD *
                                   phAntenna_resp.wAgcValuewithfixedNFCLDTolerance) /
                                  100;
            agc_nfcld =  ((act->pBuff[5] << 8) | (act->pBuff[4]));
            NXPLOG_NCIHAL_D("AGC value with Fixed Nfcld  : %ld", agc_nfcld);

            if (((phAntenna_resp.wAgcValuewithfixedNFCLD - agc_nfcld_tolerance) <=
                    agc_nfcld) &&
                    (agc_nfcld <=
                     (phAntenna_resp.wAgcValuewithfixedNFCLD + agc_nfcld_tolerance)))
            {
                gagc_nfcld_status = NFCSTATUS_SUCCESS;
                NXPLOG_NCIHAL_E(
                    "Test Antenna Response for AGC value with fixed NFCLD PASS");
            }
            else
            {
                gagc_nfcld_status =  NFCSTATUS_FAILED;
                NXPLOG_NCIHAL_E(
                    "Test Antenna Response for AGC value with fixed NFCLD FAIL org Val = %d",
                    phAntenna_resp.wAgcValuewithfixedNFCLD);
            }
        }
        else
        {
            gagc_nfcld_status =  NFCSTATUS_FAILED;
            NXPLOG_NCIHAL_E(
                "Test Antenna Response for AGC value with fixed NFCLD failed: "
                "Invalid status");
        }
    }
    else
    {
        gagc_nfcld_status =  NFCSTATUS_FAILED;
        NXPLOG_NCIHAL_E(
            "Test Antenna Response for AGC value with fixed NFCLD failed: Invalid "
            "payload length");
    }

    return result;
}

/*******************************************************************************
 **
 ** Function         st_validator_testAntenna_AgcVal_Differential
 **
 ** Description      Reads the AGC value with open/short RM from buffer and print
 **
 ** Returns          One if successful otherwise Zero.
 **
 *******************************************************************************/
static uint8_t st_validator_testAntenna_AgcVal_Differential(
    nci_data_t* exp, phTmlNfc_TransactInfo_t* act)
{
    uint8_t result = 0;
    int agc_toleranceopne1 = 0;
    int agc_toleranceopne2 = 0;
    long agc_differentialOpne1 = 0;
    long agc_differentialOpne2 = 0;

    if (NULL == exp || NULL == act)
    {
        return result;
    }

    if (0x05 == act->pBuff[2])
    {
        if (NFCSTATUS_SUCCESS == act->pBuff[3])
        {
            result = 1;
            agc_toleranceopne1=(phAntenna_resp.wAgcDifferentialWithOpen1 *
                                phAntenna_resp.wAgcDifferentialWithOpenTolerance1) /
                               100;
            agc_toleranceopne2=(phAntenna_resp.wAgcDifferentialWithOpen2 *
                                phAntenna_resp.wAgcDifferentialWithOpenTolerance2) /
                               100;
            agc_differentialOpne1 =  ((act->pBuff[5] << 8) | (act->pBuff[4]));
            agc_differentialOpne2 =  ((act->pBuff[7] << 8) | (act->pBuff[6]));
            NXPLOG_NCIHAL_D("AGC value differential Opne 1  : %ld",
                            agc_differentialOpne1);
            NXPLOG_NCIHAL_D("AGC value differentialOpne  2 : %ld",
                            agc_differentialOpne2);

            if (((agc_differentialOpne1 >=
                    phAntenna_resp.wAgcDifferentialWithOpen1 - agc_toleranceopne1) &&
                    (agc_differentialOpne1 <=
                     phAntenna_resp.wAgcDifferentialWithOpen1 + agc_toleranceopne1)) &&
                    ((agc_differentialOpne2 >=
                      phAntenna_resp.wAgcDifferentialWithOpen2 - agc_toleranceopne2) &&
                     (agc_differentialOpne2 <=
                      phAntenna_resp.wAgcDifferentialWithOpen2 + agc_toleranceopne2)))
            {
                gagc_differential_status = NFCSTATUS_SUCCESS;
                NXPLOG_NCIHAL_E("Test Antenna Response for AGC Differential Open PASS");
            }
            else
            {
                gagc_differential_status = NFCSTATUS_FAILED;
                NXPLOG_NCIHAL_E(
                    "Test Antenna Response for AGC Differential Open  FAIL");
            }
        }
        else
        {
            NXPLOG_NCIHAL_E(
                "Test Antenna Response for AGC Differential failed: Invalid status");
            gagc_differential_status = NFCSTATUS_FAILED;
        }

    }
    else
    {
        NXPLOG_NCIHAL_E(
            "Test Antenna Response for AGC Differential failed: Invalid payload "
            "length");
        gagc_differential_status = NFCSTATUS_FAILED;
    }

    return result;
}
/*******************************************************************************
 **
 ** Function         st_validator_testEquals
 **
 ** Description      Validator function to validate for equality between actual
 **                  and expected values.
 **
 ** Returns          One if successful otherwise Zero.
 **
 *******************************************************************************/
static uint8_t st_validator_testEquals(nci_data_t* exp,
                                       phTmlNfc_TransactInfo_t* act)
{
    uint8_t result = 0;

    if (NULL == exp || NULL == act)
    {
        return result;
    }

    if(exp->len <= act->wLength &&
            (memcmp(exp->p_data, act->pBuff, exp->len) == 0))
    {
        result = 1;
    }

    return result;
}

/*******************************************************************************
 **
 ** Function         hal_write_rsp_timeout_cb
 **
 ** Description      Callback function for hal write response timer.
 **
 ** Returns          None
 **
 *******************************************************************************/
static void hal_write_rsp_timeout_cb(uint32_t timerId, void* pContext)
{
    UNUSED(timerId);
    NXPLOG_NCIHAL_E("hal_write_rsp_timeout_cb - write timeout!!!");
    hal_write_timer_fired = 1;
    hal_read_cb(pContext,NULL);
}

/*******************************************************************************
 **
 ** Function         hal_write_cb
 **
 ** Description      Callback function for hal write.
 **
 ** Returns          None
 **
 *******************************************************************************/
static void hal_write_cb(void* pContext, phTmlNfc_TransactInfo_t* pInfo)
{
    phNxpNciHal_Sem_t *p_cb_data = (phNxpNciHal_Sem_t*) pContext;
    if (pInfo->wStatus == NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("write successful status = 0x%x", pInfo->wStatus);
    }
    else
    {
        NXPLOG_NCIHAL_E("write error status = 0x%x", pInfo->wStatus);
    }

    p_cb_data->status = pInfo->wStatus;
    SEM_POST(p_cb_data);
    return;
}

/*******************************************************************************
 **
 ** Function         hal_read_cb
 **
 ** Description      Callback function for hal read.
 **
 ** Returns          None
 **
 *******************************************************************************/
static void hal_read_cb(void* pContext, phTmlNfc_TransactInfo_t* pInfo)
{
    NFCSTATUS status;
    phNxpNciHal_Sem_t* p_cb_data = NULL;

    if((pInfo == NULL) || (pContext == NULL))
    {
        NXPLOG_NCIHAL_E("Invalid params..! exiting...");
        return;
    }
    p_cb_data = (phNxpNciHal_Sem_t*)pContext;
    if (hal_write_timer_fired == 1)
    {
        NXPLOG_NCIHAL_D("hal_read_cb - response timeout occurred");

        hal_write_timer_fired = 0;
        p_cb_data->status = NFCSTATUS_RESPONSE_TIMEOUT;
        status = phTmlNfc_ReadAbort();
    }
    else
    {

        NFCSTATUS status = phOsalNfc_Timer_Stop(timeoutTimerId);
        if (NFCSTATUS_SUCCESS == status)
        {
            NXPLOG_NCIHAL_D("Response timer stopped");
        }
        else
        {
            NXPLOG_NCIHAL_E("Response timer stop ERROR!!!");
            p_cb_data->status  = NFCSTATUS_FAILED;
        }
        if (pInfo->wStatus == NFCSTATUS_SUCCESS)
        {
            NXPLOG_NCIHAL_D("hal_read_cb successful status = 0x%x", pInfo->wStatus);
            if(pInfo->pBuff[0] == NXP_NCI_MT_RSP && ((pInfo->pBuff[1] & NXP_NCI_OID_MASK) == NXP_NCI_MSG_CORE_RESET))
            {
                if(pInfo->pBuff[2] == NCI20_CORE_RESET_RSP_LEN && pInfo->pBuff[3] == NCI_CORE_RESET_STATUS_OK)
                {
                    NXPLOG_NCIHAL_D("CORE_RESET_RSP NCI2.0");
                    mSelfTestHdlr.wait_for_ntf = TRUE;
                }
                else if(pInfo->pBuff[2] == NCI10_CORE_RESET_RSP_LEN && pInfo->pBuff[3] == NCI_CORE_RESET_STATUS_OK)
                {
                    NXPLOG_NCIHAL_D("CORE_RESET_RSP NCI1.0");
                    mSelfTestHdlr.wait_for_ntf = FALSE;
                    mSelfTestHdlr.nci_version = pInfo->pBuff[4];
                }
            }
            else if (pInfo->pBuff[0] == NXP_NCI_MT_NTF && ((pInfo->pBuff[1] & NXP_NCI_OID_MASK) == NXP_NCI_MSG_CORE_RESET))
            {
                mSelfTestHdlr.mTransInfo.wStatus = pInfo->wStatus;
                mSelfTestHdlr.mTransInfo.wLength = pInfo->wLength;
                memcpy(mSelfTestHdlr.mTransInfo.pBuff, pInfo->pBuff, pInfo->wLength);
                mSelfTestHdlr.nci_version = pInfo->pBuff[5];
                mSelfTestHdlr.wait_for_ntf = FALSE;
            }
            else if(pInfo->pBuff[0] == NXP_NCI_MT_RSP && ((pInfo->pBuff[1] & NXP_NCI_OID_MASK) == NXP_NCI_MSG_CORE_INIT))
            {
                mSelfTestHdlr.wait_for_ntf = FALSE;
                if(mSelfTestHdlr.nci_version == NCI_VERSION_2_0)
                {
                    NXPLOG_NCIHAL_D("CORE_INIT_RSP NCI2.0 received !");
                }
                else
                {
                    mSelfTestHdlr.mTransInfo.wStatus = pInfo->wStatus;
                    mSelfTestHdlr.mTransInfo.wLength = pInfo->wLength;
                    memcpy(mSelfTestHdlr.mTransInfo.pBuff, pInfo->pBuff, pInfo->wLength);
                    NXPLOG_NCIHAL_D("CORE_INIT_RSP NCI1.0 received !");
                }
            }
            p_cb_data->status = NFCSTATUS_SUCCESS;
        }
        else
        {
            NXPLOG_NCIHAL_E("hal_read_cb error status = 0x%x", pInfo->wStatus);
            p_cb_data->status = NFCSTATUS_FAILED;
        }

        nci_test_data_t *test_data = (nci_test_data_t*) p_cb_data->pContext;
        if ((pInfo->pBuff[0] & NCI_MSG_TYPE_MASK) == NXP_NCI_MT_NTF)
        {
            /* Compare the actual notification with expected notification.*/
            if (test_data->ntf_validator(&(test_data->exp_ntf), pInfo) == 1)
            {
                p_cb_data->status = NFCSTATUS_SUCCESS;
            }
            else
            {
                p_cb_data->status = NFCSTATUS_FAILED;
            }
        }

        /* Compare the actual response with expected response.*/
        else if((pInfo->pBuff[0] & NCI_MSG_TYPE_MASK) == NXP_NCI_MT_RSP)
        {
            if (test_data->rsp_validator(&(test_data->exp_rsp), pInfo) == 1)
            {
                p_cb_data->status = NFCSTATUS_SUCCESS;
            }
            else
            {
                p_cb_data->status = NFCSTATUS_FAILED;
            }
        }
    }

    SEM_POST(p_cb_data);
    return;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_test_rx_thread
 **
 ** Description      Thread to fetch and process messages from message queue.
 **
 ** Returns          NULL
 **
 *******************************************************************************/
static void* phNxpNciHal_test_rx_thread(void* arg)
{
    phLibNfc_Message_t msg;
    UNUSED(arg);
    NXPLOG_NCIHAL_D("Self test thread started");

    thread_running = 1;

    while (thread_running == 1)
    {
        /* Fetch next message from the NFC stack message queue */
        if (phDal4Nfc_msgrcv(gDrvCfg.nClientId, &msg, 0, 0) == -1)
        {
            NXPLOG_NCIHAL_E("Received bad message");
            continue;
        }

        if (thread_running == 0)
        {
            break;
        }
        switch (msg.eMsgType)
        {
            case PH_LIBNFC_DEFERREDCALL_MSG:
            {
                phLibNfc_DeferredCall_t *deferCall =
                    (phLibNfc_DeferredCall_t *) (msg.pMsgData);

                REENTRANCE_LOCK();
                deferCall->pCallback(deferCall->pParameter);
                REENTRANCE_UNLOCK();
                break;
            }
            default:
                break;
        }
    }

    NXPLOG_NCIHAL_D("Self test thread stopped");

    pthread_exit(NULL);
    return NULL;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_readLocked
 **
 ** Description      Reads response and notification from NFCC and waits for
 **                  read completion, for a definitive timeout value.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED,
 **                  NFCSTATUS_RESPONSE_TIMEOUT in case of timeout.
 **
 *******************************************************************************/
static NFCSTATUS phNxpNciHal_readLocked(nci_test_data_t* pData)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    phNxpNciHal_Sem_t cb_data;
    uint16_t read_len = 16;
    /* RX Buffer */
    uint32_t rx_data[NCI_MAX_DATA_LEN];
    /* Create the local semaphore */
    if (phNxpNciHal_init_cb_data(&cb_data, pData) != NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("phTmlNfc_Read Create cb data failed");
        status = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

    /* call read pending */
    status =
        phTmlNfc_Read((uint8_t*)rx_data, (uint16_t)read_len,
                      (pphTmlNfc_TransactCompletionCb_t)&hal_read_cb, &cb_data);

    if (status != NFCSTATUS_PENDING)
    {
        NXPLOG_NCIHAL_E("TML Read status error status = %x", status);
        status = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

    status = phOsalNfc_Timer_Start(timeoutTimerId, HAL_WRITE_RSP_TIMEOUT,
                                   &hal_write_rsp_timeout_cb, &cb_data);

    if (NFCSTATUS_SUCCESS == status)
    {
        NXPLOG_NCIHAL_D("Response timer started");
    }
    else
    {
        NXPLOG_NCIHAL_E("Response timer not started");
        status = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

    /* Wait for callback response */
    if (SEM_WAIT(cb_data))
    {
        NXPLOG_NCIHAL_E("phTmlNfc_Read semaphore error");
        status = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

    if (cb_data.status == NFCSTATUS_RESPONSE_TIMEOUT)
    {
        NXPLOG_NCIHAL_E("Response timeout!!!");
        status = NFCSTATUS_RESPONSE_TIMEOUT;
        goto clean_and_return;
    }

    if (cb_data.status != NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_E("phTmlNfc_Read failed  ");
        status = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

clean_and_return:
    phNxpNciHal_cleanup_cb_data(&cb_data);
    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_writeLocked
 **
 ** Description      Send command to NFCC and waits for cmd write completion, for
 **                  a definitive timeout value.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED,
 **                  NFCSTATUS_RESPONSE_TIMEOUT in case of timeout.
 **
 *******************************************************************************/
static NFCSTATUS phNxpNciHal_writeLocked(nci_test_data_t* pData)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    phNxpNciHal_Sem_t cb_data;
    int retryCnt = 0;
    /* Create the local semaphore */
    if (phNxpNciHal_init_cb_data(&cb_data, NULL) != NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("phTmlNfc_Write Create cb data failed");
        goto clean_and_return;
    }

retry:
    status =
        phTmlNfc_Write(pData->cmd.p_data, pData->cmd.len,
                       (pphTmlNfc_TransactCompletionCb_t) &hal_write_cb, &cb_data);

    if (status != NFCSTATUS_PENDING)
    {
        NXPLOG_NCIHAL_E("phTmlNfc_Write status error");
        goto clean_and_return;
    }

    /* Wait for callback response */
    if (SEM_WAIT(cb_data))
    {
        NXPLOG_NCIHAL_E("write_unlocked semaphore error");
        status = NFCSTATUS_FAILED;
        goto clean_and_return;
    }

    if (cb_data.status != NFCSTATUS_SUCCESS && retryCnt < HAL_WRITE_MAX_RETRY)
    {
        retryCnt++;
        NXPLOG_NCIHAL_E(
            "write_unlocked failed - PN54X Maybe in Standby Mode - Retry %d",
            retryCnt);
        goto retry;
    }

    status = cb_data.status;

clean_and_return:
    phNxpNciHal_cleanup_cb_data(&cb_data);
    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_performTest
 **
 ** Description      Performs a single cycle of command,response and
 **                  notification.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED,
 **
 *******************************************************************************/
NFCSTATUS phNxpNciHal_performTest(nci_test_data_t* pData)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;

    if (NULL == pData)
    {
        return NFCSTATUS_FAILED;
    }

    CONCURRENCY_LOCK();

    status = phNxpNciHal_writeLocked(pData);

    if (status == NFCSTATUS_RESPONSE_TIMEOUT)
    {
        goto clean_and_return;
    }
    if (status != NFCSTATUS_SUCCESS)
    {
        goto clean_and_return;
    }

    status = phNxpNciHal_readLocked(pData);

    if (status != NFCSTATUS_SUCCESS)
    {
        goto clean_and_return;
    }

    if (0 != pData->exp_ntf.len)
    {
        status = phNxpNciHal_readLocked(pData);

        if (status != NFCSTATUS_SUCCESS)
        {
            goto clean_and_return;
        }
    }

clean_and_return:
    CONCURRENCY_UNLOCK();
    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_TestMode_open
 **
 ** Description      It opens the physical connection with NFCC (PN54X) and
 **                  creates required client thread for operation.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED.
 **
 *******************************************************************************/
NFCSTATUS phNxpNciHal_TestMode_open (void)
{
    /* Thread */
    pthread_t test_rx_thread;

    phOsalNfc_Config_t tOsalConfig;
    phTmlNfc_Config_t tTmlConfig;
    uint8_t* nfc_dev_node = NULL;
    const uint16_t max_len = 260; /* device node name is max of 255 bytes + 5 bytes (/dev/) */
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    uint16_t read_len = 255;
    int8_t ret_val = 0x00;
    mSelfTestHdlr.mTransInfo.pBuff  = (uint8_t *)malloc(max_len * sizeof(uint8_t));
    if(mSelfTestHdlr.mTransInfo.pBuff == NULL)
    {
        NXPLOG_NCIHAL_E("Error ! memory not allocated.");
        return NFCSTATUS_FAILED;
    }
    /* initialize trace level */
    phNxpLog_InitializeLogLevel();

    if (phNxpNciHal_init_monitor() == NULL)
    {
        NXPLOG_NCIHAL_E("Init monitor failed");
        return NFCSTATUS_FAILED;
    }

    CONCURRENCY_LOCK();

    memset(&tOsalConfig, 0x00, sizeof(tOsalConfig));
    memset(&tTmlConfig, 0x00, sizeof(tTmlConfig));

    /* Read the nfc device node name */
    nfc_dev_node = (uint8_t*)malloc(max_len * sizeof(uint8_t));
    if (nfc_dev_node == NULL)
    {
        NXPLOG_NCIHAL_E("malloc of nfc_dev_node failed ");
        goto clean_and_return;
    }
    else if (!GetNxpStrValue(NAME_NXP_NFC_DEV_NODE, (char*)nfc_dev_node,
                             max_len))
    {
        NXPLOG_NCIHAL_E(
            "Invalid nfc device node name keeping the default device node "
            "/dev/pn54x");
        strcpy((char*)nfc_dev_node, "/dev/pn54x");
    }

    gDrvCfg.nClientId = phDal4Nfc_msgget(0, 0600);
    gDrvCfg.nLinkType = ENUM_LINK_TYPE_I2C;/* For PN54X */
    tTmlConfig.pDevName = (int8_t*)nfc_dev_node;
    tOsalConfig.dwCallbackThreadId = (uintptr_t) gDrvCfg.nClientId;
    tOsalConfig.pLogFile = NULL;
    tTmlConfig.dwGetMsgThreadId = (uintptr_t) gDrvCfg.nClientId;
    nxpncihal_ctrl.gDrvCfg.nClientId = (uintptr_t) gDrvCfg.nClientId;

    /* Initialize TML layer */
    status = phTmlNfc_Init(&tTmlConfig);
    if (status != NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_E("phTmlNfc_Init Failed");
        goto clean_and_return;
    }
    else
    {
        if (nfc_dev_node != NULL)
        {
            free(nfc_dev_node);
            nfc_dev_node = NULL;
        }
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret_val =
        pthread_create(&test_rx_thread, &attr, phNxpNciHal_test_rx_thread, NULL);
    pthread_attr_destroy(&attr);
    if (ret_val != 0)
    {
        NXPLOG_NCIHAL_E("pthread_create failed");
        phTmlNfc_Shutdown();
        goto clean_and_return;
    }

    if(pthread_setname_np(test_rx_thread,"HAL_TESTMODE_RX"))
    {
        NXPLOG_NCIHAL_E("pthread_setname_np failed");
    }

    timeoutTimerId = phOsalNfc_Timer_Create();

    if(phNxpNciHal_initialize_chipType() != NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_E("Chip initialization failed");
        goto clean_and_return;
    }

    if (timeoutTimerId == 0xFFFF)
    {
        NXPLOG_NCIHAL_E("phOsalNfc_Timer_Create failed");
    }
    else
    {
        NXPLOG_NCIHAL_D("phOsalNfc_Timer_Create SUCCESS");
    }
    CONCURRENCY_UNLOCK();

    return NFCSTATUS_SUCCESS;

clean_and_return:
    CONCURRENCY_UNLOCK();
    if (nfc_dev_node != NULL)
    {
        free(nfc_dev_node);
        nfc_dev_node = NULL;
    }
    phNxpNciHal_cleanup_monitor();
    return NFCSTATUS_FAILED;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_initialize_chipType
 **
 ** Description      This function Initializes the NFCC chip type and
 **                  Feature list flags
 **
 ** Returns          None.
 **
 ******************************************************************************/
NFCSTATUS phNxpNciHal_initialize_chipType()
{
    nci_test_data_t* pCoreResetCmd = &swp1_test_data[0];
    nci_test_data_t* pCoreInitCmd = &swp1_test_data[1];
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    NXPLOG_NCIHAL_D("phNxpNciHal_initialize_chipType() : entry");

    //CORE RESET COMMAND
    if(!phNxpNciHal_writeLocked(pCoreResetCmd))
    {
        if(phNxpNciHal_readLocked(pCoreResetCmd) != NFCSTATUS_SUCCESS)
        {
            NXPLOG_NCIHAL_D("Response not received for CORE RESET command");
        }

        if(mSelfTestHdlr.wait_for_ntf)
        {
            pCoreResetCmd->exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
            memcpy(pCoreResetCmd->exp_rsp.p_data, nfcc_core_reset_nci20_rsp,pCoreResetCmd->exp_rsp.len);
            pCoreResetCmd->exp_ntf.len = (sizeof(nfcc_core_reset_nci20_ntf) / sizeof(nfcc_core_reset_nci20_ntf[0]));
            memcpy(pCoreResetCmd->exp_ntf.p_data, nfcc_core_reset_nci20_ntf, pCoreResetCmd->exp_ntf.len);
            if(phNxpNciHal_readLocked(pCoreResetCmd) != NFCSTATUS_SUCCESS)
            {
                NXPLOG_NCIHAL_D("NCI 2.0 Notification is not received");
                status = NFCSTATUS_FAILED;
            }
            else
            {
            	phNxpNciHal_deriveChipType(mSelfTestHdlr.mTransInfo.pBuff,mSelfTestHdlr.mTransInfo.wLength);
            }
        }
    }
    else
    {
        NXPLOG_NCIHAL_E("CORE RESET Write failed");
        return NFCSTATUS_FAILED;
    }

    //CORE INIT COMMAND
    if(mSelfTestHdlr.nci_version == NCI_VERSION_2_0)
    {
        pCoreInitCmd->cmd.len = NFCC_CMD_NCI20_LEN;
        memcpy(pCoreInitCmd->cmd.p_data, nfcc_core_init_nci20_cmd, NFCC_CMD_NCI20_LEN);
        pCoreInitCmd->exp_rsp.len = sizeof(nfcc_core_init_nci20_rsp);
        memcpy(pCoreInitCmd->exp_rsp.p_data, nfcc_core_init_nci20_rsp,pCoreInitCmd->exp_rsp.len);
        status = phNxpNciHal_writeLocked(pCoreInitCmd);
    }
    else
    {
        pCoreInitCmd->cmd.len = NFCC_CMD_NCI10_LEN;
        memcpy(pCoreInitCmd->cmd.p_data, nfcc_core_init_nci10_cmd, NFCC_CMD_NCI10_LEN);
        status = phNxpNciHal_writeLocked(pCoreInitCmd);
    }
    if(!status)
    {
        if(phNxpNciHal_readLocked(pCoreInitCmd)  != NFCSTATUS_SUCCESS)
        {
            NXPLOG_NCIHAL_D("Response not received for CORE INIT command");
            status = NFCSTATUS_FAILED;
        }
        else
        {
            if(mSelfTestHdlr.nci_version != NCI_VERSION_2_0)
            {
            	phNxpNciHal_deriveChipType(mSelfTestHdlr.mTransInfo.pBuff,mSelfTestHdlr.mTransInfo.wLength);
            }
        }
    }
    else
        status = NFCSTATUS_FAILED;
    if(!status && (phNxpNciHal_getChipType() == pn557) && (mSelfTestHdlr.nci_version != NCI_VERSION_2_0))
    {
        NXPLOG_NCIHAL_E("Chip is in NCI1.0 mode reset the chip to 2.0 mode");
        if(!phNxpNciHal_writeLocked(pCoreResetCmd))
        {
            if(phNxpNciHal_readLocked(pCoreResetCmd) != NFCSTATUS_SUCCESS)
            {
                NXPLOG_NCIHAL_D("Response not received for CORE RESET command");
                status = NFCSTATUS_FAILED;
            }
            if(mSelfTestHdlr.wait_for_ntf)
            {
                if(phNxpNciHal_readLocked(pCoreResetCmd) != NFCSTATUS_SUCCESS)
                {
                    NXPLOG_NCIHAL_D("NCI 2.0 Notification is not received");
                    status = NFCSTATUS_FAILED;
                }
            }
        }
        else
            status = NFCSTATUS_FAILED;
        pCoreInitCmd->cmd.len = NFCC_CMD_NCI20_LEN;
        memcpy(pCoreInitCmd->cmd.p_data, nfcc_core_init_nci20_cmd, NFCC_CMD_NCI20_LEN);
        pCoreInitCmd->exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
        memcpy(pCoreInitCmd->exp_rsp.p_data, nfcc_core_reset_nci20_rsp,pCoreInitCmd->exp_rsp.len);
        if(!phNxpNciHal_writeLocked(pCoreInitCmd))
        {
            if(phNxpNciHal_readLocked(pCoreInitCmd) != NFCSTATUS_SUCCESS)
            {
                NXPLOG_NCIHAL_D("Response not received for CORE RESET command");
                status = NFCSTATUS_FAILED;
            }
        }
        else
            status = NFCSTATUS_FAILED;
    }
    NXPLOG_NCIHAL_D("phNxpNciHal_initialize_chipType() : exit chipType = 0x%02X",phNxpNciHal_getChipType());
    return status;
}
/*******************************************************************************
 **
 ** Function         phNxpNciHal_TestMode_close
 **
 ** Description      This function close the NFCC interface and free all
 **                  resources.
 **
 ** Returns          None.
 **
 *******************************************************************************/

void phNxpNciHal_TestMode_close ()
{

    NFCSTATUS status = NFCSTATUS_SUCCESS;

    CONCURRENCY_LOCK();

    if (NULL != gpphTmlNfc_Context->pDevHandle)
    {
        /* Abort any pending read and write */
        status = phTmlNfc_ReadAbort();
        status = phTmlNfc_WriteAbort();

        phOsalNfc_Timer_Cleanup();

        status = phTmlNfc_Shutdown();

        NXPLOG_NCIHAL_D("phNxpNciHal_close return status = %d", status);

        thread_running = 0;

        phDal4Nfc_msgrelease(gDrvCfg.nClientId);

        status = phOsalNfc_Timer_Delete(timeoutTimerId);
    }

    free(mSelfTestHdlr.mTransInfo.pBuff);
    mSelfTestHdlr.mTransInfo.pBuff = NULL;
    CONCURRENCY_UNLOCK();

    phNxpNciHal_cleanup_monitor();

    /* Return success always */
    return;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_SwpTest
 **
 ** Description      Test function to validate the SWP line. SWP line number is
 **                  is sent as parameter to the API.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED.
 **
 *******************************************************************************/

NFCSTATUS phNxpNciHal_SwpTest(uint8_t swp_line)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    int len = 0;
    int cnt = 0;

    NXPLOG_NCIHAL_D("phNxpNciHal_SwpTest - start\n");

    if (swp_line == 0x01)
    {
        swp1_test_data[0].exp_rsp.p_data[4] = NXP_NFCC_RESET_RSP_LEN;
        swp1_test_data[1].exp_rsp.p_data[2] = NFCC_EXP_RES_DATA_BYTE2;
        if(phNxpNciHal_getChipType() == pn557)
        {
            swp1_test_data[0].exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
            memcpy(swp1_test_data[0].exp_rsp.p_data, nfcc_core_reset_nci20_rsp,swp1_test_data[0].exp_rsp.len);
            swp1_test_data[0].exp_ntf.len = (sizeof(nfcc_core_reset_nci20_ntf) / sizeof(nfcc_core_reset_nci20_ntf[0]));
            memcpy(swp1_test_data[0].exp_ntf.p_data, nfcc_core_reset_nci20_ntf, swp1_test_data[0].exp_ntf.len);
            swp1_test_data[1].cmd.len = NFCC_CMD_NCI20_LEN;
            memcpy(swp1_test_data[1].cmd.p_data, nfcc_core_init_nci20_cmd, NFCC_CMD_NCI20_LEN);
            swp1_test_data[1].exp_rsp.len = sizeof(nfcc_core_init_nci20_rsp);
            memcpy(swp1_test_data[1].exp_rsp.p_data, nfcc_core_init_nci20_rsp, swp1_test_data[1].exp_rsp.len);
        }
        else
        {
            swp1_test_data[1].cmd.len = NFCC_CMD_NCI10_LEN;
            memcpy(swp1_test_data[1].cmd.p_data, nfcc_core_init_nci10_cmd, NFCC_CMD_NCI10_LEN);
        }
        len = (sizeof(swp1_test_data)/sizeof(swp1_test_data[0]));
        for (cnt = 0; cnt < len; cnt++)
        {
            status = phNxpNciHal_performTest(&(swp1_test_data[cnt]));
            if (status == NFCSTATUS_RESPONSE_TIMEOUT || status == NFCSTATUS_FAILED)
            {
                break;
            }
        }
    }
    else if (swp_line == 0x02)
    {
        swp2_test_data[0].exp_rsp.p_data[4] = NXP_NFCC_RESET_RSP_LEN;
        swp2_test_data[1].exp_rsp.p_data[2] = NFCC_EXP_RES_DATA_BYTE2;
        if(phNxpNciHal_getChipType() == pn557)
        {
            swp2_test_data[0].exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
            memcpy(swp2_test_data[0].exp_rsp.p_data, nfcc_core_reset_nci20_rsp,swp2_test_data[0].exp_rsp.len);
            swp2_test_data[0].exp_ntf.len = (sizeof(nfcc_core_reset_nci20_ntf) / sizeof(nfcc_core_reset_nci20_ntf[0]));
            memcpy(swp2_test_data[0].exp_ntf.p_data, nfcc_core_reset_nci20_ntf, swp2_test_data[0].exp_ntf.len);
            swp2_test_data[1].cmd.len = NFCC_CMD_NCI20_LEN;
            memcpy(swp2_test_data[1].cmd.p_data, nfcc_core_init_nci20_cmd, NFCC_CMD_NCI20_LEN);
            swp2_test_data[1].exp_rsp.len = sizeof(nfcc_core_init_nci20_rsp);
            memcpy(swp2_test_data[1].exp_rsp.p_data, nfcc_core_init_nci20_rsp, swp2_test_data[1].exp_rsp.len);
        }
        else
        {
            swp2_test_data[1].cmd.len = NFCC_CMD_NCI10_LEN;
            memcpy(swp2_test_data[1].cmd.p_data, nfcc_core_init_nci10_cmd, NFCC_CMD_NCI10_LEN);
        }
        len = (sizeof(swp2_test_data)/sizeof(swp2_test_data[0]));

        for (cnt = 0; cnt < len; cnt++)
        {
            status = phNxpNciHal_performTest(&(swp2_test_data[cnt]));
            if (status == NFCSTATUS_RESPONSE_TIMEOUT || status == NFCSTATUS_FAILED)
            {
                break;
            }
        }
    }
    else
    {
        status = NFCSTATUS_FAILED;
    }

    if (status == NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_SwpTest - SUCCESSS\n");
    }
    else
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_SwpTest - FAILED\n");
    }

    NXPLOG_NCIHAL_D("phNxpNciHal_SwpTest - end\n");

    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_PrbsTestStart
 **
 ** Description      Test function start RF generation for RF technology and bit
 **                  rate. RF technology and bit rate are sent as parameter to
 **                  the API.
 **
 ** Returns          NFCSTATUS_SUCCESS if RF generation successful,
 **                  otherwise NFCSTATUS_FAILED.
 **
 *******************************************************************************/

NFCSTATUS phNxpNciHal_PrbsTestStart(phNxpNfc_PrbsType_t prbs_type,
                                    phNxpNfc_PrbsHwType_t hw_prbs_type,
                                    phNxpNfc_Tech_t tech,
                                    phNxpNfc_Bitrate_t bitrate)
{
    NFCSTATUS status = NFCSTATUS_FAILED;

    uint8_t rsp_cmd_info[] = {0x4F, 0x30, 0x01, 0x00};

    nci_test_data_t prbs_cmd_data;

    if (phNxpNciHal_getChipType() != pn547C2)
    {
        prbs_cmd_data.cmd.len = 0x09;
    }
    else
    {
        prbs_cmd_data.cmd.len = 0x07;
    }

    memcpy(prbs_cmd_data.exp_rsp.p_data, &rsp_cmd_info[0], sizeof(rsp_cmd_info));
    prbs_cmd_data.exp_rsp.len = sizeof(rsp_cmd_info);

    //prbs_cmd_data.exp_rsp.len = 0x00;
    prbs_cmd_data.exp_ntf.len = 0x00;
    prbs_cmd_data.rsp_validator = st_validator_testEquals;
    prbs_cmd_data.ntf_validator = st_validator_null;

    uint8_t len = 0;
    uint8_t cnt = 0;

    //    [NCI] -> [0x2F 0x30 0x04 0x00 0x00 0x01 0xFF]

    status = phNxpNciHal_getPrbsCmd(prbs_type, hw_prbs_type, tech, bitrate,
                                    prbs_cmd_data.cmd.p_data,prbs_cmd_data.cmd.len);

    if( status == NFCSTATUS_FAILED)
    {
        //Invalid Param.
        NXPLOG_NCIHAL_D("phNxpNciHal_PrbsTestStart - INVALID_PARAM\n");

        goto clean_and_return;
    }
    prbs_test_data[len].exp_rsp.p_data[4] = NXP_NFCC_RESET_RSP_LEN;
    if(phNxpNciHal_getChipType() == pn557)
    {
        prbs_test_data[len].exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
        memcpy(prbs_test_data[len].exp_rsp.p_data, nfcc_core_reset_nci20_rsp,prbs_test_data[len].exp_rsp.len);
        prbs_test_data[len].exp_ntf.len = (sizeof(nfcc_core_reset_nci20_ntf) / sizeof(nfcc_core_reset_nci20_ntf[len]));
        memcpy(prbs_test_data[len].exp_ntf.p_data, nfcc_core_reset_nci20_ntf, prbs_test_data[len].exp_ntf.len);
    }
    len++;//val 1
    prbs_test_data[len].exp_rsp.p_data[2] = NFCC_EXP_RES_DATA_BYTE2;
    if(phNxpNciHal_getChipType() == pn557)
    {
        prbs_test_data[len].cmd.len = NFCC_CMD_NCI20_LEN;
        memcpy(prbs_test_data[len].cmd.p_data, nfcc_core_init_nci20_cmd, NFCC_CMD_NCI20_LEN);
        prbs_test_data[len].exp_rsp.len = sizeof(nfcc_core_init_nci20_rsp);
        memcpy(prbs_test_data[len].exp_rsp.p_data, nfcc_core_init_nci20_rsp, prbs_test_data[len].exp_rsp.len);
    }
    else
    {
        prbs_test_data[len].cmd.len = NFCC_CMD_NCI10_LEN;
        memcpy(prbs_test_data[len].cmd.p_data, nfcc_core_init_nci10_cmd, NFCC_CMD_NCI10_LEN);
    }
    len++;//val 2
    if(phNxpNciHal_getChipType() == pn547C2)
    {
        prbs_test_data[len].cmd.len = sizeof(nfcc_power_mgt_cmd);
        memcpy(prbs_test_data[len].cmd.p_data, nfcc_power_mgt_cmd, prbs_test_data[len].cmd.len);
        prbs_test_data[len].exp_rsp.len = sizeof(nfcc_power_mgt_rsp);
        memcpy(prbs_test_data[2].exp_rsp.p_data, nfcc_power_mgt_rsp, prbs_test_data[len].cmd.len);
        prbs_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
        prbs_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
        prbs_test_data[len].rsp_validator = st_validator_testEquals;
        prbs_test_data[len].ntf_validator = st_validator_null;
        len++;//val 3
    }

    for (cnt = 0; cnt < len; cnt++)
    {
        status = phNxpNciHal_performTest(&(prbs_test_data[cnt]));
        if (status == NFCSTATUS_RESPONSE_TIMEOUT || status == NFCSTATUS_FAILED)
        {
            break;
        }
    }

    /* Ignoring status, as there will be no response - Applicable till FW version
     * 8.1.1*/
    status = phNxpNciHal_performTest(&prbs_cmd_data);
clean_and_return:

    if( status == NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_PrbsTestStart - SUCCESSS\n");
    }
    else
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_PrbsTestStart - FAILED\n");
    }

    NXPLOG_NCIHAL_D("phNxpNciHal_PrbsTestStart - end\n");

    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_PrbsTestStop
 **
 ** Description      Test function stop RF generation for RF technology started
 **                  by phNxpNciHal_PrbsTestStart.
 **
 ** Returns          NFCSTATUS_SUCCESS if operation successful,
 **                  otherwise NFCSTATUS_FAILED.
 **
 *******************************************************************************/

NFCSTATUS phNxpNciHal_PrbsTestStop ()
{
    NXPLOG_NCIHAL_D("phNxpNciHal_PrbsTestStop - Start\n");

    NFCSTATUS status = NFCSTATUS_SUCCESS;

    status = phTmlNfc_IoCtl(phTmlNfc_e_ResetDevice);

    if(NFCSTATUS_SUCCESS == status)
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_PrbsTestStop - SUCCESS\n");
    }
    else
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_PrbsTestStop - FAILED\n");

    }
    NXPLOG_NCIHAL_D("phNxpNciHal_PrbsTestStop - end\n");

    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_getPrbsCmd
 **
 ** Description      Test function frames the PRBS command.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED.
 **
 *******************************************************************************/
NFCSTATUS phNxpNciHal_getPrbsCmd(phNxpNfc_PrbsType_t prbs_type,
                                 phNxpNfc_PrbsHwType_t hw_prbs_type,
                                 uint8_t tech, uint8_t bitrate,
                                 uint8_t* prbs_cmd, uint8_t prbs_cmd_len)

{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    int position_tech_param = 0;
    int position_bit_param = 0;
    int prbs_cmd_size = 0;
    unsigned int prbs_index=0;

    NXPLOG_NCIHAL_D("phNxpNciHal_getPrbsCmd - tech 0x%x bitrate = 0x%x", tech, bitrate);

    if (phNxpNciHal_getChipType() != pn547C2)
    {
        prbs_cmd_size = 0x09;
    }
    else
    {
        prbs_cmd_size = 0x07;
    }

    if(NULL == prbs_cmd || (prbs_cmd_len != prbs_cmd_size))
    {
        return status;
    }

    prbs_cmd[prbs_index++] = 0x2F;
    prbs_cmd[prbs_index++] = 0x30;
    prbs_cmd[prbs_index++] = prbs_cmd_size - 3;
    if (phNxpNciHal_getChipType() != pn547C2)
    {
        prbs_cmd[prbs_index++] = (uint8_t)prbs_type;
        //0xFF Error value used for validation.
        prbs_cmd[prbs_index++] = (uint8_t)hw_prbs_type;
    }
    position_tech_param = prbs_index;
    prbs_cmd[prbs_index++] = 0xFF;//TECH
    position_bit_param = prbs_index;
    prbs_cmd[prbs_index++] = 0xFF;//BITRATE
    prbs_cmd[prbs_index++] = 0x01;
    prbs_cmd[prbs_index++] = 0xFF;

    switch (tech)
    {
        case NFC_RF_TECHNOLOGY_A:
            NXPLOG_NCIHAL_D("phNxpNciHal_getPrbsCmd - NFC_RF_TECHNOLOGY_A");
            prbs_cmd[position_tech_param] = 0x00;
            break;
        case NFC_RF_TECHNOLOGY_B:
            NXPLOG_NCIHAL_D("phNxpNciHal_getPrbsCmd - NFC_RF_TECHNOLOGY_B");
            prbs_cmd[position_tech_param] = 0x01;
            break;
        case NFC_RF_TECHNOLOGY_F:
            NXPLOG_NCIHAL_D("phNxpNciHal_getPrbsCmd - NFC_RF_TECHNOLOGY_F");
            prbs_cmd[position_tech_param] = 0x02;
            break;
        default:
            break;
    }

    switch (bitrate)
    {
        case NFC_BIT_RATE_106:
            NXPLOG_NCIHAL_D("phNxpNciHal_getPrbsCmd - NFC_BIT_RATE_106");
            if(prbs_cmd[position_tech_param] != 0x02)
            {
                prbs_cmd[position_bit_param] = 0x00;
            }
            break;
        case NFC_BIT_RATE_212:
            NXPLOG_NCIHAL_D("phNxpNciHal_getPrbsCmd - NFC_BIT_RATE_212");
            prbs_cmd[position_bit_param] = 0x01;
            break;
        case NFC_BIT_RATE_424:
            NXPLOG_NCIHAL_D("phNxpNciHal_getPrbsCmd - NFC_BIT_RATE_424");
            prbs_cmd[position_bit_param] = 0x02;
            break;
        case NFC_BIT_RATE_848:
            NXPLOG_NCIHAL_D("phNxpNciHal_getPrbsCmd - NFC_BIT_RATE_848");
            if(prbs_cmd[position_tech_param] != 0x02)
            {
                prbs_cmd[position_bit_param] = 0x03;
            }
            break;
        default:
            break;
    }

    if(prbs_cmd[position_tech_param] == 0xFF || prbs_cmd[position_bit_param] == 0xFF)
    {
        //Invalid Param.
        status = NFCSTATUS_FAILED;
    }

    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_RfFieldTest
 **
 ** Description      Test function performs RF filed test.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED.
 **
 *******************************************************************************/
NFCSTATUS phNxpNciHal_RfFieldTest (uint8_t on)
{
    NFCSTATUS status = NFCSTATUS_SUCCESS;
    int len = 0;
    int cnt = 0;
    uint8_t nfcc_act_cmd[] = SYSTEM_PROPRIETARY_ACT_CMD;
    uint8_t nfcc_act_exp_rsp[] = SYSTEM_PROPRIETARY_ACT_RSP;
    uint8_t nfcc_antena_cmd1[] = SYSTEM_TEST_ANTENNA_CMD_1;
    uint8_t nfcc_antena_cmd2[] = SYSTEM_TEST_ANTENNA_CMD_2;
    uint8_t nfcc_antena_rsp1[] = SYSTEM_TEST_ANTENNA_RSP_1;
    uint8_t nfcc_power_mgt_cmd_1[] = SYSTEM_SET_POWERMGT_CMD_1;
    uint8_t nfcc_antena_cmd3[] = SYSTEM_TEST_ANTENNA_CMD_3;
    uint8_t nfcc_antena_cmd4[] = SYSTEM_TEST_ANTENNA_CMD_4;
    NXPLOG_NCIHAL_D("phNxpNciHal_RfFieldTest - start %x\n",on);

    if (on == 0x01)
    {
        rf_field_on_test_data[len].exp_rsp.p_data[4] = NXP_NFCC_RESET_RSP_LEN;
        if(phNxpNciHal_getChipType() == pn557)
        {
            rf_field_on_test_data[len].exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
            memcpy(rf_field_on_test_data[len].exp_rsp.p_data, nfcc_core_reset_nci20_rsp,rf_field_on_test_data[len].exp_rsp.len);
            rf_field_on_test_data[len].exp_ntf.len = sizeof(nfcc_core_reset_nci20_ntf);
            memcpy(rf_field_on_test_data[len].exp_ntf.p_data, nfcc_core_reset_nci20_ntf, rf_field_on_test_data[len].exp_ntf.len);
        }
        len++;// len val 1
        rf_field_on_test_data[len].exp_rsp.p_data[2] = NFCC_EXP_RES_DATA_BYTE2;
        if(phNxpNciHal_getChipType() == pn557)
        {
            rf_field_on_test_data[len].cmd.len = NFCC_CMD_NCI20_LEN;
            memcpy(rf_field_on_test_data[len].cmd.p_data, nfcc_core_init_nci20_cmd, NFCC_CMD_NCI20_LEN);
            rf_field_on_test_data[len].exp_rsp.len = sizeof(nfcc_core_init_nci20_rsp);
            memcpy(rf_field_on_test_data[len].exp_rsp.p_data, nfcc_core_init_nci20_rsp, rf_field_on_test_data[len].exp_rsp.len);
        }
        else
        {
            rf_field_on_test_data[len].cmd.len = NFCC_CMD_NCI10_LEN;
            memcpy(rf_field_on_test_data[len].cmd.p_data, nfcc_core_init_nci10_cmd, NFCC_CMD_NCI10_LEN);
        }
        len++; //len val 2
        if(phNxpNciHal_getChipType() != pn547C2)
        {
            rf_field_on_test_data[len].cmd.len = sizeof(nfcc_act_cmd);
            memcpy(rf_field_on_test_data[len].cmd.p_data, nfcc_act_cmd, rf_field_on_test_data[len].cmd.len);
            rf_field_on_test_data[len].exp_rsp.len = sizeof(nfcc_act_exp_rsp);
            memcpy(rf_field_on_test_data[len].exp_rsp.p_data, nfcc_act_exp_rsp, rf_field_on_test_data[len].cmd.len);
            rf_field_on_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
            rf_field_on_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
            rf_field_on_test_data[len].rsp_validator = st_validator_testEquals;
            rf_field_on_test_data[len].ntf_validator = st_validator_null;
            len++; //len val 3

            rf_field_on_test_data[len].cmd.len = sizeof(nfcc_power_mgt_cmd);
            memcpy(rf_field_on_test_data[len].cmd.p_data, nfcc_power_mgt_cmd, rf_field_on_test_data[len].cmd.len);
            rf_field_on_test_data[len].exp_rsp.len = sizeof(nfcc_power_mgt_rsp);
            memcpy(rf_field_on_test_data[len].exp_rsp.p_data, nfcc_power_mgt_rsp, rf_field_on_test_data[len].cmd.len);
            rf_field_on_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
            rf_field_on_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
            rf_field_on_test_data[len].rsp_validator = st_validator_testEquals;
            rf_field_on_test_data[len].ntf_validator = st_validator_null;
            len++; //len val 4
        }
        if(phNxpNciHal_getChipType() != pn547C2)
        {
            rf_field_on_test_data[len].cmd.len = sizeof(nfcc_antena_cmd2);
            memcpy(rf_field_on_test_data[len].cmd.p_data, nfcc_antena_cmd2, rf_field_on_test_data[len].cmd.len);
        }
        else
        {
            rf_field_on_test_data[len].cmd.len = sizeof(nfcc_antena_cmd1);
            memcpy(rf_field_on_test_data[len].cmd.p_data, nfcc_antena_cmd1, rf_field_on_test_data[len].cmd.len);
        }
        rf_field_on_test_data[len].exp_rsp.len = sizeof(nfcc_antena_rsp1);
        memcpy(rf_field_on_test_data[len].exp_rsp.p_data, nfcc_antena_rsp1, rf_field_on_test_data[len].cmd.len);
        rf_field_on_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
        rf_field_on_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
        rf_field_on_test_data[len].rsp_validator = st_validator_testEquals;
        rf_field_on_test_data[len].ntf_validator = st_validator_null;
        len++; //len val 5
        if(phNxpNciHal_getChipType() != pn547C2)
        {
            rf_field_on_test_data[len].cmd.len = sizeof(nfcc_power_mgt_cmd_1);
            memcpy(rf_field_on_test_data[len].cmd.p_data, nfcc_power_mgt_cmd_1, rf_field_on_test_data[len].cmd.len);
            rf_field_on_test_data[len].exp_rsp.len = sizeof(nfcc_power_mgt_rsp);
            memcpy(rf_field_on_test_data[len].exp_rsp.p_data, nfcc_power_mgt_rsp, rf_field_on_test_data[len].cmd.len);
            rf_field_on_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
            rf_field_on_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
            rf_field_on_test_data[len].rsp_validator = st_validator_testEquals;
            rf_field_on_test_data[len].ntf_validator = st_validator_null;
            len++; //len val 6
        }

        for (cnt = 0; cnt < len; cnt++)
        {
            status = phNxpNciHal_performTest(&(rf_field_on_test_data[cnt]));
            if (status == NFCSTATUS_RESPONSE_TIMEOUT || status == NFCSTATUS_FAILED)
            {
                break;
            }
        }
    }
    else if (on == 0x00)
    {
        rf_field_off_test_data[len].exp_rsp.p_data[4] = NXP_NFCC_RESET_RSP_LEN;
        if(phNxpNciHal_getChipType() == pn557)
        {
            rf_field_off_test_data[len].exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
            memcpy(rf_field_off_test_data[len].exp_rsp.p_data, nfcc_core_reset_nci20_rsp,rf_field_off_test_data[len].exp_rsp.len);
            rf_field_off_test_data[len].exp_ntf.len = sizeof(nfcc_core_reset_nci20_ntf);
            memcpy(rf_field_off_test_data[len].exp_ntf.p_data, nfcc_core_reset_nci20_ntf, rf_field_off_test_data[len].exp_ntf.len);
        }
        len++;//len val 1
        rf_field_off_test_data[len].exp_rsp.p_data[2] = NFCC_EXP_RES_DATA_BYTE2;
        if(phNxpNciHal_getChipType() == pn557)
        {
            rf_field_off_test_data[len].cmd.len = NFCC_CMD_NCI20_LEN;
            memcpy(rf_field_off_test_data[len].cmd.p_data, nfcc_core_init_nci20_cmd, NFCC_CMD_NCI20_LEN);
        }
        else
        {
            rf_field_off_test_data[len].cmd.len = NFCC_CMD_NCI10_LEN;
            memcpy(rf_field_off_test_data[len].cmd.p_data, nfcc_core_init_nci10_cmd, NFCC_CMD_NCI10_LEN);
        }
        len++;//len val 2
        if(phNxpNciHal_getChipType() != pn547C2)
        {
            rf_field_off_test_data[len].cmd.len = sizeof(nfcc_act_cmd);
            memcpy(rf_field_off_test_data[len].cmd.p_data, nfcc_act_cmd, rf_field_off_test_data[len].cmd.len);
            rf_field_off_test_data[len].exp_rsp.len = sizeof(nfcc_act_exp_rsp);
            memcpy(rf_field_off_test_data[len].exp_rsp.p_data, nfcc_act_exp_rsp, rf_field_off_test_data[len].cmd.len);
            rf_field_off_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
            rf_field_off_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
            rf_field_off_test_data[len].rsp_validator = st_validator_testEquals;
            rf_field_off_test_data[len].ntf_validator = st_validator_null;
            len++; //len val 3

            rf_field_off_test_data[len].cmd.len = sizeof(nfcc_power_mgt_cmd);
            memcpy(rf_field_off_test_data[len].cmd.p_data, nfcc_power_mgt_cmd, rf_field_off_test_data[len].cmd.len);
            rf_field_off_test_data[len].exp_rsp.len = sizeof(nfcc_power_mgt_rsp);
            memcpy(rf_field_off_test_data[len].exp_rsp.p_data, nfcc_power_mgt_rsp, rf_field_off_test_data[len].cmd.len);
            rf_field_off_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
            rf_field_off_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
            rf_field_off_test_data[len].rsp_validator = st_validator_testEquals;
            rf_field_off_test_data[len].ntf_validator = st_validator_null;
            len++; //len val 4
        }
        if(phNxpNciHal_getChipType() != pn547C2)
        {
            rf_field_off_test_data[len].cmd.len = sizeof(nfcc_antena_cmd3);
            memcpy(rf_field_off_test_data[len].cmd.p_data, nfcc_antena_cmd3, rf_field_off_test_data[len].cmd.len);
        }
        else
        {
            rf_field_off_test_data[len].cmd.len = sizeof(nfcc_antena_cmd4);
            memcpy(rf_field_off_test_data[len].cmd.p_data, nfcc_antena_cmd4, rf_field_off_test_data[len].cmd.len);
        }
        rf_field_off_test_data[len].exp_rsp.len = sizeof(nfcc_antena_rsp1);
        memcpy(rf_field_off_test_data[len].exp_rsp.p_data, nfcc_antena_rsp1, rf_field_off_test_data[len].cmd.len);
        rf_field_off_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
        rf_field_off_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
        rf_field_off_test_data[len].rsp_validator = st_validator_testEquals;
        rf_field_off_test_data[len].ntf_validator = st_validator_null;
        len++; //len val 5
        if(phNxpNciHal_getChipType() != pn547C2)
        {
            rf_field_off_test_data[len].cmd.len = sizeof(nfcc_power_mgt_cmd);
            memcpy(rf_field_off_test_data[len].cmd.p_data, nfcc_power_mgt_cmd, rf_field_off_test_data[len].cmd.len);
            rf_field_off_test_data[len].exp_rsp.len = sizeof(nfcc_power_mgt_rsp);
            memcpy(rf_field_off_test_data[len].exp_rsp.p_data, nfcc_power_mgt_rsp, rf_field_off_test_data[len].cmd.len);
            rf_field_off_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
            rf_field_off_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
            rf_field_off_test_data[len].rsp_validator = st_validator_testEquals;
            rf_field_off_test_data[len].ntf_validator = st_validator_null;
            len++; //len val 6
        }

        for (cnt = 0; cnt < len; cnt++)
        {
            status = phNxpNciHal_performTest(&(rf_field_off_test_data[cnt]));
            if (status == NFCSTATUS_RESPONSE_TIMEOUT || status == NFCSTATUS_FAILED)
            {
                break;
            }
        }
    }
    else
    {
        status = NFCSTATUS_FAILED;
    }

    if (status == NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_RfFieldTest - SUCCESSS\n");
    }
    else
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_RfFieldTest - FAILED\n");
    }

    NXPLOG_NCIHAL_D("phNxpNciHal_RfFieldTest - end\n");

    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_AntennaTest
 **
 ** Description
 **
 ** Returns
 **
 ******************************************************************************/
NFCSTATUS phNxpNciHal_AntennaTest()
{
    NFCSTATUS status = NFCSTATUS_FAILED;

    return status;
}

/*******************************************************************************
 **
 ** Function         phNxpNciHal_DownloadPinTest
 **
 ** Description      Test function to validate the FW download pin connection.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED.
 **
 *******************************************************************************/
NFCSTATUS phNxpNciHal_DownloadPinTest(void)
{
    NFCSTATUS status = NFCSTATUS_FAILED;
    int len = 0;
    int cnt = 0;

    NXPLOG_NCIHAL_D("phNxpNciHal_DownloadPinTest - start\n");

    download_pin_test_data1[0].exp_rsp.p_data[4] = NXP_NFCC_RESET_RSP_LEN;
    if(phNxpNciHal_getChipType() == pn557)
    {
        download_pin_test_data1[0].exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
        memcpy(download_pin_test_data1[0].exp_rsp.p_data, nfcc_core_reset_nci20_rsp,download_pin_test_data1[0].exp_rsp.len);
        download_pin_test_data1[0].exp_ntf.len = sizeof(nfcc_core_reset_nci20_ntf);
        memcpy(download_pin_test_data1[0].exp_ntf.p_data, nfcc_core_reset_nci20_ntf, download_pin_test_data1[0].exp_ntf.len);
    }
    len = (sizeof(download_pin_test_data1)/sizeof(download_pin_test_data1[0]));

    for (cnt = 0; cnt < len; cnt++)
    {
        status = phNxpNciHal_performTest(&(download_pin_test_data1[cnt]));
        if (status == NFCSTATUS_RESPONSE_TIMEOUT || status == NFCSTATUS_FAILED)
        {
            break;
        }
    }

    if (status != NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_DownloadPinTest - FAILED\n");
        return status;
    }

    status = NFCSTATUS_FAILED;
    status = phTmlNfc_IoCtl(phTmlNfc_e_EnableDownloadMode);
    if (NFCSTATUS_SUCCESS != status)
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_DownloadPinTest - FAILED\n");
        return status;
    }

    status = NFCSTATUS_FAILED;
    len = (sizeof(download_pin_test_data2)/sizeof(download_pin_test_data2[0]));

    for (cnt = 0; cnt < len; cnt++)
    {
        status = phNxpNciHal_performTest(&(download_pin_test_data2[cnt]));
        if (status == NFCSTATUS_RESPONSE_TIMEOUT || status == NFCSTATUS_FAILED)
        {
            break;
        }
    }

    if (status == NFCSTATUS_SUCCESS)
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_DownloadPinTest - SUCCESSS\n");
    }
    else
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_DownloadPinTest - FAILED\n");
    }

    NXPLOG_NCIHAL_D("phNxpNciHal_DownloadPinTest - end\n");

    return status;
}
/*******************************************************************************
 **
 ** Function         phNxpNciHal_AntennaSelfTest
 **
 ** Description      Test function to validate the Antenna's discrete
 **                  components connection.
 **
 ** Returns          NFCSTATUS_SUCCESS if successful,otherwise NFCSTATUS_FAILED.
 **
 *******************************************************************************/
NFCSTATUS phNxpNciHal_AntennaSelfTest(phAntenna_St_Resp_t* phAntenna_St_Resp)
{
    NFCSTATUS status = NFCSTATUS_FAILED;
    NFCSTATUS antenna_st_status = NFCSTATUS_FAILED;
    int len = 0;
    int cnt = 0;
    uint8_t nfcc_antena_cmd5[] = SYSTEM_TEST_ANTENNA_CMD_5;
    uint8_t nfcc_antena_rsp2[] = SYSTEM_TEST_ANTENNA_RSP_2;
    uint8_t nfcc_antena_cmd6[] = SYSTEM_TEST_ANTENNA_CMD_6; /* AGC with NFCLD measurement cmd */
    uint8_t nfcc_antena_cmd7[] = SYSTEM_TEST_ANTENNA_CMD_7;
    uint8_t nfcc_antena_cmd8[] = SYSTEM_TEST_ANTENNA_CMD_8;
    uint8_t nfcc_antena_cmd9[] = SYSTEM_TEST_ANTENNA_CMD_9;
    uint8_t nfcc_power_mgt_cmd_1[] = SYSTEM_SET_POWERMGT_CMD_1;

    NXPLOG_NCIHAL_D("phNxpNciHal_AntennaSelfTest - start\n");
    memcpy(&phAntenna_resp, phAntenna_St_Resp, sizeof(phAntenna_St_Resp_t));
    antenna_self_test_data[len].exp_rsp.p_data[4] = NXP_NFCC_RESET_RSP_LEN;
    if(phNxpNciHal_getChipType() == pn557)
    {
        antenna_self_test_data[len].exp_rsp.len = sizeof(nfcc_core_reset_nci20_rsp);
        memcpy(antenna_self_test_data[len].exp_rsp.p_data, nfcc_core_reset_nci20_rsp,antenna_self_test_data[len].exp_rsp.len);
        antenna_self_test_data[len].exp_ntf.len = sizeof(nfcc_core_reset_nci20_ntf);
        memcpy(antenna_self_test_data[len].exp_ntf.p_data, nfcc_core_reset_nci20_ntf, antenna_self_test_data[len].exp_ntf.len);
    }
    len++;//len val 1
    antenna_self_test_data[len].exp_rsp.p_data[2] = NFCC_EXP_RES_DATA_BYTE2;
    if(phNxpNciHal_getChipType() == pn557)
    {
        antenna_self_test_data[len].cmd.len = NFCC_CMD_NCI20_LEN;
        memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_core_init_nci20_cmd, NFCC_CMD_NCI20_LEN);
    }
    else
    {
        antenna_self_test_data[len].cmd.len = NFCC_CMD_NCI10_LEN;
        memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_core_init_nci10_cmd, NFCC_CMD_NCI10_LEN);
    }
    len+=2;//len val 3, By default, structure will be filled till array 3

    if(phNxpNciHal_getChipType() != pn547C2)
    {
        antenna_self_test_data[len].cmd.len = sizeof(nfcc_power_mgt_cmd);
        memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_power_mgt_cmd, antenna_self_test_data[len].cmd.len);
        antenna_self_test_data[len].exp_rsp.len = sizeof(nfcc_power_mgt_rsp);
        memcpy(antenna_self_test_data[len].exp_rsp.p_data, nfcc_power_mgt_rsp, antenna_self_test_data[len].cmd.len);
        antenna_self_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
        antenna_self_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
        antenna_self_test_data[len].rsp_validator = st_validator_testEquals;
        antenna_self_test_data[len].ntf_validator = st_validator_null;
        len++; //len val 4
    }
    antenna_self_test_data[len].cmd.len = sizeof(nfcc_antena_cmd5);
    memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_antena_cmd5, antenna_self_test_data[len].cmd.len);
    antenna_self_test_data[len].exp_rsp.len = sizeof(nfcc_antena_rsp2);
    memcpy(antenna_self_test_data[len].exp_rsp.p_data, nfcc_antena_rsp2, antenna_self_test_data[len].cmd.len);
    antenna_self_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
    antenna_self_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
    antenna_self_test_data[len].rsp_validator = st_validator_testAntenna_Txldo;
    antenna_self_test_data[len].ntf_validator = st_validator_null;
    len++; //len val 5
    if(phNxpNciHal_getChipType() != pn547C2)
    {
        antenna_self_test_data[len].cmd.len = sizeof(nfcc_antena_cmd7);
        memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_antena_cmd7, antenna_self_test_data[len].cmd.len);
    }
    else
    {
        antenna_self_test_data[len].cmd.len = sizeof(nfcc_antena_cmd8);
        memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_antena_cmd8, antenna_self_test_data[len].cmd.len);
    }
    antenna_self_test_data[len].exp_rsp.len = sizeof(nfcc_antena_rsp2);
    memcpy(antenna_self_test_data[len].exp_rsp.p_data, nfcc_antena_rsp2, antenna_self_test_data[len].cmd.len);
    antenna_self_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
    antenna_self_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
    antenna_self_test_data[len].rsp_validator = st_validator_testAntenna_AgcVal;
    antenna_self_test_data[len].ntf_validator = st_validator_null;
    len++;//len val 6
    antenna_self_test_data[len].cmd.len = sizeof(nfcc_antena_cmd9);
    memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_antena_cmd9, antenna_self_test_data[len].cmd.len);
    antenna_self_test_data[len].exp_rsp.len = sizeof(nfcc_antena_rsp2);
    memcpy(antenna_self_test_data[len].exp_rsp.p_data, nfcc_antena_rsp2, antenna_self_test_data[len].cmd.len);
    antenna_self_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
    antenna_self_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
    antenna_self_test_data[len].rsp_validator = st_validator_testAntenna_AgcVal_FixedNfcLd;
    antenna_self_test_data[len].ntf_validator = st_validator_null;
    len++;//len val 7

    if(NXP_NFCC_HW_ANTENNA_LOOP4_SELF_TEST)
    {
        antenna_self_test_data[len].cmd.len = sizeof(nfcc_antena_cmd6);
        memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_antena_cmd6, antenna_self_test_data[len].cmd.len);
        antenna_self_test_data[len].exp_rsp.len = sizeof(nfcc_antena_rsp2);
        memcpy(antenna_self_test_data[len].exp_rsp.p_data, nfcc_antena_rsp2, antenna_self_test_data[len].cmd.len);
        antenna_self_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
        antenna_self_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
        antenna_self_test_data[len].rsp_validator = st_validator_testAntenna_AgcVal_FixedNfcLd;
        antenna_self_test_data[len].ntf_validator = st_validator_null;
        len++;//len val 8
    }

    antenna_self_test_data[len].cmd.len = sizeof(nfcc_power_mgt_cmd_1);
    memcpy(antenna_self_test_data[len].cmd.p_data, nfcc_power_mgt_cmd_1, antenna_self_test_data[len].cmd.len);
    antenna_self_test_data[len].exp_rsp.len = sizeof(nfcc_power_mgt_rsp);
    memcpy(antenna_self_test_data[len].exp_rsp.p_data, nfcc_power_mgt_rsp, antenna_self_test_data[len].cmd.len);
    antenna_self_test_data[len].exp_ntf.len = NFCC_EXP_NTF_LEN_NULL;
    antenna_self_test_data[len].exp_ntf.p_data[0] = NFCC_EXP_NTF_DATA_NULL;
    antenna_self_test_data[len].rsp_validator = st_validator_testEquals;
    antenna_self_test_data[len].ntf_validator = st_validator_null;
    len++;//len val 9

    for (cnt = 0; cnt < len; cnt++)
    {
        status = phNxpNciHal_performTest(&(antenna_self_test_data[cnt]));
        if (status == NFCSTATUS_RESPONSE_TIMEOUT || status == NFCSTATUS_FAILED)
        {
            NXPLOG_NCIHAL_E(
                "phNxpNciHal_AntennaSelfTest: commnad execution - FAILED\n");
            break;
        }
    }

    if (status == NFCSTATUS_SUCCESS)
    {
        if ((gtxldo_status == NFCSTATUS_SUCCESS) &&
                (gagc_value_status == NFCSTATUS_SUCCESS) &&
                (gagc_nfcld_status == NFCSTATUS_SUCCESS))
        {
            if((NXP_NFCC_HW_ANTENNA_LOOP4_SELF_TEST) && (gagc_differential_status == NFCSTATUS_SUCCESS))
            {
                antenna_st_status = NFCSTATUS_SUCCESS;
                NXPLOG_NCIHAL_D("phNxpNciHal_AntennaSelfTest - SUCESS\n");
            }
            else if (!NXP_NFCC_HW_ANTENNA_LOOP4_SELF_TEST)
            {
                antenna_st_status = NFCSTATUS_SUCCESS;
                NXPLOG_NCIHAL_D("phNxpNciHal_AntennaSelfTest - SUCESS\n");
            }
        }
        if (antenna_st_status != NFCSTATUS_SUCCESS)
        {
            NXPLOG_NCIHAL_D("phNxpNciHal_AntennaSelfTest - FAILED\n");
            NXPLOG_NCIHAL_D("gtxldo_status=%d gagc_value_status=%d gagc_nfcld_status=%d HW_ANTENNA_LOOP4_SELF_TEST=%d"
                            "gagc_differential_status=%d\n", gtxldo_status,gagc_value_status,
                            gagc_nfcld_status, NXP_NFCC_HW_ANTENNA_LOOP4_SELF_TEST, gagc_differential_status);
        }
    }
    else
    {
        NXPLOG_NCIHAL_D("phNxpNciHal_AntennaSelfTest - FAILED, status = %d\n", status);
    }

    NXPLOG_NCIHAL_D("phNxpNciHal_AntennaSelfTest - end\n");

    return antenna_st_status;
}

#endif /*#ifdef NXP_HW_SELF_TEST*/
