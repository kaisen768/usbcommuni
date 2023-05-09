#ifndef ANDROID_USB_COMMUNI_H_
#define ANDROID_USB_COMMUNI_H_

#include <thread>
#include "commondef.h"
#include "libusb-1.0/libusb.h"

namespace usbcommuni {

struct USBGadgetAccessoryInfo {
    const char* manufacturer;
    const char* model_name;
    const char* description;
    const char* version;
    const char* uri;
    const char* serial_number;
};

typedef struct USBDeviceId {
    uint16_t vendor;
    uint16_t product;
} USBDeviceId;

typedef enum USBAndroidDeviceTypes {
    USBANDROID_DEVICE_ANDROID,
    USBANDROID_DEVICE_GOOGLE,
    USBANDROID_DEVICE_UNKNOWN,
} USBAndroidDeviceTypes_t;

typedef struct USBDeviceAttr {
    USBDeviceId id;
    libusb_device* device;
    libusb_device_handle* handle;
    USBAndroidDeviceTypes_t type;
    uint16_t interface;
    uint16_t ep_in;
    uint16_t ep_out;
    uint16_t packet_size;

    USBDeviceAttr() {
        id = {0};
        device = nullptr;
        handle = nullptr;
        type = USBANDROID_DEVICE_UNKNOWN;
        interface = 0;
        ep_in = 0;
        ep_out = 0;
    }
} USBDeviceAttr_t;

class USBAndroidCommuni
{
public:
    USBAndroidCommuni();
    ~USBAndroidCommuni();

    USBCommuniErrors_t Init();
    void Deinit();

    USBCommuniErrors_t HotplugEventRegister();
    void HotplugEventDisregister();

    void SubscribeRegister(USBCommuniEventCb eventcb);

    bool GetConnectStatus();

    USBCommuniErrors_t SendData(const char *data, uint32_t data_size, uint32_t &send_bytes);

    void RecvHandleRegister(USBCommuniRecvHandleCb recvcb);

    void SetEventInfo(libusb_hotplug_event event, libusb_device *device);

public:
    USBCommuniRecvHandleCb recv_handle_;

private:
    void LoopThreadHandler();
    void OpenThreadHandler();
    USBCommuniErrors_t OpenUsbDevice();
    void CloseUsbDevice();
    USBCommuniErrors_t SetupUsbToAccessory();
    USBCommuniErrors_t OpenAccessoryDevice();
    void CloseAccessoryDevice();
    USBCommuniErrors_t UsbSendCtrl(const char *buff, int req, int index);
    USBCommuniErrors_t ConfigAsyncRead();

private:
    struct USBGadgetAccessoryInfo gadgetacci_;
    libusb_context* context_;
    libusb_hotplug_callback_handle hotplug_handle_;
    libusb_hotplug_event event_;
    std::thread loop_thread_;
    std::thread open_thread_;
    USBDeviceId curr_id_;
    USBDeviceId last_id_;
    USBAndroidDeviceTypes_t device_type_;
    bool connect_status_;
    bool exit_enable_;
    bool loop_thead_exist_;
    USBDeviceAttr_t phone_;
    USBDeviceAttr_t google_;
    USBCommuniEventCb event_handle_;
};

}

#endif /* ANDROID_USB_COMMUNI_H_ */
