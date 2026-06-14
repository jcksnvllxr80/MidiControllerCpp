// TinyUSB descriptors for the raw-USB editor link: a single VENDOR-class
// interface with bulk IN/OUT endpoints, plus a BOS + MS-OS-2.0 descriptor set
// so Windows auto-loads WinUSB (no Zadig, no .inf) and the host app's `nusb`
// transport can claim it. The MS-OS-2.0 blob is the canonical TinyUSB
// webusb_serial pattern; TU_VERIFY_STATIC guards its length at compile time.
//
// Only compiled with -DMC_ENABLE_USB_EDITOR=ON. NOT verified on hardware here
// (no SDK/board in this environment) — build in WSL and flash to confirm
// enumeration + WinUSB binding. Must stay in sync with the host's
// MidiControllerControllerApp/src-tauri/src/transport/usb.rs constants
// (VID 0xCAFE, PID 0x4001, interface 0, EP OUT 0x01, EP IN 0x81).
#include "tusb.h"

#define USB_VID 0xCAFE
#define USB_PID 0x4001
#define USB_BCD 0x0210  // 2.1 — required so the host requests the BOS descriptor

// ---- Device descriptor ------------------------------------------------------
static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD,
    .bDeviceClass = 0x00,     // defined at interface level (vendor)
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

const uint8_t* tud_descriptor_device_cb(void) { return (const uint8_t*)&desc_device; }

// ---- Configuration descriptor ----------------------------------------------
enum { ITF_NUM_VENDOR = 0, ITF_NUM_TOTAL };

#define EPNUM_VENDOR_OUT 0x01  // host -> device
#define EPNUM_VENDOR_IN 0x81   // device -> host

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    // Interface, string index, EP OUT, EP IN, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 4, EPNUM_VENDOR_OUT, EPNUM_VENDOR_IN, 64),
};

const uint8_t* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

// ---- BOS + MS-OS-2.0 (WinUSB auto-binding) ----------------------------------
#define VENDOR_REQUEST_MICROSOFT 1
#define MS_OS_20_DESC_LEN 0xB2

#define BOS_TOTAL_LEN (TUD_BOS_DESC_LEN + TUD_BOS_MICROSOFT_OS_DESC_LEN)

static const uint8_t desc_bos[] = {
    TUD_BOS_DESCRIPTOR(BOS_TOTAL_LEN, 1),
    TUD_BOS_MS_OS_20_DESCRIPTOR(MS_OS_20_DESC_LEN, VENDOR_REQUEST_MICROSOFT),
};

const uint8_t* tud_descriptor_bos_cb(void) { return desc_bos; }

static const uint8_t desc_ms_os_20[] = {
    // Set header: length, type, windows version, total length
    U16_TO_U8S_LE(0x000A), U16_TO_U8S_LE(MS_OS_20_SET_HEADER_DESCRIPTOR),
    U32_TO_U8S_LE(0x06030000), U16_TO_U8S_LE(MS_OS_20_DESC_LEN),

    // Configuration subset header: length, type, config index, reserved, total length
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_CONFIGURATION), 0, 0,
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A),

    // Function subset header: length, type, first interface, reserved, subset length
    U16_TO_U8S_LE(0x0008), U16_TO_U8S_LE(MS_OS_20_SUBSET_HEADER_FUNCTION), ITF_NUM_VENDOR, 0,
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08),

    // MS OS 2.0 Compatible ID descriptor: length, type, compatible ID, sub-compatible ID
    U16_TO_U8S_LE(0x0014), U16_TO_U8S_LE(MS_OS_20_FEATURE_COMPATBLE_ID), 'W', 'I', 'N', 'U', 'S', 'B',
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // MS OS 2.0 Registry property descriptor: length, type
    U16_TO_U8S_LE(MS_OS_20_DESC_LEN - 0x0A - 0x08 - 0x08 - 0x14),
    U16_TO_U8S_LE(MS_OS_20_FEATURE_REG_PROPERTY),
    U16_TO_U8S_LE(0x0007), U16_TO_U8S_LE(0x002A),  // wPropertyDataType (REG_MULTI_SZ), wPropertyNameLength
    // PropertyName: "DeviceInterfaceGUIDs\0" (UTF-16-LE)
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00,
    'e', 0x00, 'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00,
    'D', 0x00, 's', 0x00, 0x00, 0x00,
    U16_TO_U8S_LE(0x0050),  // wPropertyDataLength
    // PropertyData: "{975F44D9-0D08-43FD-8B3E-127CA8AFFF9D}\0\0" (UTF-16-LE)
    '{', 0x00, '9', 0x00, '7', 0x00, '5', 0x00, 'F', 0x00, '4', 0x00, '4', 0x00, 'D', 0x00, '9', 0x00, '-', 0x00,
    '0', 0x00, 'D', 0x00, '0', 0x00, '8', 0x00, '-', 0x00, '4', 0x00, '3', 0x00, 'F', 0x00, 'D', 0x00, '-', 0x00,
    '8', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, '-', 0x00, '1', 0x00, '2', 0x00, '7', 0x00, 'C', 0x00, 'A', 0x00,
    '8', 0x00, 'A', 0x00, 'F', 0x00, 'F', 0x00, 'F', 0x00, '9', 0x00, 'D', 0x00, '}', 0x00, 0x00, 0x00, 0x00, 0x00,
};

TU_VERIFY_STATIC(sizeof(desc_ms_os_20) == MS_OS_20_DESC_LEN, "MS OS 2.0 descriptor length mismatch");

// Answer the Microsoft vendor request (wIndex == 7) with the descriptor set.
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    if (stage != CONTROL_STAGE_SETUP) return true;
    switch (request->bRequest) {
        case VENDOR_REQUEST_MICROSOFT:
            if (request->wIndex == 7) {
                uint16_t total_len;
                memcpy(&total_len, desc_ms_os_20 + 8, 2);
                return tud_control_xfer(rhport, request, (void*)(uintptr_t)desc_ms_os_20, total_len);
            }
            return false;
        default:
            return false;
    }
}

// ---- String descriptors -----------------------------------------------------
static const char* string_desc_arr[] = {
    (const char[]){0x09, 0x04},      // 0: language = English (0x0409)
    "MidiController",                 // 1: Manufacturer
    "MidiController Editor Link",     // 2: Product
    "000001",                        // 3: Serial
    "MidiController Vendor Link",     // 4: Vendor interface
};

static uint16_t desc_str[32];

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    uint8_t chr_count;
    if (index == 0) {
        desc_str[1] = 0x0409;
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        const char* str = string_desc_arr[index];
        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; ++i) desc_str[1 + i] = str[i];
    }
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc_str;
}
