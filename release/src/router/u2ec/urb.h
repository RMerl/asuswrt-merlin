/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

struct _URB_HEADER {
    //
    // Fields filled in by client driver
    //
    USHORT Length;
    USHORT Function;
    USBD_STATUS Status;
    //
    // Fields used only by USBD
    //
    PVOID UsbdDeviceHandle; // device handle assigned to this device
                            // by USBD
    ULONG UsbdFlags;        // flags field reserved for USBD use.
};

struct _URB_SELECT_INTERFACE {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    USBD_CONFIGURATION_HANDLE ConfigurationHandle;

    // client must input AlternateSetting & Interface Number
    // class driver returns interface and handle
    // for new alternate setting
    USBD_INTERFACE_INFORMATION Interface;
};

struct _URB_SELECT_CONFIGURATION {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    // NULL indicates to set the device
    // to the 'unconfigured' state
    // ie set to configuration 0
    PUSB_CONFIGURATION_DESCRIPTOR ConfigurationDescriptor;
    USBD_CONFIGURATION_HANDLE ConfigurationHandle;
    USBD_INTERFACE_INFORMATION Interface;
};

//
// This structure used for ABORT_PIPE & RESET_PIPE
//

struct _URB_PIPE_REQUEST {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    USBD_PIPE_HANDLE PipeHandle;
    ULONG Reserved;
};

//
// This structure used for
// TAKE_FRAME_LENGTH_CONTROL &
//        RELEASE_FRAME_LENGTH_CONTROL
//

struct _URB_FRAME_LENGTH_CONTROL {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
};

struct _URB_GET_FRAME_LENGTH {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    ULONG FrameLength;
    ULONG FrameNumber;
};

struct _URB_SET_FRAME_LENGTH {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    LONG FrameLengthDelta;
};

struct _URB_GET_CURRENT_FRAME_NUMBER {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    ULONG FrameNumber;
};

//
// Structures for specific control transfers
// on the default pipe.
//

// GET_DESCRIPTOR
// SET_DESCRIPTOR

struct _URB_CONTROL_DESCRIPTOR_REQUEST {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif    
    PVOID Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;               // fields for HCD use
    USHORT Reserved1;
    UCHAR Index;
    UCHAR DescriptorType;
    USHORT LanguageId;
    USHORT Reserved2;
};

// GET_STATUS

struct _URB_CONTROL_GET_STATUS_REQUEST {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    PVOID Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use
    UCHAR Reserved1[4];
    USHORT Index;                       // zero, interface or endpoint
    USHORT Reserved2;
};

// SET_FEATURE
// CLEAR_FEATURE

struct _URB_CONTROL_FEATURE_REQUEST {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif
    PVOID Reserved;
    ULONG Reserved2;
    ULONG Reserved3;
    PVOID Reserved4;
    PMDL Reserved5;
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use
    USHORT Reserved0;
    USHORT FeatureSelector;
    USHORT Index;                       // zero, interface or endpoint
    USHORT Reserved1;
};

// VENDOR & CLASS

struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    PVOID Reserved;
    ULONG TransferFlags;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use
    UCHAR RequestTypeReservedBits;
    UCHAR Request;
    USHORT Value;
    USHORT Index;
    USHORT Reserved1;
};


struct _URB_CONTROL_GET_INTERFACE_REQUEST {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    PVOID Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use
    UCHAR Reserved1[4];    
    USHORT Interface;
    USHORT Reserved2;
};


struct _URB_CONTROL_GET_CONFIGURATION_REQUEST {
#ifdef OSR21_COMPAT
    struct _URB_HEADER Hdr;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    PVOID Reserved;
    ULONG Reserved0;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use
    UCHAR Reserved1[8];    
};


//
// request format for a control transfer on
// the non-default pipe.
//

struct _URB_CONTROL_TRANSFER {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    USBD_PIPE_HANDLE PipeHandle;
    ULONG TransferFlags;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use
    UCHAR SetupPacket[8];
};


struct _URB_BULK_OR_INTERRUPT_TRANSFER {
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    USBD_PIPE_HANDLE PipeHandle;
    ULONG TransferFlags;                // note: the direction bit will be set by USBD
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use
};


