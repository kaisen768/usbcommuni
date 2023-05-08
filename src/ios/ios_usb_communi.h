#ifndef IOS_USB_COMMUNI_H_
#define IOS_USB_COMMUNI_H_

#include "commondef.h"
#include "cclqueue/blocking_queue.h"
#include "libimobiledevice/libimobiledevice.h"
#include "plist/plist.h"

namespace usbcommuni {

#define USBMUXD_DEFAUL_PORT 12345
#define QUEUE_DEFAULT_SIZE  10000
#define SENDBUFFER_SIZE     65536
#define RECVBUFFER_SIZE     65536

class USBIosCommuni
{
public:
    explicit USBIosCommuni(uint16_t port = USBMUXD_DEFAUL_PORT);
    ~USBIosCommuni();

    USBCommuniErrors_t Init();

    USBCommuniErrors_t HotplugEventRegister();

    void HotplugEventDisregister();

    void SubscribeRegister(USBCommuniEventCb eventcb);

    bool GetConnectStatus();

    const std::string &GetUdid();

    bool IsFoundUSBDevice();

    void RecvHandleRegister(USBCommuniRecvHandleCb recvcb);

    USBCommuniErrors_t SendData(const char *data, uint32_t data_size, uint32_t &send_bytes);

    void _RecvThreadHandler();
    void _SendThreadHandler();

public:
    int efd_;
    idevice_t device_;
    idevice_connection_t connection_;
    std::string udid_;
    USBCommuniEventCb event_handle_;
    USBCommuniRecvHandleCb recv_handle_;
    bool found_device_;
    std::thread recv_thread_;
    std::thread send_thread_;

private:
    uint16_t port_;
    cclqueue::BlockingQueue* queue_;
    char* sendbuffer;
    bool connect_status_;
};

}

#endif /* IOS_USB_COMMUNI_H_ */
