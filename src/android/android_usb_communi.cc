#include "android_usb_communi.h"
#include <unistd.h>
#include <regex>

namespace usbcommuni {

#define WKRC_MANUFACTURER   "Walkera"
#define WKRC_MODLENAME      "T210 mini"
#define WKRC_DESCRIPTION    "T210 mini RC"
#define WKRC_VERSION        "1.0"
#define WKRC_URI            "https://fly.walkera.cn/a"
#define WKRC_SERIALNUMBER   "01linuxSerialNo"

#define EP_IN 0x81
#define EP_OUT 0x02

#define GOOGLE_VID 0x18d1
#define ACCESSORY_PID 0x2d01
#define ACCESSORY_PID_ALT 0x2d00

const USBDeviceId default_id[] = {
    {0x1d6b, 0x0003},
    {0x2109, 0x3431},
    {0x1d6b, 0x0002},
};

const uint16_t ios_vendor_id = 0x05ac;
const std::string ios_product_id_regx = "12[9a][0-9a-f]/*|/190[1-5]/*|/8600/*";

static unsigned char g_recvbuffer[1024*1024*2];

USBAndroidCommuni::USBAndroidCommuni()
{
    gadgetacci_ = {
        .manufacturer = WKRC_MANUFACTURER,
        .model_name = WKRC_MODLENAME,
        .description = WKRC_DESCRIPTION,
        .version = WKRC_VERSION,
        .uri = WKRC_URI,
        .serial_number = WKRC_SERIALNUMBER
    };

    context_ = nullptr;
    curr_id_ = {0};
    last_id_ = {0};
    device_type_ = USBANDROID_DEVICE_UNKNOWN;
    connect_status_ = false;
    exit_enable_ = false;
    loop_thead_exist_ = false;
    event_handle_ = nullptr;
    recv_handle_ = nullptr;
}

USBAndroidCommuni::~USBAndroidCommuni()
{
}

USBCommuniErrors_t USBAndroidCommuni::Init()
{
    int r;
    USBCommuniErrors_t err;

    r = libusb_init(&context_);
    if (LIBUSB_SUCCESS != r) {
        fprintf(stderr, "Libusb Init failed, err : %s\n", libusb_error_name(r));
        return USBCOMMUNI_E_IO;
    }

    err = HotplugEventRegister();
    if (USBCOMMUNI_E_SUCCESS != r) {
        fprintf(stderr, "USB hotplug event regitser failed\n");
        return USBCOMMUNI_E_IO;
    }

    loop_thread_ = std::thread(&USBAndroidCommuni::LoopThreadHandler, this);
    loop_thread_.detach();

    open_thread_ = std::thread(&USBAndroidCommuni::OpenThreadHandler, this);
    open_thread_.detach();

    return USBCOMMUNI_E_SUCCESS;
}

void USBAndroidCommuni::Deinit()
{
    exit_enable_ = true;
}

static int HotplugCallback(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data)
{
    int r;
    char id_product[8] = {0};
    libusb_device_descriptor descriptor;
    USBAndroidCommuni *android = (USBAndroidCommuni *)user_data;

    r = libusb_get_device_descriptor(device, &descriptor);
    if (r)
        return -1;

    fprintf(stderr, "\033[32m## USB Device %04x:%04x %s\033[0m\n", descriptor.idVendor, descriptor.idProduct, 
            event==LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED ? "Inserted" : "Remove");

    /* 过滤IOS USB设备 */
    if (descriptor.idVendor == ios_vendor_id) {
        std::regex reg(ios_product_id_regx);
        sprintf(id_product, "%04x", descriptor.idProduct);
        fprintf(stderr, "********* id_product : %s, regex : %d\n", id_product, std::regex_match(id_product, reg));

        if (std::regex_match(id_product, reg)) {
            fprintf(stderr, "IOS Device : %04x:%04x\n", descriptor.idVendor, descriptor.idProduct);
            return -1;
        }
    }

    /* 过滤系统默认USB设备 */
    for (size_t i = 0; i < sizeof(default_id)/sizeof(USBDeviceId); i++) {
        if ((descriptor.idVendor == default_id[i].vendor) && 
            (descriptor.idProduct == default_id[i].product)) {
            fprintf(stderr, "System defualt USB device [%ld] !\n", i);
            return -1;
        }
    }

    android->SetEventInfo(event, device);

    return 0;
}

USBCommuniErrors_t USBAndroidCommuni::HotplugEventRegister()
{
    int r;
    int vendor_id;
    int product_id;
    int dev_class;
    libusb_hotplug_event events;
    libusb_hotplug_flag flags;

    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        fprintf(stderr, "this version's libusb doesn't support hotplug\n");
        return USBCOMMUNI_E_IO;
    }

