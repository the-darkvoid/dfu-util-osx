/*
 *  Low-level DFU communication routines, originally taken from
 *  $Id: dfu.c,v 1.3 2006/06/20 06:28:04 schmidtw Exp $
 *  (part of dfu-programmer).
 *
 *  Copyright 2005-2006 Weston Schmidt <weston_schmidt@alumni.purdue.edu>
 *  Copyright 2011-2014 Tormod Volden <debian.tormod@gmail.com>
 *
 *  Modified for use on OS X
 *
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

#include "dfu.h"

static int dfu_timeout = 5000;  /* 5 seconds - default */

IOReturn control_transfer(IOUSBInterfaceInterface300** interface,
                          UInt8 requestType,
                          UInt8 request,
                          UInt16 value,
                          UInt16 index,
                          void * data,
                          UInt16 length)
{
    IOUSBDevRequestTO usbRequest;
    usbRequest.bmRequestType = requestType;
    usbRequest.bRequest = request;
    usbRequest.wValue = value;
    usbRequest.wIndex = index;
    usbRequest.wLength = length;
    usbRequest.pData = data;
    usbRequest.completionTimeout = dfu_timeout;
    usbRequest.noDataTimeout = dfu_timeout;
    
    return (*interface)->ControlRequestTO(interface, 0, &usbRequest);
}

/*
 *  DFU_DETACH Request (DFU Spec 1.0, Section 5.1)
 *
 *  device    - USB device pointer
 *  interface - the interface to communicate with
 *  timeout   - the timeout in ms the USB device should wait for a pending
 *              USB reset before giving up and terminating the operation
 *
 *  returns IOReturn value
 */
IOReturn dfu_detach(IOUSBInterfaceInterface300** interface,
                    const unsigned char index,
                    const unsigned short timeout)
{
    IOReturn result = control_transfer(interface,
                                       /* bmRequestType */ USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface),
                                       /* bRequest      */ DFU_DETACH,
                                       /* wValu:ne        */ timeout,
                                       /* wIndex        */ index,
                                       /* Data          */ NULL,
                                       /* wLength       */ 0);
    
    if (result != kIOReturnSuccess)
        fprintf(stderr, "[!] Failed DFU_DETACH: 0x%08x.\n", result);
    
    return result;
}


/*
 *  DFU_DNLOAD Request (DFU Spec 1.0, Section 6.1.1)
 *
 *  device    - USB device pointer
 *  interface - the interface to communicate with
 *  length    - the total number of bytes to transfer to the USB
 *              device - must be less than wTransferSize
 *  data      - the data to transfer
 *
 *  returns IOReturn value
 */
IOReturn dfu_download(IOUSBInterfaceInterface300** interface,
                      const unsigned char index,
                      const unsigned short length,
                      const unsigned short transaction,
                      unsigned char* data)
{
    IOReturn result = control_transfer(interface,
                                       /* bmRequestType */ USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface),
                                       /* bRequest      */ DFU_DNLOAD,
                                       /* wValue        */ transaction,
                                       /* wIndex        */ index,
                                       /* Data          */ data,
                                       /* wLength       */ length);
    
    if (result != kIOReturnSuccess)
        fprintf(stderr, "[!] Failed DFU_DNLOAD: 0x%08x.\n", result);
    
    return result;
}


/*
 *  DFU_UPLOAD Request (DFU Spec 1.0, Section 6.2)
 *
 *  device    - USB device pointer
 *  interface - the interface to communicate with
 *  length    - the maximum number of bytes to receive from the USB
 *              device - must be less than wTransferSize
 *  data      - the buffer to put the received data in
 *
 *  returns IOReturn value
 */
IOReturn dfu_upload(IOUSBInterfaceInterface300** interface,
               const unsigned char index,
               const unsigned short length,
               const unsigned short transaction,
               unsigned char* data)
{
    IOReturn result= control_transfer(interface,
                                      /* bmRequestType */ USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface),
                                      /* bRequest      */ DFU_UPLOAD,
                                      /* wValue        */ transaction,
                                      /* wIndex        */ index,
                                      /* Data          */ data,
                                      /* wLength       */ length);
    
    if (result != kIOReturnSuccess)
        fprintf(stderr, "[!] Failed DFU_UPLOAD: 0x%08x.\n", result);
    
    return result;
}


/*
 *  DFU_GETSTATUS Request (DFU Spec 1.0, Section 6.1.2)
 *
 *  device    - USB device pointer
 *  interface - the interface to communicate with
 *  status    - the data structure to be populated with the results
 *
 *  returns IOReturn value
 */
IOReturn dfu_get_status(IOUSBInterfaceInterface300** interface, const unsigned char index, struct dfu_status *status)
{
    unsigned char buffer[6];
    
    /* Initialize the status data structure */
    status->bStatus       = DFU_STATUS_ERROR_UNKNOWN;
    status->bwPollTimeout = 0;
    status->bState        = STATE_DFU_ERROR;
    status->iString       = 0;
    
    IOReturn result = control_transfer(interface,
                                       /* bmRequestType */ USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface),
                                       /* bRequest      */ DFU_GETSTATUS,
                                       /* wValue        */ 0,
                                       /* wIndex        */ index,
                                       /* Data          */ buffer,
                                       /* wLength       */ sizeof(buffer));
    
    if (result == kIOReturnSuccess)
    {
        status->bStatus = buffer[0];
        status->bwPollTimeout = ((0xff & buffer[3]) << 16) | ((0xff & buffer[2]) << 8) | (0xff & buffer[1]);
        status->bState = buffer[4];
        status->iString = buffer[5];
    }
    else
        fprintf(stderr, "[!] Failed DFU_GETSTATUS: 0x%08x.\n", result);
    
    return result;
}


