#ifndef USBCOMMUNI_H_
#define USBCOMMUNI_H_

#include <thread>
#include "commondef.h"
#include "ios/ios_usb_communi.h"

namespace usbcommuni {

class USBCommuni
{
public:
    USBCommuni();
    ~USBCommuni();

    USBCommuniErrors_t Init();

    bool GetConnectStatus();

    void RecvHandleRegister(USBCommuniRecvHandleCb recvcb);

    USBCommuniErrors_t SendData(const char *data, uint32_t len, uint32_t &send_bytes);

private:
    void AndroidSubscribeHandler(USBCommuniEventTypes_t event);
    void IosSubscribeHandler(USBCommuniEventTypes_t event);
    void LoopHandler();

private:
    USBIosCommuni ios_;
    USBCommuniDeviceTypes_t type_;
    USBCommuniRecvHandleCb recvhandle_;
    std::thread loop_thread_;

    enum {
        USBCOMMUNI_IOS_ACTION_IDLE = 0,
        USBCOMMUNI_IOS_ACTION_ARRIVED,
        USBCOMMUNI_IOS_ACTION_REMOVE,
    } ios_actions_;
};

}

#endif /* USBCOMMUNI_H_ */
