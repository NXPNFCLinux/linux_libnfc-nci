#!/bin/bash

echo "PN7160 GPIO Setup for NFC Demo"
echo "=============================="

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
        echo $pin | sudo tee /sys/class/gpio/export > /dev/null
        sleep 0.2
    fi
    
    # Set direction
    if [ -w "/sys/class/gpio/gpio$pin/direction" ]; then
        echo "Setting GPIO$pin direction to $direction"
        echo $direction | sudo tee /sys/class/gpio/gpio$pin/direction > /dev/null
        sleep 0.1
    else
        echo "Warning: Cannot set direction for GPIO$pin"
    fi
    
    # Set value if specified and if it's an output
    if [ -n "$value" ] && [ "$direction" = "out" ]; then
        if [ -w "/sys/class/gpio/gpio$pin/value" ]; then
            echo "Setting GPIO$pin value to $value"
            echo $value | sudo tee /sys/class/gpio/gpio$pin/value > /dev/null
        fi
    fi
    
    # For interrupt pin, set edge trigger
    if [ "$pin" = "535" ] && [ "$direction" = "in" ]; then
        if [ -w "/sys/class/gpio/gpio$pin/edge" ]; then
            echo "Setting GPIO$pin edge to rising"
            echo "rising" | sudo tee /sys/class/gpio/gpio$pin/edge > /dev/null
        fi
    fi
}

# Clean up any existing GPIO exports first
for pin in $PIN_INT $PIN_ENABLE $PIN_FWDNLD; do
    if [ -d "/sys/class/gpio/gpio$pin" ]; then
        echo "Cleaning up existing GPIO$pin export"
        echo $pin | sudo tee /sys/class/gpio/unexport > /dev/null 2>&1
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
echo 1 | sudo tee /sys/class/gpio/gpio$PIN_ENABLE/value > /dev/null

# Wait for device to initialize
sleep 1

# Check GPIO status
echo ""
echo "GPIO Status:"
if [ -r "/sys/class/gpio/gpio$PIN_INT/value" ]; then
    echo "IRQ (GPIO$PIN_INT): $(cat /sys/class/gpio/gpio$PIN_INT/value)"
else
    echo "IRQ (GPIO$PIN_INT): Cannot read"
fi

if [ -r "/sys/class/gpio/gpio$PIN_ENABLE/value" ]; then
    echo "VEN (GPIO$PIN_ENABLE): $(cat /sys/class/gpio/gpio$PIN_ENABLE/value)"
else  
    echo "VEN (GPIO$PIN_ENABLE): Cannot read"
fi

if [ -r "/sys/class/gpio/gpio$PIN_FWDNLD/value" ]; then
    echo "DWL (GPIO$PIN_FWDNLD): $(cat /sys/class/gpio/gpio$PIN_FWDNLD/value)"
else
    echo "DWL (GPIO$PIN_FWDNLD): Cannot read"
fi

echo ""
echo "Scanning I2C bus for PN7160..."
i2cdetect -y 1 | grep "28" > /dev/null && echo "✅ PN7160 detected at address 0x28" || echo "❌ PN7160 not detected"

echo ""
echo "GPIO setup complete!"

# Ask if user wants to create a startup service for GPIO setup
echo ""
read -p "Do you want to create a systemd service to setup GPIO on boot? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "Creating systemd service for GPIO setup..."
    
    # Create the service file
    sudo tee /etc/systemd/system/pn7160-gpio.service > /dev/null << EOF
[Unit]
Description=PN7160 NFC GPIO Setup
After=multi-user.target
DefaultDependencies=no

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/home/alex/linux_libnfc-nci/pn7160_setup_gpio.sh
User=root

[Install]
WantedBy=multi-user.target
EOF
    
    # Enable and start the service
    sudo systemctl daemon-reload
    sudo systemctl enable pn7160-gpio.service
    
    echo "✅ Systemd service created and enabled!"
    echo "GPIO will be automatically setup on every boot."
    echo "You can check status with: sudo systemctl status pn7160-gpio.service"
else
    echo "Skipped systemd service creation."
    echo "Note: You'll need to run this script after each reboot."
fi

echo ""
echo "You can now run: sudo ./nfcDemoApp poll"