/*
 *  DFU_CLRSTATUS Request (DFU Spec 1.0, Section 6.1.3)
 *
 *  device    - the usb_dev_handle to communicate with
 *  interface - the interface to communicate with
 *
 *  returns IOReturn value
 */
IOReturn dfu_clear_status(IOUSBInterfaceInterface300** interface, const unsigned char index)
{
    IOReturn result = control_transfer(interface,
                            /* bmRequestType */ USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface),
                            /* bRequest      */ DFU_CLRSTATUS,
                            /* wValue        */ 0,
                            /* wIndex        */ index,
                            /* Data          */ NULL,
                            /* wLength       */ 0);

    if (result != kIOReturnSuccess)
        fprintf(stderr, "[!] Failed DFU_CLRSTATUS: 0x%08x.\n", result);
    
    return result;
}

/*
 *  DFU_GETSTATE Request (DFU Spec 1.0, Section 6.1.5)
 *
 *  device    - USB device pointer
 *  interface - the interface to communicate with
 *
 *  returns the state or < 0 on error
 */
IOReturn dfu_get_state(IOUSBInterfaceInterface300** interface, const unsigned char index)
{
    unsigned char buffer[1];
    
    IOReturn result = control_transfer(interface,
                                       /* bmRequestType */ USBmakebmRequestType(kUSBIn, kUSBClass, kUSBInterface),
                                       /* bRequest      */ DFU_GETSTATE,
                                       /* wValue        */ 0,
                                       /* wIndex        */ index,
                                       /* Data          */ buffer,
                                       /* wLength       */ 1);
    
    if (result == kIOReturnSuccess)
    {
        printf("[!] State: %s\n", dfu_state_to_string(buffer[0]));
        
        // Return the state
        return buffer[0];
    }
    else
        fprintf(stderr, "[!] Failed DFU_GETSTATE: 0x%08x.\n", result);
    
    return -1;
}


/*
 *  DFU_ABORT Request (DFU Spec 1.0, Section 6.1.4)
 *
 *  device    - USB device pointer
 *  interface - the interface to communicate with
 *
 *  returns 0 or < 0 on an error
 */
IOReturn dfu_abort(IOUSBInterfaceInterface300** interface, const unsigned char index)
{
    IOReturn result = control_transfer(interface,
                            /* bmRequestType */ USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface),
                            /* bRequest      */ DFU_ABORT,
                            /* wValue        */ 0,
                            /* wIndex        */ index,
                            /* Data          */ NULL,
                            /* wLength       */ 0);
    
    if (result != kIOReturnSuccess)
        fprintf(stderr, "[!] Failed DFU_ABORT: 0x%08x.\n", result);
    
    return result;
}

const char* dfu_state_to_string(int state)
{
    switch (state)
    {
        case STATE_APP_IDLE:
            return "appIDLE";
        case STATE_APP_DETACH:
            return "appDETACH";
        case STATE_DFU_IDLE:
            return "dfuIDLE";
        case STATE_DFU_DOWNLOAD_SYNC:
            return "dfuDNLOAD-SYNC";
        case STATE_DFU_DOWNLOAD_BUSY:
            return "dfuDNBUSY";
        case STATE_DFU_DOWNLOAD_IDLE:
            return  "dfuDNLOAD-IDLE";
        case STATE_DFU_MANIFEST_SYNC:
            return "dfuMANIFEST-SYNC";
        case STATE_DFU_MANIFEST:
            return "dfuMANIFEST";
        case STATE_DFU_MANIFEST_WAIT_RESET:
            return "dfuMANIFEST-WAIT-RESET";
        case STATE_DFU_UPLOAD_IDLE:
            return "dfuUPLOAD-IDLE";
        case STATE_DFU_ERROR:
            return "dfuERROR";
        default:
            return "Unknown";
    }
}

const char* dfu_status_to_string(int status)
{
    switch (status)
    {
        case DFU_STATUS_OK:
            return "No error condition is present";
        case DFU_STATUS_ERROR_TARGET:
            return "File is not targeted for use by this device";
        case DFU_STATUS_ERROR_FILE:
            return "File is for this device but fails some vendor-specific test";
        case DFU_STATUS_ERROR_WRITE:
            return "Device is unable to write memory";
        case DFU_STATUS_ERROR_ERASE:
            return "Memory erase function failed";
        case DFU_STATUS_ERROR_CHECK_ERASED:
            return "Memory erase check failed";
        case DFU_STATUS_ERROR_PROG:
            return "Program memory function failed";
        case DFU_STATUS_ERROR_VERIFY:
            return "Programmed memory failed verification";
        case DFU_STATUS_ERROR_ADDRESS:
            return "Cannot program memory due to received address that is out of range";
        case DFU_STATUS_ERROR_NOTDONE:
            return "Received DFU_DNLOAD with wLength = 0, but device does not think that it has all data yet";
        case DFU_STATUS_ERROR_FIRMWARE:
            return "Device's firmware is corrupt. It cannot return to run-time (non-DFU) operations";
        case DFU_STATUS_ERROR_VENDOR:
            return "iString indicates a vendor specific error";
        case DFU_STATUS_ERROR_USBR:
            return "Device detected unexpected USB reset signalling";
        case DFU_STATUS_ERROR_POR:
            return "Device detected unexpected power on reset";
        case DFU_STATUS_ERROR_UNKNOWN:
            return "Something went wrong, but the device does not know what it was";
        case DFU_STATUS_ERROR_STALLEDPKT:
            return "Device stalled an unexpected request";
        default:
            return "Unknown";
    }
}

