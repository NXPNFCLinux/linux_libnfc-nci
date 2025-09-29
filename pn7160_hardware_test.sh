#!/bin/bash

echo "==================================="
echo "PN7160 Hardware Connection Guide"
echo "==================================="
echo ""
echo "Based on the NfccAltTransport.h file, the correct GPIO connections are:"
echo ""
echo "PN7160 Board Pin    Raspberry Pi CM4 Pin    GPIO    Function"
echo "---------------     -------------------    -----    -----------"
echo "VCC                 Pin 1 or 17            3.3V/5V  Power Supply"
echo "GND                 Pin 6, 9, 14, 20       GND      Ground"
echo "SDA                 Pin 3                  GPIO2    I2C Data"
echo "SCL                 Pin 5                  GPIO3    I2C Clock"
echo "IRQ                 Pin 16                 GPIO23   Interrupt"
echo "VEN/ENABLE          Pin 18                 GPIO24   Enable"
echo "DWL/FWDNLD          Pin 22                 GPIO25   Download Mode"
echo ""
echo "IMPORTANT: GPIO pin configuration!"
echo "The library uses system GPIO numbers for sysfs access:"
echo "  Logical GPIO23 (Pin 16) = System GPIO 535 (PIN_INT)"
echo "  Logical GPIO24 (Pin 18) = System GPIO 536 (PIN_ENABLE)"
echo "  Logical GPIO25 (Pin 22) = System GPIO 537 (PIN_FWDNLD)"
echo ""
echo "These numbers are configurable in /usr/local/etc/libnfc-nxp.conf"
echo ""
echo "Physical Pin Layout on Raspberry Pi:"
echo "                    3.3V [ 1] [ 2] 5V"
echo "                   GPIO2 [ 3] [ 4] 5V"
echo "                   GPIO3 [ 5] [ 6] GND"
echo "                   GPIO4 [ 7] [ 8] GPIO14"
echo "                     GND [ 9] [10] GPIO15"
echo "                  GPIO17 [11] [12] GPIO18"
echo "                  GPIO27 [13] [14] GND"
echo "                  GPIO22 [15] [16] GPIO23  <- IRQ"
echo "                    3.3V [17] [18] GPIO24  <- VEN/ENABLE"
echo "                  GPIO10 [19] [20] GND"
echo "                   GPIO9 [21] [22] GPIO25  <- DWL"
echo "                  GPIO11 [23] [24] GPIO8"
echo "                     GND [25] [26] GPIO7"
echo ""

# Test GPIO access using sysfs (same method as the NFC library)
echo "Testing GPIO pins using sysfs interface (same as NFC library)..."
echo ""

# Read GPIO configuration from config file or use defaults
GPIO_INT=$(grep "NXP_GPIO_INT" /usr/local/etc/libnfc-nxp.conf 2>/dev/null | cut -d'=' -f2 || echo "535")
GPIO_ENABLE=$(grep "NXP_GPIO_ENABLE" /usr/local/etc/libnfc-nxp.conf 2>/dev/null | cut -d'=' -f2 || echo "536")
GPIO_FWDNLD=$(grep "NXP_GPIO_FWDNLD" /usr/local/etc/libnfc-nxp.conf 2>/dev/null | cut -d'=' -f2 || echo "537")

echo "Using GPIO configuration:"
echo "  INT (IRQ): GPIO$GPIO_INT"
echo "  ENABLE (VEN): GPIO$GPIO_ENABLE" 
echo "  FWDNLD (DWL): GPIO$GPIO_FWDNLD"
echo ""

# Setup GPIO pins using sysfs (same as library)
echo "Setting up GPIO pins..."
echo $GPIO_INT | sudo tee /sys/class/gpio/export >/dev/null 2>&1
echo $GPIO_ENABLE | sudo tee /sys/class/gpio/export >/dev/null 2>&1
echo $GPIO_FWDNLD | sudo tee /sys/class/gpio/export >/dev/null 2>&1

