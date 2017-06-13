/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright (C) 2015 NXP Semiconductors
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
 *  This file contains the CIPHER API code
 *
 ******************************************************************************/


#include "llcp_int.h"

#if(NFC_NXP_LLCP_SECURED_P2P == TRUE)
#include "cipher.h"

void cipher_init()
{

    LLCP_TRACE_DEBUG0 ("cipher_init");
    memset (&cipher_suite, 0, sizeof (tCIPHER_SUITE));

    cipher_suite.tag_len = CCM_TAG_LEN;
    cipher_suite.ccmNonce_len = CCM_NONCE_LEN;
    cipher_suite.key_len = KEY_LEN;
    cipher_suite.rn_len = RN_LEN;
    ecdh_init();

}

void cipher_deinit()
{

    LLCP_TRACE_DEBUG0 ("cipher_deinit");
    ecdh_deinit();

}

void cipher_generate_keys()
{

    LLCP_TRACE_DEBUG0("cipher_generate_keys");

    ecdh_get_localkeys();
    ecdh_get_randomnonce(cipher_suite.randomNonce_local);

    BN_bn2bin(EC_KEY_get0_private_key(ecdh_key.privKey_local),cipher_suite.privKey);
    BN_bn2bin(ecdh_key.x_a_local,cipher_suite.pubKey_local_x_a);
    BN_bn2bin(ecdh_key.y_a_local,cipher_suite.pubKey_local_y_a);

}

void cipher_generate_final_key_kenc()
{

    LLCP_TRACE_DEBUG0("cipher_generate_final_key_kenc");

    ecdh_get_remotekeys();
    ecdh_compute_sharedkey(ecdh_key.pubKey_remote,ecdh_key.privKey_local);

    if (llcp_cb.lcb.is_initiator)
    {
        memcpy(cipher_suite.randomNonce,cipher_suite.randomNonce_local,cipher_suite.rn_len);
        memcpy(&cipher_suite.randomNonce[cipher_suite.rn_len],cipher_suite.randomNonce_remote,cipher_suite.rn_len);
    }
    else
    {
        memcpy(cipher_suite.randomNonce,cipher_suite.randomNonce_remote,cipher_suite.rn_len);
        memcpy(&cipher_suite.randomNonce[cipher_suite.rn_len],cipher_suite.randomNonce_local,cipher_suite.rn_len);
    }

    kenc_compute_cipherkey();
}

void ecdh_init(){
    LLCP_TRACE_DEBUG0 ("ecdh_init");

    ecdh_key.privKey_local = EC_KEY_new_by_curve_name(ECDH_CURVE);
    ecdh_key.group_local = EC_KEY_get0_group(ecdh_key.privKey_local);
    ecdh_key.x_a_local = BN_new();BN_clear(ecdh_key.x_a_local);
    ecdh_key.y_a_local = BN_new();BN_clear(ecdh_key.y_a_local);

    ecdh_key.privKey_remote = EC_KEY_new_by_curve_name(ECDH_CURVE);
    ecdh_key.group_remote = EC_KEY_get0_group(ecdh_key.privKey_remote);
    ecdh_key.x_a_remote = BN_new();BN_clear(ecdh_key.x_a_remote);
    ecdh_key.y_a_remote = BN_new();BN_clear(ecdh_key.y_a_remote);

    ecdh_key.ctx = BN_CTX_new();
    ecdh_key.cctx = CMAC_CTX_new();
    ecdh_key.ccmctx = EVP_CIPHER_CTX_new();
}

void ecdh_deinit()
{
    LLCP_TRACE_DEBUG0 ("ecdh_deinit");

    if(ecdh_key.x_a_local)
        BN_free(ecdh_key.x_a_local);
    if(ecdh_key.y_a_local)
        BN_free(ecdh_key.y_a_local);
    if(ecdh_key.x_a_remote)
        BN_free(ecdh_key.x_a_remote);
    if(ecdh_key.y_a_remote)
        BN_free(ecdh_key.y_a_remote);

    if(ecdh_key.ctx)
        BN_CTX_free(ecdh_key.ctx);
    if(ecdh_key.cctx)
        CMAC_CTX_free(ecdh_key.cctx);
    if(ecdh_key.ccmctx)
        EVP_CIPHER_CTX_free(ecdh_key.ccmctx);
}