    vendor_id = LIBUSB_HOTPLUG_MATCH_ANY;
    product_id = LIBUSB_HOTPLUG_MATCH_ANY;
    dev_class = LIBUSB_HOTPLUG_MATCH_ANY;
    events = static_cast <libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT);
    flags = LIBUSB_HOTPLUG_ENUMERATE;

    r = libusb_hotplug_register_callback(context_,
                                         events, 
                                         flags, 
                                         vendor_id, 
                                         product_id, 
                                         dev_class, 
                                         HotplugCallback, 
                                         this, 
                                         &hotplug_handle_);
    if (r != LIBUSB_SUCCESS) {
        fprintf(stderr, "resigter hotplug_callback failed\n");
        return USBCOMMUNI_E_IO;
    }

    return USBCOMMUNI_E_SUCCESS;
}

void USBAndroidCommuni::HotplugEventDisregister()
{
    libusb_hotplug_deregister_callback(context_, hotplug_handle_);
}

void USBAndroidCommuni::SubscribeRegister(USBCommuniEventCb eventcb)
{
    event_handle_ = eventcb;
}

bool USBAndroidCommuni::GetConnectStatus()
{
    return connect_status_;
}

static void TansferSendCallback(libusb_transfer *transfer)
{
    if (nullptr == transfer)
        return;

    switch (transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        break;
    case LIBUSB_TRANSFER_ERROR:
        break;
    case LIBUSB_TRANSFER_TIMED_OUT:
        break;
    case LIBUSB_TRANSFER_CANCELLED:
        break;
    case LIBUSB_TRANSFER_NO_DEVICE:
        break;
    case LIBUSB_TRANSFER_OVERFLOW:
        break;
    default:
        break;
    }

error:
    libusb_free_transfer(transfer);
    transfer = nullptr;    
}

USBCommuniErrors_t USBAndroidCommuni::SendData(const char *data, uint32_t data_size, uint32_t &send_bytes)
{
    int r;
    libusb_transfer *transfer;

    if ((nullptr == google_.handle) || (nullptr == data) || (data_size == 0))
        return USBCOMMUNI_E_INVAIL_ARG;

    transfer = libusb_alloc_transfer(0);
    if (nullptr == transfer) {
        fprintf(stderr, "usb alloc transfer fialed\n");
        return USBCOMMUNI_E_IO;
    }

    libusb_fill_bulk_transfer(transfer, 
                              google_.handle, 
                              google_.ep_out, 
                              reinterpret_cast<unsigned char*>(const_cast<char*>(data)), 
                              data_size, 
                              TansferSendCallback, 
                              this, 
                              1000);

    r = libusb_submit_transfer(transfer);
    if (LIBUSB_SUCCESS != r) {
        fprintf(stderr, "usb submit transfer failed, err: %s\n", libusb_error_name(r));
        libusb_free_transfer(transfer);
        return USBCOMMUNI_E_IO;
    }

    return USBCOMMUNI_E_SUCCESS;
}

void USBAndroidCommuni::RecvHandleRegister(USBCommuniRecvHandleCb recvcb)
{
    recv_handle_ = recvcb;
}

