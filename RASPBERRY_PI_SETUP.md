# PN7160 Integration Guide for Raspberry Pi Compute Module 4 (CM4)

This guide helps you integrate the NXP PN7160 NFC controller (OM27160A1EVK) with the Raspberry Pi Compute Module 4.

## Hardware Setup

### Required Hardware
- Raspberry Pi Compute Module 4 with I/O board
- NXP OM27160A1EVK development kit
- Jumper wires for connections
- 3.3V or 5V power supply (depending on PN7160 board configuration)

### Wiring Connections

Connect the OM27160A1EVK to the Raspberry Pi CM4 as follows:

| OM27160A1EVK Pin | Raspberry Pi CM4 Pin | GPIO | Function |
|------------------|---------------------|------|----------|
| VCC (3.3V/5V)    | 3.3V or 5V Power   | -    | Power    |
| GND              | Ground             | -    | Ground   |
| SDA              | Pin 3              | GPIO 2 | I2C1 Data |
| SCL              | Pin 5              | GPIO 3 | I2C1 Clock |
| IRQ              | Pin 16             | GPIO 23 | Interrupt |
| VEN (Enable)     | Pin 18             | GPIO 24 | Enable/Reset |
| DWL_REQ (optional) | Pin 22           | GPIO 25 | FW Download Request |

### Hardware Notes
- Ensure proper power supply (3.3V recommended for Pi compatibility)
- Use short, good quality jumper wires
- Double-check all connections before powering on
- The PN7160 I2C address is 0x28 (default)

## Software Setup

### 1. Enable I2C on Raspberry Pi

```bash
# Enable I2C interface
sudo raspi-config
# Navigate to: Interfacing Options -> I2C -> Enable

# Or enable via command line:
sudo modprobe i2c-dev
sudo modprobe i2c-bcm2708

# Verify I2C is working
i2cdetect -y 1
```

### 2. Install Dependencies

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install build dependencies
sudo apt install -y build-essential autotools-dev libtool pkg-config

# Install I2C tools for testing
sudo apt install -y i2c-tools

# Ensure user is in i2c group (should already be done on Raspberry Pi OS)
sudo usermod -a -G i2c $USER
```

### 3. Build and Install the NFC Library

The library is already built and installed in your system. The following files have been installed:

```bash
# Libraries
/usr/local/lib/libnfc_nci_linux.so
/usr/local/lib/libpn7160_fw.so

# Headers
/usr/local/include/linux_nfc_api.h
/usr/local/include/linux_nfc_factory_api.h
/usr/local/include/linux_nfc_api_compatibility.h

# Configuration files
/usr/local/etc/libnfc-nci.conf
/usr/local/etc/libnfc-nxp.conf
/usr/local/etc/libnfc-nxp-rpi.conf  # Raspberry Pi specific config

# Demo application
/usr/local/sbin/nfcDemoApp

# Package config
/usr/local/lib/pkgconfig/libnfc-nci.pc
```

### 4. Update Library Path

```bash
# Add library path to ldconfig
echo '/usr/local/lib' | sudo tee /etc/ld.so.conf.d/libnfc-nci.conf
sudo ldconfig
```

### 5. Configure for Raspberry Pi

The Raspberry Pi specific configuration uses the ALT_I2C transport which provides direct I2C access without requiring a kernel driver:

```bash
# Use the Raspberry Pi specific configuration
sudo cp /usr/local/etc/libnfc-nxp-rpi.conf /usr/local/etc/libnfc-nxp.conf
```

### 6. GPIO Permissions

Ensure proper GPIO permissions:

```bash
# Add user to gpio group (should already be done on Raspberry Pi OS)
sudo usermod -a -G gpio $USER

# You may need to log out and back in for group changes to take effect
```

## Boot Persistence and GPIO Setup

**CRITICAL**: GPIO settings do not persist after reboot. You must set up GPIO pins every time the system boots.

### Automated Setup (Recommended)

Run the GPIO setup script and choose to create a systemd service:
```bash
sudo ./pn7160_setup_gpio.sh
# When prompted, choose 'y' to create boot service
```

### Manual Setup After Each Reboot

If you didn't create the automatic service, run this after every reboot:
```bash
sudo ./pn7160_boot_setup.sh
```

### Verification

Always run a verification script before using NFC:
```bash
# Quick hardware test
sudo ./pn7160_hardware_test.sh

# Full troubleshooting if needed
sudo ./pn7160_troubleshoot.sh
```

## Testing the Integration

### 1. Check I2C Communication

```bash
# Scan I2C bus for devices
i2cdetect -y 1

# You should see device 0x28 if the PN7160 is properly connected and powered
```

### 2. Test GPIO Access

**IMPORTANT: Use the provided setup scripts instead of manual GPIO setup:**

```bash
# Use the automated GPIO setup script (recommended)
sudo ./pn7160_setup_gpio.sh

# Or use the boot setup script
sudo ./pn7160_boot_setup.sh
```

For manual testing only (not recommended for regular use):
```bash
# Test if GPIO pins can be accessed (run as root)
echo 535 | sudo tee /sys/class/gpio/export  # System GPIO for logical GPIO23
echo 536 | sudo tee /sys/class/gpio/export  # System GPIO for logical GPIO24
echo 537 | sudo tee /sys/class/gpio/export  # System GPIO for logical GPIO25

