/*
 *  Released under "The GNU General Public License (GPL-2.0)"
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

#include "usb_device.h"
#include "dfu.h"
#include "dfu_file.h"

struct dfu_file firmware;

IOUSBDeviceInterface300** prepareDFU(unsigned short idVendor, unsigned short idProduct)
{
    IOUSBDeviceInterface300** device = getDevice(idVendor, idProduct);
    
    if (device == NULL)
    {
        fprintf(stderr, "[!] Failed to retrieve USB device [%04x:%04x].\n", idVendor, idProduct);
        return NULL;
    }
    
    printDeviceInfo(device);
    
    if ((*device)->USBDeviceOpen(device) != kIOReturnSuccess)
        return NULL;
    
    setConfiguration(device);
    
    IOUSBInterfaceInterface300** interface = getDFUInterface(device);
    
    if (interface != NULL)
    {
        IOUSBDFUDescriptor* descriptor = getDFUDescriptor(interface);
        
        if (descriptor != NULL)
        {
            if (descriptor->bmAttributes & USB_DFU_CAN_DOWNLOAD)
            {
                if ((*interface)->USBInterfaceOpen(interface) != kIOReturnSuccess)
                    return NULL;
                
                unsigned char intfIndex;
                if ((*interface)->GetInterfaceNumber(interface, &intfIndex) != kIOReturnSuccess)
                    return NULL;
                
                struct dfu_status status;
                
                if (dfu_get_status(interface, intfIndex, &status) != kIOReturnSuccess)
                    goto error;
                
                printf("[i] Device State %s, Status %s\n",
                       dfu_state_to_string(status.bState), dfu_status_to_string(status.bStatus));
                
                // Is device stuck in error mode?
                if (status.bState == STATE_DFU_ERROR)
                {
                    printf("[i] Device is in error mode\n.");
                    
                    if (dfu_clear_status(interface, intfIndex) != kIOReturnSuccess)
                        goto error;
                    
                    if (dfu_abort(interface, intfIndex) != kIOReturnSuccess)
                        goto error;
                    
                    if (dfu_get_status(interface, intfIndex, &status) != kIOReturnSuccess)
                        goto error;
                }
                
                // Is previous firmware transfer incomplete?
                if (status.bState == STATE_DFU_DOWNLOAD_IDLE || status.bState == STATE_DFU_UPLOAD_IDLE)
                {
                    if (dfu_abort(interface, intfIndex) != kIOReturnSuccess)
                        goto error;
                   
                    if (dfu_get_status(interface, intfIndex, &status) != kIOReturnSuccess)
                        goto error;
                }
                
                // Device is already in DFU mode
                if (status.bState == STATE_DFU_IDLE)
                {
                    printf("[i] Device is already in DFU mode.\n");
                    
                    if ((*interface)->USBInterfaceClose(interface) != kIOReturnSuccess)
                        return NULL;
                    
                    if ((*interface)->Release(interface) != kIOReturnSuccess)
                        return NULL;
                   
                    return device;
                }
                
                if (status.bState != STATE_APP_IDLE)
                {
                    fprintf(stderr, "[!] Device is not idle, unable to detach (state %s).\n", dfu_state_to_string(status.bState));
                    goto error;
                }
                
                printf("[i] Transitioning from STATE_APP_IDLE into STATE_DFU_IDLE.\n");
                
                if (dfu_detach(interface, intfIndex, descriptor->wDetachTimeout) != kIOReturnSuccess)
                    goto error;
                
                if (!(descriptor->bmAttributes & USB_DFU_WILL_DETACH))
                    if ((*device)->ResetDevice(device) != kIOReturnSuccess)
                        return NULL;
                
                if (dfu_get_status(interface, intfIndex, &status) != kIOReturnSuccess)
                    goto error;
                
                if (status.bState != STATE_DFU_IDLE)
                {
                    fprintf(stderr, "[!] Device is not in dfu mode (state %s).\n", dfu_state_to_string(status.bState));
                    goto error;
                }
                
                if ((*interface)->USBInterfaceClose(interface) != kIOReturnSuccess)
                    return NULL;
                
                if ((*interface)->Release(interface) != kIOReturnSuccess)
                    return NULL;
                
                if ((*device)->USBDeviceClose(device) != kIOReturnSuccess)
                    return NULL;

                
                if ((*device)->USBDeviceOpen(device) != kIOReturnSuccess)
                    return NULL;
                
                return device;
            }
            else
                fprintf(stderr, "[!] Device is not able to receive firmware through DFU.\n");
        }
        else
            fprintf(stderr, "[!] Failed to locate DFU descriptor for interface.\n");
        
    error:
        (*interface)->USBInterfaceClose(interface);
        (*interface)->Release(interface);
    }
    else
        fprintf(stderr, "[!] Failed to locate DFU interface.\n");

    (*device)->USBDeviceClose(device);
    (*device)->Release(device);
    
    return NULL;
}

bool uploadFirmware(IOUSBDeviceInterface300** device)
{
    IOUSBInterfaceInterface300** interface = getDFUInterface(device);
    
    if (interface != NULL)
    {
        IOUSBDFUDescriptor* descriptor = getDFUDescriptor(interface);
        
        if (descriptor != NULL)
        {
            if (descriptor->bmAttributes & USB_DFU_CAN_DOWNLOAD)
            {
                (*interface)->USBInterfaceOpen(interface);
                
                unsigned char intfIndex;
                (*interface)->GetInterfaceNumber(interface, &intfIndex);
                
                struct dfu_status status;
                int firmware_size = firmware.size.total - firmware.size.suffix;
                int remaining = firmware_size;
                unsigned short transaction = 1;
                int sent = 0;

                printf("[i] Initiating firmware upload (%d bytes, %d bytes transfer size).\n", remaining, descriptor->wTransferSize);
                
                while (remaining > 0)
                {
                    int size = descriptor->wTransferSize < remaining ? descriptor->wTransferSize : remaining;
                    
                    printf("[i] Downloading firmware: Chunk %d (%d bytes) - %d / %d bytes.\n", transaction, size, sent, firmware_size);
                    
                    if (dfu_download(interface,
                                     intfIndex,
                                     size,
                                     transaction,
                                     firmware.firmware + sent) != kIOReturnSuccess)
                        break;
                    
                    sent += size;
                    remaining -= size;
                                    
                    // Wrap transaction around if required
                    transaction++;
                    transaction %= USHRT_MAX;
                    
                    if (dfu_get_status(interface, intfIndex, &status) != kIOReturnSuccess)
                        break;
                    
                    if (status.bStatus != DFU_STATUS_OK)
                    {
                        fprintf(stderr, "[!] Firmware download aborting (state %s, status %s).\n",
                                dfu_state_to_string(status.bState),
                                dfu_status_to_string(status.bStatus));
                        
                        break;
                    }
                }
                
                dfu_get_status(interface, intfIndex, &status);
                printf("[i] Device State %s, Status %s, String %d\n", dfu_state_to_string(status.bState), dfu_status_to_string(status.bStatus), status.iString);
                                
                usleep(status.bwPollTimeout * 1000);
                
                // Signal firmware upload finished
                dfu_download(interface, intfIndex, 0, transaction, NULL);
                
                dfu_get_status(interface, intfIndex, &status);
                printf("[i] Device State %s, Status %s, String %d\n", dfu_state_to_string(status.bState), dfu_status_to_string(status.bStatus), status.iString);
                
                // Firmware upload done resetting device
                if (remaining > 0)
                    fprintf(stderr, "[!] Error while flashing: \"%s\", %d / %d bytes remaining.\n",
                            dfu_status_to_string(status.bStatus), remaining, firmware_size);
                else if (status.bStatus != DFU_STATUS_OK)
                    fprintf(stderr, "[!] Error while flashing, %s.\n", dfu_status_to_string(status.bStatus));
                else
                {
                    printf("[i] Firmware upload complete, resetting device.\n");
                    
                    (*device)->ResetDevice(device);
                    (*interface)->USBInterfaceClose(interface);
                    (*interface)->Release(interface);
                    
                    (*device)->USBDeviceClose(device);
                    
                    return true;
                }

                (*interface)->USBInterfaceClose(interface);
            }
            else
                fprintf(stderr, "[!] Device is not capable to download firmware.\n");
        }
        else
            fprintf(stderr, "[!] Failed to locate DFU descriptor for interface.\n");
        
        (*interface)->Release(interface);
    }
    else
        fprintf(stderr, "[!] Failed to locate DFU interface.\n");
    
    (*device)->USBDeviceClose(device);
    
    return false;
}

int main(int argc, const char * argv[])
{
    printf("dfu-util, utility to flash dfu firmware into USB devices on OS X.\n");
    printf("Based on original dfu-tool & dfu-programmer for Linux.\n\n");
    
    if (argc != 4)
    {
        printf("Usage: dfu-util <vendorId hex> <productId hex> <firmware.dfu>\n");
        return -1;
    }
    
    // Parse device vendor & product
    unsigned short idVendor = strtoul(argv[1], NULL, 16);
    unsigned short idProduct = strtoul(argv[2], NULL, 16);
    
    printf("[i] Initiating DFU for USB device [%04x:%04x].\n", idVendor, idProduct);
    
    firmware.name = argv[3];
    dfu_load_file(&firmware, NEEDS_SUFFIX);
    
    show_suffix_and_prefix(&firmware);
  
    IOUSBDeviceInterface300** device = NULL;
    
    if ((device = prepareDFU(idVendor, idProduct)) != NULL)
        uploadFirmware(device);
    else
        fprintf(stderr, "[!] Failed to enter DFU mode.\n");

    free(firmware.firmware);
    
    return 0;
}