static USBCommuniErrors_t GetDeviceAttr(libusb_device *device, 
                                        uint16_t &vendor_id,
                                        uint16_t &product_id,
                                        uint16_t &ep_in, 
                                        uint16_t &ep_out, 
                                        uint16_t &interface, 
                                        uint16_t &packet_size)
{
    int r;
    libusb_device_descriptor descriptor;
    const libusb_interface_descriptor *intf_desc_found = nullptr;

    vendor_id = 0;
    product_id = 0;
    ep_in = 0;
    ep_out = 0;
    interface = 0;
    packet_size = 0;

    if (nullptr == device)
        return USBCOMMUNI_E_INVAIL_ARG;

    r = libusb_get_device_descriptor(device, &descriptor);
    if (r != LIBUSB_SUCCESS) {
        fprintf(stderr, "could not get device descriptor for device : %s\n", libusb_error_name(r));
        return USBCOMMUNI_E_IO;
    }

    for (size_t config_idx = 0; config_idx < descriptor.bNumConfigurations; config_idx++) {
        libusb_config_descriptor *config = nullptr;

        r = libusb_get_config_descriptor(device, config_idx, &config);
        if (r != LIBUSB_SUCCESS) {
            fprintf(stderr, "could not get configuration descriptor %ld for device 0x%04x:0x%04x: %s\n",
                    config_idx, descriptor.idVendor, descriptor.idProduct, libusb_error_name(r));
            continue;
        }

        for (size_t interface_idx = 0; interface_idx < config->bNumInterfaces; interface_idx++) {
            const libusb_interface_descriptor *intf_desc = &config->interface[interface_idx].altsetting[0];
            int interface_num = intf_desc->bInterfaceNumber;

            /* check endpoints */
            if (intf_desc->bNumEndpoints < 2) {
                fprintf(stderr, "skipping interface %d, has only %d endpoints\n",
                            interface_num, intf_desc->bNumEndpoints);
                continue;
            }

            for (size_t endpoints_idx = 0; endpoints_idx < intf_desc->bNumEndpoints; endpoints_idx++) {
                if ((intf_desc->endpoint[endpoints_idx].bmAttributes & 3) == LIBUSB_TRANSFER_TYPE_BULK &&
                        (intf_desc->endpoint[endpoints_idx].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_OUT)
                    ep_out = intf_desc->endpoint[endpoints_idx].bEndpointAddress;

                if ((intf_desc->endpoint[endpoints_idx].bmAttributes & 3) == LIBUSB_TRANSFER_TYPE_BULK &&
                        (intf_desc->endpoint[endpoints_idx].bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN)
                    ep_in = intf_desc->endpoint[endpoints_idx].bEndpointAddress;

                if ((ep_in != 0) && (ep_out != 0))
                    break;
            }

            if ((ep_in == 0) || (ep_out == 0))
                continue;


            if (!intf_desc_found) {
                intf_desc_found = intf_desc;
                break;
            }
        }

        if (nullptr != config) {
            libusb_free_config_descriptor(config);
            config = nullptr;
        }

        if (!intf_desc_found)
            continue;

        interface = intf_desc_found->bInterfaceNumber;
        packet_size = intf_desc_found->endpoint[0].wMaxPacketSize;
    }

    if (nullptr == intf_desc_found)
        return USBCOMMUNI_E_IO;

    vendor_id = descriptor.idVendor;
    product_id = descriptor.idProduct;

    fprintf(stderr, "*************************************\n");
    fprintf(stderr, "USB device [%04x:%04x]:\n", vendor_id, product_id);
    fprintf(stderr, "\t Endpoint input  : %02x\n", ep_in);
    fprintf(stderr, "\t Endpoint output : %02x\n", ep_out);
    fprintf(stderr, "\t interface       : %d\n", interface);
    fprintf(stderr, "\t packet size     : %d\n", packet_size);
    fprintf(stderr, "*************************************\n");

    return USBCOMMUNI_E_SUCCESS;
}

void USBAndroidCommuni::SetEventInfo(libusb_hotplug_event event, libusb_device *device)
{
    USBCommuniErrors_t err;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t ep_in;
    uint16_t ep_out;
    uint16_t interface;
    uint16_t packet_size;
    USBAndroidDeviceTypes_t type;

    err = GetDeviceAttr(device, vendor_id, product_id, ep_in, ep_out, interface, packet_size);
    if (err != USBCOMMUNI_E_SUCCESS)
        return;

    if ((vendor_id == GOOGLE_VID) && 
        ((product_id == ACCESSORY_PID) || (product_id == ACCESSORY_PID_ALT)))
    {
        type = USBANDROID_DEVICE_GOOGLE;
        google_.device = device;
        google_.id.vendor = vendor_id;
        google_.id.product = product_id;
        google_.ep_in = ep_in;
        google_.ep_out = ep_out;
        google_.interface = interface;
        google_.packet_size = packet_size;
    } else {
        type = USBANDROID_DEVICE_ANDROID;
        phone_.device = device;
        phone_.id.vendor = vendor_id;
        phone_.id.product = product_id;
        phone_.ep_in = ep_in;
        phone_.ep_out = ep_out;
        phone_.interface = interface;
        phone_.packet_size = packet_size;
    }

    this->event_ = event;
    this->curr_id_.vendor = vendor_id;
    this->curr_id_.product = product_id;
    this->device_type_ = type;

    if (nullptr != event_handle_)
        event_handle_(((event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) ? \
                            USBCOMMUNI_DEVICE_ADD : USBCOMMUNI_DEVICE_REMOVE));
}

void USBAndroidCommuni::LoopThreadHandler()
{
    loop_thead_exist_ = true;

    while (true) {
        if (true == exit_enable_)
            break;

        libusb_handle_events(context_);
    }

    loop_thead_exist_ = false;
}

void USBAndroidCommuni::OpenThreadHandler()
{
    USBCommuniErrors_t err;
    bool ck_keep_alive = true;

    while (true) {
        // fprintf(stderr, "--------------------------------------------\n");
        // fprintf(stderr, "_OpenThreadHandler status:\n");
        // fprintf(stderr, "\t event   : %d,        ck_keep_alive : %d\n", event, ck_keep_alive);
        // fprintf(stderr, "\t curr_id : %04x:%04x, last_id : %04x:%04x\n", curr_id.vendor, curr_id.product, 
        //                                                                  last_id.vendor, last_id.product);
        // fprintf(stderr, "\t device type: %d\n", device_type);
        // fprintf(stderr, "--------------------------------------------\n");

        if ((true == exit_enable_) && (false == loop_thead_exist_))
            break;

        if (event_ == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
            if ((curr_id_.vendor == last_id_.vendor) && 
                (curr_id_.product == last_id_.product))
            {
                ck_keep_alive = true;
                goto delay200ms;
            }

            switch (device_type_) {
            case USBANDROID_DEVICE_ANDROID:
                if (ck_keep_alive) {
                    ck_keep_alive = false;
                    goto delay2000ms;
                }

                err = OpenUsbDevice();
                if (USBCOMMUNI_E_SUCCESS != err)
                    goto delay1000ms;

                err = SetupUsbToAccessory();
                if (USBCOMMUNI_E_SUCCESS != err) {
                    CloseUsbDevice();
                    goto delay1000ms;
                }
                last_id_ = curr_id_;
                break;
            
            case USBANDROID_DEVICE_GOOGLE:
                err = OpenAccessoryDevice();
                if (USBCOMMUNI_E_SUCCESS != err)
                    goto delay1000ms;

                err = ConfigAsyncRead();
                if (USBCOMMUNI_E_SUCCESS != err) {
                    CloseAccessoryDevice();
                    goto delay1000ms;
                }

                connect_status_ = true;
                last_id_ = curr_id_;
                break;

            default:
                goto delay200ms;
                break;
            }
        } else {
            if ((curr_id_.vendor == 0) && (last_id_.vendor == 0))
                goto delay1000ms;

            if ((curr_id_.vendor != last_id_.vendor) ||
                (curr_id_.product != last_id_.product))
            {
                goto delay200ms;
            }

            switch (device_type_) {
            case USBANDROID_DEVICE_ANDROID:
                CloseUsbDevice();
                last_id_ = {0};
                break;
            
            case USBANDROID_DEVICE_GOOGLE:
                CloseAccessoryDevice();
                connect_status_ = false;
                last_id_ = {0};
                break;

            default:
                break;
            }
        }

delay0s:
        continue;
delay200ms:
        usleep(200*1000);
        continue;
delay1000ms:
        usleep(1000*1000);
        continue;
delay2000ms:
        usleep(2000*1000);
        continue;
    }

    libusb_hotplug_deregister_callback(context_, hotplug_handle_);
    if (nullptr != context_) {
        libusb_exit(context_);
        context_ = nullptr;
    }
}

USBCommuniErrors_t USBAndroidCommuni::OpenUsbDevice()
{
    phone_.handle = libusb_open_device_with_vid_pid(context_, phone_.id.vendor, phone_.id.product);
    if (nullptr == phone_.handle) {
        fprintf(stdout, "Problem acquireing handle\n");
        return USBCOMMUNI_E_IO;
    }

    libusb_claim_interface(phone_.handle, 0);

    return USBCOMMUNI_E_SUCCESS;
}

void USBAndroidCommuni::CloseUsbDevice()
{
    if (phone_.handle != nullptr) {
        libusb_release_interface(phone_.handle, 0);
        libusb_close(phone_.handle);
        phone_.handle = nullptr;
    }
}

USBCommuniErrors_t USBAndroidCommuni::SetupUsbToAccessory()
{
    unsigned char io_buffer[2];
	int version;
	int err;

    err = libusb_control_transfer(phone_.handle, 0xC0, 51, 0, 0, io_buffer, 2, 0);
    if (err < 0)
        return USBCOMMUNI_E_IO;

	version = io_buffer[1] << 8 | io_buffer[0];

    usleep(1000);

    if (UsbSendCtrl(gadgetacci_.manufacturer, 52, 0) != USBCOMMUNI_E_SUCCESS)
        return USBCOMMUNI_E_IO;
    if (UsbSendCtrl(gadgetacci_.model_name, 52, 1) != USBCOMMUNI_E_SUCCESS)
        return USBCOMMUNI_E_IO;
    if (UsbSendCtrl(gadgetacci_.description, 52, 2) != USBCOMMUNI_E_SUCCESS)
        return USBCOMMUNI_E_IO;
    if (UsbSendCtrl(gadgetacci_.version, 52, 3) != USBCOMMUNI_E_SUCCESS)
        return USBCOMMUNI_E_IO;
    if (UsbSendCtrl(gadgetacci_.uri, 52, 4) != USBCOMMUNI_E_SUCCESS)
        return USBCOMMUNI_E_IO;
    if (UsbSendCtrl(gadgetacci_.serial_number, 52, 5) != USBCOMMUNI_E_SUCCESS)
        return USBCOMMUNI_E_IO;
    if (UsbSendCtrl(nullptr, 53, 0) != USBCOMMUNI_E_SUCCESS)
        return USBCOMMUNI_E_IO;

    if (phone_.handle != nullptr) {
        libusb_release_interface(phone_.handle, 0);
        libusb_close(phone_.handle);
        phone_.handle = nullptr;
    }

    return USBCOMMUNI_E_SUCCESS;
}

USBCommuniErrors_t USBAndroidCommuni::OpenAccessoryDevice()
{
    google_.handle = libusb_open_device_with_vid_pid(context_, google_.id.vendor, google_.id.product);
    if (nullptr == google_.handle) {
        fprintf(stderr, "open accessory device failed\n");
        return USBCOMMUNI_E_IO;
    }

    libusb_claim_interface(google_.handle, 0);
    fprintf(stdout, "Interface claimed, ready to transfer data\n");

    return USBCOMMUNI_E_SUCCESS;
}

void USBAndroidCommuni::CloseAccessoryDevice()
{
    if (nullptr != google_.handle) {
        libusb_release_interface(google_.handle, 0);
        libusb_close(google_.handle);
        google_.handle = nullptr;
    }
}

USBCommuniErrors_t USBAndroidCommuni::UsbSendCtrl(const char *buff, int req, int index)
{
    int r;

    if (nullptr == phone_.handle)
        return USBCOMMUNI_E_INVAIL_ARG;

    if (nullptr != buff)
        r = libusb_control_transfer(phone_.handle, 0x40, req, 0, index, (unsigned char*)buff, (uint16_t)strlen(buff) + 1 , 0);
    else
        r = libusb_control_transfer(phone_.handle, 0x40, req, 0, index, (unsigned char*)buff, 0, 0);

    if (r < 0)
        return USBCOMMUNI_E_IO;

    return USBCOMMUNI_E_SUCCESS;
}

static void TransferRecvCallback(libusb_transfer *transfer)
{
    int r;
    USBAndroidCommuni *android = (USBAndroidCommuni*)transfer->user_data;

    if ((nullptr == transfer) || (android == nullptr))
        return;

    switch (transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        if (transfer->actual_length > 0) {
            if (android->recv_handle_ != nullptr)
                android->recv_handle_((const char*)transfer->buffer, transfer->actual_length);
        }

        r = libusb_submit_transfer(transfer);
        if (r) {
            fprintf(stderr, "error libusb_submit_transfer : %s\n", libusb_strerror(libusb_error(r)));
            libusb_cancel_transfer(transfer);
        }
        break;
    
    case LIBUSB_TRANSFER_CANCELLED:
        libusb_free_transfer(transfer);
        break;

    default:
        break;
    }
}

USBCommuniErrors_t USBAndroidCommuni::ConfigAsyncRead()
{
    int r;
    libusb_transfer *transfer;

    if (nullptr == google_.handle)
        return USBCOMMUNI_E_INVAIL_ARG;

    transfer = libusb_alloc_transfer(0);
    if (nullptr == transfer) {
        fprintf(stderr, "usb alloc transfer fialed, err\n");
        return USBCOMMUNI_E_IO;
    }

    memset(g_recvbuffer, 0, sizeof(g_recvbuffer));
    transfer->actual_length = 0;

    libusb_fill_bulk_transfer(transfer,
                              google_.handle, 
                              google_.ep_in, 
                              g_recvbuffer, 
                              sizeof(g_recvbuffer),
                              TransferRecvCallback, 
                              this,
                              0);

    r = libusb_submit_transfer(transfer);
    if (LIBUSB_SUCCESS != r) {
        fprintf(stderr, "usb submit transfer failed, err: %s\n", libusb_error_name(r));
        libusb_free_transfer(transfer);
        return USBCOMMUNI_E_IO;
    }

    return USBCOMMUNI_E_SUCCESS;
}


}