# Set directions
echo in | sudo tee /sys/class/gpio/gpio535/direction   # Interrupt
echo out | sudo tee /sys/class/gpio/gpio536/direction  # Enable
echo out | sudo tee /sys/class/gpio/gpio537/direction  # FW Download

# Test enable pin (should reset the PN7160)
echo 0 | sudo tee /sys/class/gpio/gpio536/value
sleep 0.1
echo 1 | sudo tee /sys/class/gpio/gpio536/value

# Clean up
echo 535 | sudo tee /sys/class/gpio/unexport
echo 536 | sudo tee /sys/class/gpio/unexport
echo 537 | sudo tee /sys/class/gpio/unexport
```

### 3. Run the Demo Application

```bash
# Run the NFC demo application (requires root for GPIO access)
sudo /usr/local/sbin/nfcDemoApp

# Or run from build directory
cd /home/alex/linux_libnfc-nci
sudo ./nfcDemoApp
```

## Configuration Details

### Transport Configuration

The Raspberry Pi setup uses the ALT_I2C transport (`NXP_TRANSPORT=0x02`) which:
- Accesses I2C directly via `/dev/i2c-1`
- Controls GPIO pins via sysfs (`/sys/class/gpio/`)
- Does not require kernel driver installation
- Uses I2C address 0x28

### GPIO Pin Configuration

**IMPORTANT**: The compiled library uses system GPIO numbers for sysfs access:

- **GPIO 23 (PIN_INT)**: Interrupt from PN7160 to Pi (system GPIO 535)
- **GPIO 24 (PIN_ENABLE)**: Enable/Reset signal from Pi to PN7160 (system GPIO 536)  
- **GPIO 25 (PIN_FWDNLD)**: Firmware download request (system GPIO 537)

**Note**: Modern Raspberry Pi systems map logical GPIO numbers to different system GPIO numbers in sysfs. The library automatically handles this mapping.

### I2C Configuration

- **Bus**: /dev/i2c-1 (I2C1 on Raspberry Pi)
- **Address**: 0x28 (PN7160 default)
- **Speed**: Standard mode (100kHz) or Fast mode (400kHz)

## Troubleshooting

### Common Issues

1. **Permission denied accessing /dev/i2c-1**
   - Ensure user is in i2c group: `sudo usermod -a -G i2c $USER`
   - Log out and back in for group changes to take effect

2. **Permission denied accessing GPIO**
   - Run with sudo initially: `sudo /usr/local/sbin/nfcDemoApp`
   - Ensure user is in gpio group: `sudo usermod -a -G gpio $USER`

3. **I2C device not found**
   - Check wiring connections
   - Verify power supply to PN7160
   - Run `i2cdetect -y 1` to scan for devices
   - Should see device at address 0x28

4. **Library not found errors**
   - Run `sudo ldconfig` to update library cache
   - Check `/etc/ld.so.conf.d/libnfc-nci.conf` exists

5. **GPIO export fails**
   - Check if pins are already in use: `ls /sys/class/gpio/`
   - Try different GPIO pins if needed
   - Ensure GPIO pins are available on your Pi model

### Debug Options

Enable debugging by modifying `/usr/local/etc/libnfc-nxp.conf`:

```
# Set logging levels to debug (0x03)
NXPLOG_EXTNS_LOGLEVEL=0x03
NXPLOG_NCIHAL_LOGLEVEL=0x03
NXPLOG_NCIX_LOGLEVEL=0x03
NXPLOG_NCIR_LOGLEVEL=0x03
NXPLOG_FWDNLD_LOGLEVEL=0x03
NXPLOG_TML_LOGLEVEL=0x03
```

### Advanced Configuration

For custom GPIO pins, modify the source code in:
```
src/nfcandroid_nfc_hidlimpl/halimpl/tml/transport/NfccAltTransport.h
```

Change the pin definitions:
```cpp
#define PIN_INT 23      // Interrupt pin
#define PIN_ENABLE 24   // Enable pin  
#define PIN_FWDNLD 25   // Firmware download pin
```

Then rebuild and reinstall:
```bash
cd /home/alex/linux_libnfc-nci
make clean
make
sudo make install
```

## Next Steps

Once the basic integration is working:

1. **Develop Applications**: Use the `linux_nfc_api.h` header to develop custom NFC applications
2. **Performance Tuning**: Adjust RF parameters in configuration files
3. **Production Deployment**: Consider kernel driver approach for production systems
4. **Security**: Implement proper access controls for GPIO and I2C resources

## References

- [OM27160A1EVK Product Page](https://www.nxp.com/part/OM27160A1EVK)
- [AN13287: PN7160 Linux Integration Guide](https://www.nxp.com/docs/en/application-note/AN13287.pdf)
- [linux_libnfc-nci GitHub Repository](https://github.com/NXPNFCLinux/linux_libnfc-nci)
- [NXP NFC Controller Documentation](https://www.nxp.com/products/identification-and-security/nfc/nfc-reader-ics:NFC-READER)