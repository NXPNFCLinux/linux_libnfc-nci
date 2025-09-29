# PN7160 Integration with Raspberry Pi CM4 - Complete Guide

## Overview
This guide provides comprehensive instructions for integrating the NXP PN7160 NFC controller (OM27160A1EVK development kit) with a Raspberry Pi Compute Module 4.

## Hardware Setup

### Required Connections
Connect the OM27160A1EVK to your Raspberry Pi CM4 as follows:

| PN7160 Board Pin | Raspberry Pi CM4 Pin | GPIO | Function |
|------------------|---------------------|------|-----------|
| VCC              | Pin 1 or 17         | 3.3V/5V | Power Supply |
| GND              | Pin 6, 9, 14, 20    | GND  | Ground |
| SDA              | Pin 3               | GPIO2 | I2C Data |
| SCL              | Pin 5               | GPIO3 | I2C Clock |
| IRQ              | Pin 16              | GPIO23 | Interrupt |
| VEN/ENABLE       | Pin 18              | GPIO24 | Enable |
| DWL (Optional)   | Pin 22              | GPIO25 | Download Mode |

### Power Requirements
- Check your PN7160 board specifications for voltage requirements (3.3V or 5V)
- Ensure adequate current supply for the NFC chip

## Software Setup

### 1. Repository and Branch
```bash
git clone https://github.com/NXPNFCLinux/linux_libnfc-nci.git
cd linux_libnfc-nci
git checkout NCI2.0_PN7160
```

### 2. Install Dependencies
```bash
sudo apt update
sudo apt install build-essential libtool autoconf automake i2c-tools gpiod
```

### 3. Enable I2C
```bash
sudo raspi-config
# Navigate to Interfacing Options -> I2C -> Enable
sudo reboot
```

### 4. Build with Correct GPIO Configuration
The library has been pre-configured with the correct GPIO pins for Raspberry Pi CM4:
- PIN_INT = GPIO23 (system GPIO 535)
- PIN_ENABLE = GPIO24 (system GPIO 536)  
- PIN_FWDNLD = GPIO25 (system GPIO 537)

```bash
./bootstrap
./configure
make clean
make
sudo make install
```

### 5. Configuration Files

#### A. libnfc-nxp.conf
Configure for ALT_I2C transport (direct I2C access):

```bash
# Edit /home/alex/linux_libnfc-nci/conf/libnfc-nxp.conf

# Set transport type to ALT_I2C
NXP_TRANSPORT=0x02

# I2C Configuration  
I2C_BUS=1
I2C_ADDRESS=0x28

# GPIO Pin Configuration (compiled into library)
# Uses system GPIO numbers for sysfs access:
PIN_INT=535     # Logical GPIO23
PIN_ENABLE=536  # Logical GPIO24
PIN_FWDNLD=537  # Logical GPIO25
```

#### B. Install Configuration
```bash
sudo mkdir -p /usr/local/etc
sudo cp conf/libnfc-nxp.conf /usr/local/etc/
sudo cp conf/libnfc-nci.conf /usr/local/etc/
```

## Hardware Verification

### 1. Check I2C Bus
```bash
# List I2C buses
i2cdetect -l

# Scan I2C bus 1 for devices
i2cdetect -y 1
```

### 2. Test GPIO Control
```bash
# Enable PN7160 (set VEN high using logical GPIO number)
gpioset --mode=time --sec=5 gpiochip0 24=1

# Then scan I2C again
i2cdetect -y 1
```

## GPIO Setup and Persistence

### Initial GPIO Setup
After hardware connections are made, run the GPIO setup script:

```bash
sudo ./pn7160_setup_gpio.sh
```

This script will:
- Configure GPIO pins 23, 24, 25 for NFC use
- Set up proper pin directions and initial values
- Enable the PN7160 device
- Verify I2C communication
- Optionally create a systemd service for automatic setup on boot

### Boot Persistence (Important!)
GPIO settings are not persistent across reboots. You have two options:

#### Option 1: Automatic Setup (Recommended)
When running `pn7160_setup_gpio.sh`, choose 'y' to create a systemd service that automatically sets up GPIO on every boot.

#### Option 2: Manual Setup After Reboot  
If you didn't enable the automatic service, run this after each reboot:
```bash
sudo ./pn7160_boot_setup.sh
```

Or run the full setup script again:
```bash
sudo ./pn7160_setup_gpio.sh
```