void ecdh_get_localkeys()
{
    LLCP_TRACE_DEBUG0("ecdh_get_localkeys");

    if (!EC_KEY_generate_key(ecdh_key.privKey_local))
    {
        return;
    }
    ecdh_key.pubKey_local = EC_KEY_get0_public_key(ecdh_key.privKey_local);

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(ecdh_key.group_local)) == NID_X9_62_prime_field)
    {
        if (!EC_POINT_get_affine_coordinates_GFp(ecdh_key.group_local,
            ecdh_key.pubKey_local, ecdh_key.x_a_local, ecdh_key.y_a_local, ecdh_key.ctx))
        {
            return;
        }
    }
}

void ecdh_get_randomnonce(UINT8 *randomNum)
{

    LLCP_TRACE_DEBUG0("ecdh_get_randomnonce");
    UINT8 buffer[8];
    UINT8 written = RAND_pseudo_bytes(buffer, sizeof(buffer));
    if(written == 1)
    {
        RAND_seed(buffer, written);
        memcpy(randomNum,buffer,8);
    }
    else
        return;
}

void ecdh_get_remotekeys()
{

    LLCP_TRACE_DEBUG0("ecdh_get_remotekeys");

    ecdh_key.x_a_remote = BN_bin2bn(cipher_suite.pubKey_remote_x_a,cipher_suite.key_len,NULL);
    ecdh_key.y_a_remote = BN_bin2bn(cipher_suite.pubKey_remote_y_a,cipher_suite.key_len,NULL);

    if (EC_METHOD_get_field_type(EC_GROUP_method_of(ecdh_key.group_remote)) == NID_X9_62_prime_field)
    {
        LLCP_TRACE_DEBUG0("EC_KEY_set_public_key_affine_coordinates");
        EC_KEY_set_public_key_affine_coordinates(ecdh_key.privKey_remote,ecdh_key.x_a_remote,ecdh_key.y_a_remote);
    }

    ecdh_key.pubKey_remote = EC_KEY_get0_public_key(ecdh_key.privKey_remote);
}

int ecdh_compute_sharedkey(EC_POINT *remotEcPnt,EC_KEY *loclecKey)
{

    LLCP_TRACE_DEBUG0("ecdh_compute_sharedkey");
    UINT16 alen = KDF1_SHA1_len;
    UINT8 abuff[alen];
    memset(abuff,0,alen);

    ECDH_compute_key(abuff,alen,remotEcPnt,loclecKey,NULL);
    cipher_suite.sharedSecretKey_len = alen;
    memcpy(cipher_suite.sharedSecretKey, abuff, alen);

    return TRUE;
}

void kenc_compute_cipherkey()
{

    LLCP_TRACE_DEBUG0("kenc_compute_cipherkey");
    UINT16 aesCmaclen=aes_cmac_len;
    UINT8 aesCmac[aesCmaclen];
    memset(aesCmac,0,aesCmaclen);

    CMAC_Init(ecdh_key.cctx, cipher_suite.randomNonce, aesCmaclen, EVP_aes_128_cbc(), NULL);
    CMAC_Update(ecdh_key.cctx, cipher_suite.sharedSecretKey, cipher_suite.sharedSecretKey_len);

    CMAC_Final(ecdh_key.cctx, aesCmac, &aesCmaclen);
    cipher_suite.finalKey_len = aesCmaclen;
    memcpy(cipher_suite.finalKey,aesCmac,aesCmaclen);

    return;
}

