
/**********************************************************************
 * Describe the buffer sizes of the pipes use
 *
 * Don't meddle with pipe0, unless you know what you're doing.
 *
 * Unused pipes should be set to '0'
 */

#define PIPE_0_SZ_IN		64
#define PIPE_0_SZ_OUT		64

#define PIPE_1_SZ_IN		64
#define PIPE_1_SZ_OUT		64

#define PIPE_2_SZ_IN		8
#define PIPE_2_SZ_OUT		8

#define PIPE_3_SZ_IN		0
#define PIPE_3_SZ_OUT		0

#define PIPE_4_SZ_IN		0
#define PIPE_4_SZ_OUT		0

#define PIPE_5_SZ_IN		0
#define PIPE_5_SZ_OUT		0

#define PIPE_6_SZ_IN		0
#define PIPE_6_SZ_OUT		0

#define PIPE_7_SZ_IN		0
#define PIPE_7_SZ_OUT		0

#define PIPE_8_SZ_IN		0
#define PIPE_8_SZ_OUT		0


static const __code uint8_t deviceDescriptor[] =
{
	sizeof(deviceDescriptor),	// bLength
	DEVICE_DESCRIPTOR,		// bDescriptorType
	W16(0x110),			// bcdUSB
	0x02,				// bDeviceClass
	0x00,				// bDeviceSubClass
	0x00,				// bDeviceProtocl
	PIPE_0_SZ_OUT,			// bMaxPacketSize
	W16(0x0482),			// idVendor
	W16(0x0203),			// idProduct
	W16(0x0100),			// bcdDevice
	0x01,				// iManufacturer
	0x02,				// iProduct
	0x03, 				// iSerialNumber
	0x01 				// bNumConfigurations
};

CTASSERT(sizeof deviceDescriptor == 0x12);

static const __code uint8_t configDescriptor[] = {
	// Configuration descriptor
	0x09,			// bLength,
	0x02,			// bDescriptorType (Configuration)
	W16(sizeof configDescriptor),	// wTotalLength
	0x02,			// bNumInterfaces,
	0x01,			// bConfigurationValue
	0x00,			// iConfiguration,
	0xA0,			// bmAttributes ()
	0x10,			// bMaxPower

	// Control Interface descriptor
	0x09,			// bLength,
	INTERFACE_DESCRIPTOR,	// bDescriptorType (Interface)
	0x00,			// bInterfaceNumber,
	0x00,			// bAlternateSetting
	0x01,			// bNumEndpoints,
	0x02,			// bInterfaceClass
	0x02,			// bInterfaceSubclass,
	0x01,			// bInterfaceProtocol,
	0x00,			// iInterface

	// CDC Header
	0x05,			// bFunctionLength
	0x24,			// bDescriptorType: CS_INTERFACE
	0x00,			// bDescriptorSubtype: Header
	W16(0x0110),		// Version

	// CDC Call Managment Functional Descriptor
	0x05,			// bFunctionLength
	0x24,			// bDescriptorType: CS_INTERFACE
	0x01,			// bDescriptorSubtype: Call Management Func Desc 
	0x03,			// bmCapabilities: D0+D1 
	0x01,			// bDataInterface: 1 

	// CDC ACM Functional Descriptor
	0x04,			// bFunctionLength 
	0x24,			// bDescriptorType: CS_INTERFACE
	0x02,			// bDescriptorSubtype: Abstract Control Management desc
	0x02,			// bmCapabilities

	// CDC Union Descriptor
	0x05,			// bFunctionLength 
	0x24,			// bDescriptorType: CS_INTERFACE
	0x06,			// bDescriptorSubtype: Union
	0x00,			// Master
	0x01,			// Slave

	// Endpoint 
	0x07,			// bLength,
	ENDPOINT_DESCRIPTOR,	// bDescriptorType (Endpoint)
	0x82,			// bEndpointAddress
	0x03,			// bmAttributes (Interrupt)
	W16(8),			// wmaxPacketSize,
	0xff,			// bInterval (* 1 millisecond)

	// Data Interface descriptor
	0x09,			// bLength,
	INTERFACE_DESCRIPTOR,	// bDescriptorType (Interface)
	0x01,			// bInterfaceNumber,
	0x00,			// bAlternateSetting
	0x02,			// bNumEndpoints,
	0x0a,			// bInterfaceClass
	0x00,			// bInterfaceSubclass,
	0x00,			// bInterfaceProtocol,
	0x00,			// iInterface

	// Endpoint
	0x07,			// bLength,
	ENDPOINT_DESCRIPTOR,	// bDescriptorType (Endpoint)
	0x01,			// bEndpointAddress
	0x02,			// bmAttributes (Interrupt)
	W16(PIPE_1_SZ_IN),	// wmaxPacketSize,
	0x00,			// bInterval (1 millisecond)

	// Endpoint
	0x07,			// bLength,
	ENDPOINT_DESCRIPTOR,	// bDescriptorType (Endpoint)
	0x81,			// bEndpointAddress
	0x02,			// bmAttributes (Interrupt)
	W16(PIPE_1_SZ_OUT),	// wmaxPacketSize,
	0x00,			// bInterval (1 millisecond)
};

static const __code uint8_t stringDescriptor0[] = {
	sizeof(stringDescriptor0),
	STRING_DESCRIPTOR,
	W16(0x0409),
};

static const __code uint8_t stringDescriptor1[] = {
	sizeof(stringDescriptor1),
	STRING_DESCRIPTOR,
	W16('D'),
	W16('a'),
	W16('t'),
	W16('a'),
	W16('M'),
	W16('u'),
	W16('s'),
	W16('e'),
	W16('u'),
	W16('m'),
	W16('.'),
	W16('d'),
	W16('k'),
};

static const __code uint8_t stringDescriptor2[] = {
	sizeof(stringDescriptor2),
	STRING_DESCRIPTOR,
	W16('G'),
	W16('I'),
	W16('E'),
	W16('R'),
};

static const __code uint8_t stringDescriptor3[] = {
	sizeof(stringDescriptor3),
	STRING_DESCRIPTOR,
	W16('0'),
	W16('0'),
	W16('0'),
	W16('0'),
	W16('0'),
	W16('1'),
};

static const __code uint8_t * const stringDescriptors[] = {
	stringDescriptor0,
	stringDescriptor1,
	stringDescriptor2,
	stringDescriptor3
};
