/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void T4T_NDEF_EMU_SetRecord(unsigned char *, unsigned short, void (*)(unsigned char *, unsigned short));
void T4T_NDEF_EMU_Reset(void);
void T4T_NDEF_EMU_Next(unsigned char *, unsigned short, unsigned char *, unsigned short *);
static unsigned char app[] = {0,0xa4,4,0,7,0xd2,0x76,0,0,0x85,1,1};
static unsigned char ndef[] = {0,0xa4,0,0x0c,2,0xe1,4};
static unsigned char record[] = {0xd1,1,5,0x54,2,'e','n','H','i'};
static unsigned char response[257];
static unsigned short exchange(unsigned char *cmd, unsigned short len)
{
    unsigned short n = 65535;
    T4T_NDEF_EMU_Next(cmd, len, response, &n);
    assert(n >= 2 && n <= sizeof(response));
    return n;
}
static void select_ndef(void)
{
    T4T_NDEF_EMU_Reset();
    assert(exchange(app,sizeof(app)) == 2 && response[0] == 0x90);
    assert(exchange(ndef,sizeof(ndef)) == 2 && response[0] == 0x90);
}
int main(void)
{
    unsigned char read_cmd[] = {0,0xb0,0,0,2};
    unsigned int seed = 1;
    T4T_NDEF_EMU_SetRecord(record,sizeof(record),NULL);
    select_ndef();
    assert(exchange(read_cmd,5) == 4 && response[0] == 0 && response[1] == sizeof(record));
    read_cmd[4] = 1;
    assert(exchange(read_cmd,5) == 3 && response[0] == 0); /* formerly length-2 underflow */
    read_cmd[3] = 1;
    assert(exchange(read_cmd,5) == 3 && response[0] == sizeof(record));
    read_cmd[3] = 2; read_cmd[4] = sizeof(record);
    assert(exchange(read_cmd,5) == sizeof(record)+2 && !memcmp(response,record,sizeof(record)));
    read_cmd[2] = 0xff; read_cmd[3] = 0xff;
    assert(exchange(read_cmd,5) == 2 && response[0] == 0x6a);
    select_ndef(); read_cmd[2] = read_cmd[3] = read_cmd[4] = 0;
    assert(exchange(read_cmd,5) == 2 && response[0] == 0x6a);
    /* Exact-sized allocations expose reads beyond truncated APDUs to ASan. */
    for (unsigned int len = 0; len < sizeof(app); len++) {
        unsigned char *cmd = malloc(len ? len : 1);
        memcpy(cmd,app,len);
        assert(exchange(cmd,len) == 2 && response[0] == 0x6a);
        free(cmd);
    }
    for (unsigned int i = 0; i < 10000; i++) {
        unsigned char cmd[5] = {0,0xb0,0,0,0};
        select_ndef();
        for (unsigned int j = 2; j < 5; j++) {
            seed = seed * 1664525U + 1013904223U;
            cmd[j] = seed >> 24;
        }
        exchange(cmd,5);
    }
    {
        unsigned char largest[253] = {0};
        T4T_NDEF_EMU_SetRecord(largest,sizeof(largest),NULL);
        select_ndef();
        read_cmd[2] = read_cmd[3] = 0; read_cmd[4] = 255;
        assert(exchange(read_cmd,5) == 257 && response[1] == 253);
    }
    puts("PASS: Type-4 selection/read transcript, bounds, truncated APDUs and 10000 malformed reads");
}