//
// ISO Transfer request
//
// TransferBufferMDL must point to a single virtually 
// contiguous buffer.
//
// StartFrame - the frame to send/receive the first packet of 
// the request. 
//
// NumberOfPackets - number of packets to send in this request
//
//
// IsoPacket Array
//
//      Input:  Offset - offset of the packet from the beginig
//                 of the client buffer.
//      Output: Length -  is set to the actual length of the packet
//                (For IN transfers). 
//      Status: error that occurred during transmission or 
//              reception of the packet.
//      

typedef struct _USBD_ISO_PACKET_DESCRIPTOR {
    ULONG Offset;       // INPUT Offset of the packet from the begining of the
                        // buffer.

    ULONG Length;       // OUTPUT length of data received (for in).
                        // OUTPUT 0 for OUT.
    USBD_STATUS Status; // status code for this packet.     
} USBD_ISO_PACKET_DESCRIPTOR, *PUSBD_ISO_PACKET_DESCRIPTOR;

struct _URB_ISOCH_TRANSFER {
    //
    // This block is the same as CommonTransfer
    //
#ifdef OSR21_COMPAT
    struct _URB_HEADER;     
#else
    struct _URB_HEADER Hdr;                 // function code indicates get or set.
#endif 
    USBD_PIPE_HANDLE PipeHandle;
    ULONG TransferFlags;
    ULONG TransferBufferLength;
    PVOID TransferBuffer;
    PMDL TransferBufferMDL;             // *optional*
    struct _URB *UrbLink;               // *optional* link to next urb request
                                        // if this is a chain of commands
    struct _URB_HCD_AREA hca;           // fields for HCD use

    //
    // this block contains transfer fields
    // specific to isochronous transfers
    //

    // 32 bit frame number to begin this transfer on, must be within 1000
    // frames of the current USB frame or an error is returned.

    // START_ISO_TRANSFER_ASAP flag in transferFlags:
    // If this flag is set and no transfers have been submitted
    // for the pipe then the transfer will begin on the next frame
    // and StartFrame will be updated with the frame number the transfer
    // was started on.
    // If this flag is set and the pipe has active transfers then 
    // the transfer will be queued to begin on the frame after the 
    // last transfer queued is completed.
    //
    ULONG StartFrame;
    // number of packets that make up this request
    ULONG NumberOfPackets;
    // number of packets that completed with errors
    ULONG ErrorCount;
#ifdef OSR21_COMPAT        
    USBD_ISO_PACKET_DESCRIPTOR IsoPacket[0]; 
#else     
    USBD_ISO_PACKET_DESCRIPTOR IsoPacket[1]; 
#endif    
};


typedef struct _URB {
    union {
            struct _URB_HEADER                           UrbHeader;
            struct _URB_SELECT_INTERFACE                 UrbSelectInterface;
            struct _URB_SELECT_CONFIGURATION             UrbSelectConfiguration;
            struct _URB_PIPE_REQUEST                     UrbPipeRequest;
            struct _URB_FRAME_LENGTH_CONTROL             UrbFrameLengthControl;
            struct _URB_GET_FRAME_LENGTH                 UrbGetFrameLength;
            struct _URB_SET_FRAME_LENGTH                 UrbSetFrameLength;
            struct _URB_GET_CURRENT_FRAME_NUMBER         UrbGetCurrentFrameNumber;
            struct _URB_CONTROL_TRANSFER                 UrbControlTransfer;
            struct _URB_BULK_OR_INTERRUPT_TRANSFER       UrbBulkOrInterruptTransfer;
            struct _URB_ISOCH_TRANSFER                   UrbIsochronousTransfer;

            // for standard control transfers on the default pipe
            struct _URB_CONTROL_DESCRIPTOR_REQUEST       UrbControlDescriptorRequest;
            struct _URB_CONTROL_GET_STATUS_REQUEST       UrbControlGetStatusRequest;
            struct _URB_CONTROL_FEATURE_REQUEST          UrbControlFeatureRequest;
            struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST  UrbControlVendorClassRequest;
            struct _URB_CONTROL_GET_INTERFACE_REQUEST    UrbControlGetInterfaceRequest;
            struct _URB_CONTROL_GET_CONFIGURATION_REQUEST UrbControlGetConfigurationRequest;
    };
}__attribute((aligned (8)))  URB, *PURB;


#endif /*  __USBDI_H__ */