### Verification Scripts
Use these scripts to test your setup:

```bash
# Hardware connection test
sudo ./pn7160_hardware_test.sh

# Complete troubleshooting
sudo ./pn7160_troubleshoot.sh

# Quick GPIO status check  
sudo ./pn7160_control.sh
```

## Testing

### 1. Basic Functionality Test
```bash
# Run NFC demo in polling mode
sudo ./nfcDemoApp poll
```

### 2. Debug Mode
```bash
# Run with debug logging
sudo NXPLOG_EXTNS_LOGLEVEL=0x03 NXPLOG_NCIHAL_LOGLEVEL=0x03 NXPLOG_TML_LOGLEVEL=0x03 ./nfcDemoApp poll
```

## Troubleshooting

### Common Issues

1. **"NfcService Init Failed"**
   - Check hardware connections
   - Run GPIO setup script: `sudo ./pn7160_setup_gpio.sh`  
   - Verify VEN pin is high (logical GPIO24)
   - Ensure I2C bus permissions (`sudo usermod -a -G i2c $USER`)

2. **Device Not Detected on I2C**
   - Verify power supply
   - Check I2C address (try 0x28, 0x29)
   - Run: `sudo ./pn7160_setup_gpio.sh`
   - Ensure GPIO24 (VEN) is set high
   - Check for I2C pull-up resistors

3. **Permission Denied Errors**
   - Run with `sudo` for GPIO/I2C access
   - Add user to i2c group: `sudo usermod -a -G i2c $USER`

4. **GPIO Export Failures**
   - Use modern `gpiod` tools instead of sysfs
   - Check if pins are already in use

### Hardware Verification Checklist

- [ ] All connections secure and correct
- [ ] Power supply adequate (3.3V or 5V as required)
- [ ] Ground connections established
- [ ] I2C enabled in Raspberry Pi configuration
- [ ] GPIO setup completed: `sudo ./pn7160_setup_gpio.sh`
- [ ] VEN pin properly controlled (logical GPIO24)
- [ ] No short circuits or loose connections

## Alternative Approaches

### 1. Kernel Driver Method
Instead of ALT_I2C, you can use a kernel driver:
- Follow AN13287 documentation for kernel driver compilation
- Creates `/dev/nxpnfc` device file
- Set `NXP_TRANSPORT=0x00` in configuration

### 2. Different I2C Buses
If the default I2C bus doesn't work, try:
- I2C bus 20: `/dev/i2c-20`
- I2C bus 21: `/dev/i2c-21`
- Update `I2C_BUS` setting in configuration

## Useful Commands

```bash
# Check system info
uname -a
cat /proc/device-tree/model

# I2C tools
i2cdetect -l                    # List I2C buses
i2cdetect -y 1                  # Scan bus 1
i2cget -y 1 0x28 0             # Read from address 0x28

# GPIO tools
gpiodetect                      # List GPIO chips
gpioinfo gpiochip0             # GPIO line information
gpioset gpiochip0 24=1         # Set GPIO24 high (VEN/Enable)
gpioget gpiochip0 24           # Read GPIO24 state
gpioget gpiochip0 23           # Read GPIO23 state (IRQ)

# Debug logging levels
export NXPLOG_EXTNS_LOGLEVEL=0x03
export NXPLOG_NCIHAL_LOGLEVEL=0x03
export NXPLOG_TML_LOGLEVEL=0x03
```

## Documentation References

- [OM27160A1EVK Product Page](https://www.nxp.com/part/OM27160A1EVK)
- [Quick Start Guide PDF](https://www.nxp.com/docs/en/quick-reference-guide/OM27160A1QSGFL.pdf)
- [AN13287 - PN7160 Linux Integration Guide](https://www.nxp.com/docs/en/application-note/AN13287.pdf)
- [linux_libnfc-nci Repository](https://github.com/NXPNFCLinux/linux_libnfc-nci)

## Support Scripts Created

1. `pn7160_control.sh` - GPIO control script
2. `pn7160_troubleshoot.sh` - Comprehensive troubleshooting
3. Custom compiled library with Raspberry Pi GPIO pins

## Next Steps

Once hardware communication is established:
1. Test basic NFC operations (polling, reading tags)
2. Implement your specific NFC application
3. Consider power management and optimization
4. Integrate with your larger system

For additional support, consult the NXP documentation and community forums.