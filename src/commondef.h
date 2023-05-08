#ifndef USB_COMMONDEF_H_
#define USB_COMMONDEF_H_

#include <functional>

namespace usbcommuni {

typedef enum USBCommuniDeviceTypes {
    USBCOMMUNI_DEVICE_TYPE_UNKNOWD = 0, /**< USB 设备类型未知 */
    USBCOMMUNI_DEVICE_TYPE_ANDROID,     /**< USB Android */
    USBCOMMUNI_DEVICE_TYPE_IOS,         /**< USB IOS (IPhone, MAC) */
} USBCommuniDeviceTypes_t;

typedef enum USBCommuniErrors {
    USBCOMMUNI_E_SUCCESS    =  0,
    USBCOMMUNI_E_INVAIL_ARG = -1,
    USBCOMMUNI_E_IO         = -2,
    USBCOMMUNI_E_NOT_CONN   = -3,
    USBCOMMUNI_E_VERSION    = -4,
    USBCOMMUNI_E_UNKNOWN    = -5,
    USBCOMMUNI_E_NMEN       = -6
} USBCommuniErrors_t;

typedef enum USBCommuniEventTypes {
    USBCOMMUNI_DEVICE_NONE = 0,     /**< device none occur */
    USBCOMMUNI_DEVICE_ADD,          /**< device was add */
    USBCOMMUNI_DEVICE_REMOVE,       /**< device was remove */
    USBCOMMUNI_DEVICE_PAIRED        /**< device completed pairing process */
} USBCommuniEventTypes_t;

typedef std::function<void (USBCommuniEventTypes_t)> USBCommuniEventCb;
typedef std::function<void (const char *data, uint32_t datal)> USBCommuniRecvHandleCb;

}

#endif /* USB_COMMONDEF_H_ */
