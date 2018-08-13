/*
 * Copyright (C) 2010-2014 NXP Semiconductors
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

/*
 * TML Alternative I2C port implementation for linux
 */

/* Basic type definitions */
#include "poll.h"

/* Describe PN71xx connection
 *  0 = Custom configuration
 *  1 = OM557x on Raspberry Pi
 *  2 = OM557x on UdooNeo
 *  3 = OM557x on BeagleBone black
 *
 */
#define CONFIGURATION    1

#if (CONFIGURATION == 1)
/* OM557x on Raspberry Pi */
 #define I2C_BUS         "/dev/i2c-1"
 #define I2C_ADDRESS     0x28
 #define PIN_INT         23
 #define PIN_ENABLE      24
#elif (CONFIGURATION == 2)
/* OM557x on UdooNeo */
 #define I2C_BUS         "/dev/i2c-1"
 #define I2C_ADDRESS     0x28
 #define PIN_INT         105
 #define PIN_ENABLE      149
#elif (CONFIGURATION == 3)
/* OM557x on BeagleBone Black */
 #define I2C_BUS         "/dev/i2c-2"
 #define I2C_ADDRESS     0x28
 #define PIN_INT         61
 #define PIN_ENABLE      30
#else
/* Custom configuration */
 #define I2C_BUS         "/dev/i2c-1"
 #define I2C_ADDRESS     0x28
 #define PIN_INT         23
 #define PIN_ENABLE      24
#endif