void aes_ccm_encrypt_data(UINT8*pInBuff,UINT16 BuffLen, UINT8 *aad,UINT16 aad_len, UINT16 *pOutBuff)
{
    UINT8 len=0;
    LLCP_TRACE_DEBUG0("AES CCM Encrypt data");

    /* Set cipher type and mode */
    EVP_EncryptInit_ex(ecdh_key.ccmctx, EVP_aes_128_ccm(), NULL, NULL, NULL);

    /*EVP_CTRL_AEAD_SET_IVLEN or EVP_CTRL_GCM_SET_IVLEN 13*/
    EVP_CIPHER_CTX_ctrl(ecdh_key.ccmctx, EVP_CTRL_CCM_SET_IVLEN, cipher_suite.ccmNonce_len, NULL);
    /* Set tag length */
    /*EVP_CTRL_AEAD_SET_TAG or EVP_CTRL_GCM_SET_TAG  4*/
    EVP_CIPHER_CTX_ctrl(ecdh_key.ccmctx, EVP_CTRL_CCM_SET_TAG, cipher_suite.tag_len, NULL);

    /* Initialize key and IV */
    EVP_EncryptInit_ex(ecdh_key.ccmctx, NULL, NULL, cipher_suite.finalKey, cipher_suite.ccmNonce);

    /* Encrypt plain text and aad: can only be called once */
    EVP_EncryptUpdate(ecdh_key.ccmctx, NULL,&len,NULL, BuffLen);
    EVP_EncryptUpdate(ecdh_key.ccmctx, NULL,&len,aad,aad_len);

    /* Encrypt plain text: can only be called once */
    EVP_EncryptUpdate(ecdh_key.ccmctx, pOutBuff,&len, pInBuff, BuffLen);

    /* Get tag */
    /*EVP_CTRL_AEAD_GET_TAG or EVP_CTRL_GCM_GET_TAG  */
    EVP_CIPHER_CTX_ctrl(ecdh_key.ccmctx, EVP_CTRL_CCM_GET_TAG, cipher_suite.tag_len, cipher_suite.tag);

    //send packet counter PC(S)
    cipher_suite.packet_counter_send++;
    *cipher_suite.ccmNonce = cipher_suite.packet_counter_send;

}

void aes_ccm_decrypt_data(UINT8*pInBuff,UINT16 BuffLen, UINT8 *aad, UINT16 aad_len,UINT16 *pOutBuff)
{
    UINT8 rv=0;
    UINT8 len=0;
    LLCP_TRACE_DEBUG0("AES CCM Decrypt data");

    /* Select cipher */
    EVP_DecryptInit_ex(ecdh_key.ccmctx, EVP_aes_128_ccm(), NULL, NULL, NULL);

    /*EVP_CTRL_AEAD_SET_IVLEN or EVP_CTRL_GCM_SET_IVLEN  13*/
    EVP_CIPHER_CTX_ctrl(ecdh_key.ccmctx, EVP_CTRL_CCM_SET_IVLEN, cipher_suite.ccmNonce_len, NULL);

    /* Set expected tag value */
    /*EVP_CTRL_AEAD_SET_TAG or EVP_CTRL_GCM_SET_TAG  4*/
    EVP_CIPHER_CTX_ctrl(ecdh_key.ccmctx, EVP_CTRL_CCM_SET_TAG,cipher_suite.tag_len,cipher_suite.tag);

    /* Specify key and IV */
    EVP_DecryptInit_ex(ecdh_key.ccmctx, NULL, NULL, cipher_suite.finalKey, cipher_suite.ccmNonce);

    EVP_DecryptUpdate(ecdh_key.ccmctx, NULL,&len, NULL, BuffLen);
    EVP_DecryptUpdate(ecdh_key.ccmctx, NULL,&len, aad,aad_len);

    /* Decrypt plain text, verify tag: can only be called once */
    rv = EVP_DecryptUpdate(ecdh_key.ccmctx, pOutBuff,&len, pInBuff, BuffLen);

    /* Output decrypted block: if tag verify failed we get nothing */
    if (rv > 0) {
        LLCP_TRACE_DEBUG0("Plain text available :");
    } else
        LLCP_TRACE_ERROR0("Plain text not available: tag verify failed.\n");

    //recv packet counter PC(R)
    cipher_suite.packet_counter_recv++;
    *cipher_suite.ccmNonce = cipher_suite.packet_counter_recv;

}
#endif //NFC_NXP_LLCP_SECURED_P2P End
