#!/bin/bash

# PN7160 GPIO Auto-Setup Script
# This script runs the GPIO setup without interactive prompts
# Suitable for running on boot or from systemd service

echo "PN7160 GPIO Auto-Setup (Boot Mode)"
echo "=================================="

# GPIO pin definitions from NfccAltTransport.h
# These are the system GPIO numbers used by the compiled library
PIN_INT=535     # Interrupt pin (logical GPIO 23)
PIN_ENABLE=536  # VEN/Enable pin (logical GPIO 24)
PIN_FWDNLD=537  # Download pin (logical GPIO 25)

echo "Setting up GPIO pins using sysfs interface..."

# Function to setup GPIO pin via sysfs
setup_gpio() {
    local pin=$1
    local direction=$2
    local value=$3
    
    echo "Setting up GPIO$pin..."
    
    # Check if already exported
    if [ ! -d "/sys/class/gpio/gpio$pin" ]; then
        echo "Exporting GPIO$pin..."
        echo $pin > /sys/class/gpio/export 2>/dev/null || true
        sleep 0.2
    fi
    
    # Set direction
    if [ -w "/sys/class/gpio/gpio$pin/direction" ]; then
        echo "Setting GPIO$pin direction to $direction"
        echo $direction > /sys/class/gpio/gpio$pin/direction 2>/dev/null || true
        sleep 0.1
    else
        echo "Warning: Cannot set direction for GPIO$pin"
    fi
    
    # Set value if specified and if it's an output
    if [ -n "$value" ] && [ "$direction" = "out" ]; then
        if [ -w "/sys/class/gpio/gpio$pin/value" ]; then
            echo "Setting GPIO$pin value to $value"
            echo $value > /sys/class/gpio/gpio$pin/value 2>/dev/null || true
        fi
    fi
    
    # For interrupt pin, set edge trigger
    if [ "$pin" = "535" ] && [ "$direction" = "in" ]; then
        if [ -w "/sys/class/gpio/gpio$pin/edge" ]; then
            echo "Setting GPIO$pin edge to rising"
            echo "rising" > /sys/class/gpio/gpio$pin/edge 2>/dev/null || true
        fi
    fi
}

# Clean up any existing GPIO exports first
for pin in $PIN_INT $PIN_ENABLE $PIN_FWDNLD; do
    if [ -d "/sys/class/gpio/gpio$pin" ]; then
        echo "Cleaning up existing GPIO$pin export"
        echo $pin > /sys/class/gpio/unexport 2>/dev/null || true
        sleep 0.1
    fi
done

# Setup interrupt pin (input with rising edge)
setup_gpio $PIN_INT in

# Setup enable pin (output, initially low)  
setup_gpio $PIN_ENABLE out 0

# Setup download pin (output, initially low)
setup_gpio $PIN_FWDNLD out 0

# Wait a moment
sleep 0.5

# Enable the PN7160 by setting VEN high
echo "Enabling PN7160 (VEN = 1)..."
echo 1 > /sys/class/gpio/gpio$PIN_ENABLE/value 2>/dev/null || true

# Wait for device to initialize
sleep 1

# Check GPIO status (silently)
echo ""
echo "GPIO Status:"
if [ -r "/sys/class/gpio/gpio$PIN_INT/value" ]; then
    echo "IRQ (GPIO$PIN_INT): $(cat /sys/class/gpio/gpio$PIN_INT/value 2>/dev/null || echo 'Unknown')"
fi

if [ -r "/sys/class/gpio/gpio$PIN_ENABLE/value" ]; then
    echo "VEN (GPIO$PIN_ENABLE): $(cat /sys/class/gpio/gpio$PIN_ENABLE/value 2>/dev/null || echo 'Unknown')"
fi

if [ -r "/sys/class/gpio/gpio$PIN_FWDNLD/value" ]; then
    echo "DWL (GPIO$PIN_FWDNLD): $(cat /sys/class/gpio/gpio$PIN_FWDNLD/value 2>/dev/null || echo 'Unknown')"
fi

echo ""
echo "✅ PN7160 GPIO auto-setup complete!"

# Check I2C if tools are available
if command -v i2cdetect > /dev/null; then
    echo ""
    echo "Scanning I2C bus for PN7160..."
    if i2cdetect -y 1 | grep -q "28"; then
        echo "✅ PN7160 detected at address 0x28"
    else
        echo "⚠️  PN7160 not detected (may need more time to initialize)"
    fi
fi