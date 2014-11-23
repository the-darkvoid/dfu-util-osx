dfu-util-osx
============

USB DFU (Device Firmware Upgrade) utility for OS X

Based on original Linux dfu-programmer / dfu-util.

This uses the USB DFU specification (http://www.usb.org/developers/docs/devclass_docs/DFU_1.1.pdf), to upload firmware into a DFU device.

The same functionality is used in /System/Library/Extensions/IOBluetoothFamily.kext/Contents/PlugIns/IOBluetoothUSBDFU.kext/Contents/Resources/IOBluetoothUSBDFUTool.

However IOBluetoothUSBDFUTool does not allow loading custom firmware into existing devices.

Firmwares included with OS X (.dfu files) can be found in /System/Library/Extensions/IOBluetoothFamily.kext/Contents/PlugIns/IOBluetoothUSBDFU.kext/Contents/Resources/.
These can be freely used with this tool.

This tool has been tested with Broadcom USB bluetooth devices (built-in and external).

Note that this does not work on Broadcom PatchRAM USB devices. For other devices (such as the original Apple bluetooth devices) it is able to re-program them.

When used on a PatchRAM device by accident, simply restore the device functionality by shutting down the computer fully and restarting.

**Flashing firmware is dangerous and could render your device non-functional. Use this at your own risk!**