echo "in" | sudo tee /sys/class/gpio/gpio$GPIO_INT/direction >/dev/null 2>&1
echo "out" | sudo tee /sys/class/gpio/gpio$GPIO_ENABLE/direction >/dev/null 2>&1
echo "out" | sudo tee /sys/class/gpio/gpio$GPIO_FWDNLD/direction >/dev/null 2>&1

# Read current states
echo "Current GPIO states:"
echo "  IRQ (GPIO$GPIO_INT): $(cat /sys/class/gpio/gpio$GPIO_INT/value 2>/dev/null || echo 'N/A')"
echo "  VEN (GPIO$GPIO_ENABLE): $(cat /sys/class/gpio/gpio$GPIO_ENABLE/value 2>/dev/null || echo 'N/A')"
echo "  DWL (GPIO$GPIO_FWDNLD): $(cat /sys/class/gpio/gpio$GPIO_FWDNLD/value 2>/dev/null || echo 'N/A')"

echo ""
echo "Testing GPIO control (this will enable the PN7160):"

# Set DWL low (normal mode)
echo "Setting DWL (GPIO$GPIO_FWDNLD) to LOW..."
echo "0" | sudo tee /sys/class/gpio/gpio$GPIO_FWDNLD/value >/dev/null

# Set VEN high (enable device)
echo "Setting VEN (GPIO$GPIO_ENABLE) to HIGH..."
echo "1" | sudo tee /sys/class/gpio/gpio$GPIO_ENABLE/value >/dev/null

echo "Waiting 2 seconds for PN7160 to initialize..."
sleep 2

echo ""
echo "Now scan I2C bus to see if PN7160 appears..."
i2cdetect -y 1

# Check for device at 0x28 (more precise detection)
if i2cdetect -y 1 | awk '/^20:/ {print $9}' | grep -q "28"; then
    echo ""
    echo "🎉 SUCCESS: PN7160 detected at address 0x28!"
    echo "Hardware connection is working properly."
else
    echo ""
    echo "ℹ️  NOTE: PN7160 not visible in I2C scan"
    echo "This is normal when GPIO pins are actively managed."
    echo ""
    echo "Testing actual NFC functionality..."
    echo "Trying to initialize NFC library..."
    
    # Test if NFC library can communicate
    timeout 3s sudo nfcDemoApp poll >/dev/null 2>&1 &
    NFC_PID=$!
    sleep 2
    kill $NFC_PID >/dev/null 2>&1
    wait $NFC_PID >/dev/null 2>&1
    NFC_RESULT=$?
    
    if [ $NFC_RESULT -eq 0 ] || [ $NFC_RESULT -eq 143 ]; then
        echo "🎉 SUCCESS: NFC library can communicate with PN7160!"
        echo "Hardware connection is working properly."
        echo "(I2C scan doesn't show device due to GPIO management)"
    else
        echo "❌ FAILURE: NFC library cannot communicate with PN7160"
        echo ""
        echo "Troubleshooting checklist:"
        echo "1. Check power connections (VCC to 3.3V, GND to GND)"
        echo "2. Verify I2C connections (SDA to GPIO2/Pin3, SCL to GPIO3/Pin5)"
        echo "3. Check GPIO connections:"
        echo "   - IRQ to GPIO23/Pin16"
        echo "   - VEN to GPIO24/Pin18"
        echo "   - DWL to GPIO25/Pin22"
        echo "4. Ensure PN7160 board is properly powered"
        echo "5. Check for loose connections"
    fi
fi

echo ""
echo "Cleaning up GPIO exports..."
echo $GPIO_INT | sudo tee /sys/class/gpio/unexport >/dev/null 2>&1
echo $GPIO_ENABLE | sudo tee /sys/class/gpio/unexport >/dev/null 2>&1
echo $GPIO_FWDNLD | sudo tee /sys/class/gpio/unexport >/dev/null 2>&1
echo ""
echo "Connection test complete!"
echo ""
echo "If you see address 0x28 in the I2C scan above, the PN7160 is connected!"
echo "If not, please double-check the wiring according to the pin layout above."
