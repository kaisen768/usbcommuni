#include "usbcommuni.h"
#include <unistd.h>

namespace usbcommuni {

USBCommuni::USBCommuni()
{
    type_ = USBCOMMUNI_DEVICE_TYPE_UNKNOWD;
    recvhandle_ = nullptr;
    ios_actions_ = USBCOMMUNI_IOS_ACTION_IDLE;
}

USBCommuni::~USBCommuni()
{
}

USBCommuniErrors_t USBCommuni::Init()
{
    USBCommuniEventCb ios_subscribe_cb = [this](USBCommuniEventTypes_t event){IosSubscribeHandler(event);};
    USBCommuniEventCb android_subscribe_cb = [this](USBCommuniEventTypes_t event){AndroidSubscribeHandler(event);};

    ios_.Init();
    android_.Init();

    ios_.SubscribeRegister(ios_subscribe_cb);
    android_.SubscribeRegister(android_subscribe_cb);

    loop_thread_ = std::thread(&USBCommuni::LoopHandler, this);
    loop_thread_.detach();

    return USBCOMMUNI_E_SUCCESS;
}

bool USBCommuni::GetConnectStatus()
{
    switch (type_) {
    case USBCOMMUNI_DEVICE_TYPE_ANDROID:
        return android_.GetConnectStatus();
    
    case USBCOMMUNI_DEVICE_TYPE_IOS:
        return ios_.GetConnectStatus();

    default:
        return false;
    }
}

void USBCommuni::RecvHandleRegister(USBCommuniRecvHandleCb recvcb)
{
    recvhandle_ = recvcb;

    ios_.RecvHandleRegister(recvhandle_);
    android_.RecvHandleRegister(recvhandle_);
}

USBCommuniErrors_t USBCommuni::SendData(const char *data, uint32_t len, uint32_t &send_bytes)
{
    USBCommuniErrors_t err = USBCOMMUNI_E_NOT_CONN;

    switch (type_) {
    case USBCOMMUNI_DEVICE_TYPE_ANDROID:
        err = android_.SendData(data, len, send_bytes);
        break;
    
    case USBCOMMUNI_DEVICE_TYPE_IOS:
        err = ios_.SendData(data, len, send_bytes);
        break;

    default:
        break;
    }

    return err;
}

void USBCommuni::AndroidSubscribeHandler(USBCommuniEventTypes_t event)
{
    type_ = (event == USBCOMMUNI_DEVICE_ADD) ? USBCOMMUNI_DEVICE_TYPE_ANDROID : USBCOMMUNI_DEVICE_TYPE_UNKNOWD;
}

void USBCommuni::IosSubscribeHandler(USBCommuniEventTypes_t event)
{
    switch (event) {
    case USBCOMMUNI_DEVICE_ADD:
        type_ = USBCOMMUNI_DEVICE_TYPE_IOS;
        ios_actions_ = USBCOMMUNI_IOS_ACTION_ARRIVED;
        break;

    case USBCOMMUNI_DEVICE_REMOVE:
        type_ = USBCOMMUNI_DEVICE_TYPE_UNKNOWD;
        ios_actions_ = USBCOMMUNI_IOS_ACTION_REMOVE;
        break;

    default:
        ios_actions_ = USBCOMMUNI_IOS_ACTION_IDLE;
        break;
    }
}

void USBCommuni::LoopHandler()
{
    int count = 0;
    bool doonce = false;

    while (true) {
        if (doonce && count++ > 4) {
            doonce = false;
            count = 0;
            ios_.HotplugEventRegister();
            android_.HotplugEventRegister();
        }

        switch (ios_actions_) {
        case USBCOMMUNI_IOS_ACTION_ARRIVED:
            ios_actions_ = USBCOMMUNI_IOS_ACTION_IDLE;
            android_.HotplugEventDisregister();
            fprintf(stderr, "USBCOMMUNI_IOS_ACTION_ARRIVED\n");
            break;

        case USBCOMMUNI_IOS_ACTION_REMOVE:
            doonce = true;
            ios_actions_ = USBCOMMUNI_IOS_ACTION_IDLE;
            ios_.HotplugEventDisregister();
            break;

        case USBCOMMUNI_IOS_ACTION_IDLE:
        default:
            break;
        }

        usleep(1000*500);
    }
}

}
