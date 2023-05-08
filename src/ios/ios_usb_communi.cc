#include "ios_usb_communi.h"
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>

namespace usbcommuni {

enum EeventSignalTypes {
    ESIG_NONE = 0,
    ESIG_SEND_USERDATA = 1,
    ESIG_DEVICE_REMOVE = 2
};

struct MsgData {
    char *payload;
    uint32_t length;
};

static USBCommuniErrors_t EventSignalSend(int efd, enum EeventSignalTypes val);
static uint32_t PeertalkProtocolHeadPacket(char *msg, const char *user_data, uint32_t len);

USBIosCommuni::USBIosCommuni(uint16_t port)
{
    port_ = port;
    efd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    queue_ = new cclqueue::BlockingQueue(QUEUE_DEFAULT_SIZE);
    sendbuffer = static_cast<char*>(malloc(SENDBUFFER_SIZE));

    device_ = nullptr;
    found_device_ = false;
    event_handle_ = nullptr;
    connect_status_ = false;
}

USBIosCommuni::~USBIosCommuni()
{
    if (queue_) {
        queue_->Free();
        delete queue_;
    }

    if (sendbuffer)
        free(sendbuffer);

    if (efd_ > 0)
        close(efd_);
}

USBCommuniErrors_t USBIosCommuni::Init()
{
    if (efd_ < 0) {
        if ((efd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) == -1)
            return USBCOMMUNI_E_IO;
    }

    if (queue_ == nullptr) {
        if ((queue_ = new cclqueue::BlockingQueue(QUEUE_DEFAULT_SIZE)) == nullptr)
            return USBCOMMUNI_E_IO;
    }

    if (sendbuffer == nullptr) {
        if ((sendbuffer = static_cast<char*>(malloc(SENDBUFFER_SIZE))) == nullptr)
            return USBCOMMUNI_E_IO;
    }

    return HotplugEventRegister();
}

static void idevice_event_handle(const idevice_event_t *event, void *user_data)
{
    idevice_error_t err;
    USBIosCommuni *ios = (USBIosCommuni *)user_data;

    fprintf(stderr, "[USB IOS][Event] Occur !\n");
    fprintf(stderr, "   conn_type   : %d\n", event->conn_type);
    fprintf(stderr, "   event       : %d\n", event->event);
    fprintf(stderr, "   udid        : %s\n", event->udid);

    switch (event->event) {
    case IDEVICE_DEVICE_ADD:
        err = idevice_new_with_options(&ios->device_, nullptr, IDEVICE_LOOKUP_USBMUX);
        if (err != IDEVICE_E_SUCCESS) {
            fprintf(stderr, "[USB IOS][ERROR]: No device found!\n");
            return;
        }

        ios->udid_ = event->udid;
        ios->found_device_ = true;

        ios->recv_thread_ = std::thread(&USBIosCommuni::_RecvThreadHandler, ios);
        ios->recv_thread_.detach();

        ios->send_thread_ = std::thread(&USBIosCommuni::_SendThreadHandler, ios);
        ios->send_thread_.detach();
        break;

    case IDEVICE_DEVICE_REMOVE:
        ios->found_device_ = false;
        EventSignalSend(ios->efd_, ESIG_DEVICE_REMOVE);
        break;

    case IDEVICE_DEVICE_PAIRED:
    default:
        break;
    }

    if (ios->event_handle_) {
        USBCommuniEventTypes_t event_type = (USBCommuniEventTypes_t)event->event;
        ios->event_handle_(event_type);
    }
}

USBCommuniErrors_t USBIosCommuni::HotplugEventRegister()
{
    idevice_error_t err;

    err = idevice_event_subscribe(idevice_event_handle, this);
    if (err != IDEVICE_E_SUCCESS) {
        fprintf(stderr, "[USB IOS][ERROR]: idevice_event_subscribe failed!\n");
        return USBCOMMUNI_E_IO;
    }

    return USBCOMMUNI_E_SUCCESS;
}

void USBIosCommuni::HotplugEventDisregister()
{
    idevice_event_unsubscribe();
}

void USBIosCommuni::SubscribeRegister(USBCommuniEventCb eventcb)
{
    event_handle_ = eventcb;
}

bool USBIosCommuni::GetConnectStatus()
{
    return connect_status_;
}

const std::string &USBIosCommuni::GetUdid()
{
    return udid_;
}

bool USBIosCommuni::IsFoundUSBDevice()
{
    return found_device_;
}

void USBIosCommuni::RecvHandleRegister(USBCommuniRecvHandleCb recvcb)
{
    recv_handle_ = recvcb;
}

USBCommuniErrors_t USBIosCommuni::SendData(const char *data, uint32_t data_size, uint32_t &send_bytes)
{
    USBCommuniErrors_t err;
    struct MsgData* msgdat;

    if ((data == nullptr) || (data_size == 0) || (data_size > SENDBUFFER_SIZE))
        return USBCOMMUNI_E_INVAIL_ARG;

    if ((connection_ == nullptr) || (connect_status_ == false))
        return USBCOMMUNI_E_INVAIL_ARG;

    msgdat = static_cast<struct MsgData*>(malloc(sizeof(struct MsgData)));
    if (msgdat == nullptr)
        return USBCOMMUNI_E_NMEN;

    msgdat->payload = static_cast<char*>(malloc(data_size));
    if (msgdat->payload == nullptr) {
        err = USBCOMMUNI_E_NMEN;
        goto error0;
    }

    memmove(msgdat->payload, data, data_size);
    msgdat->length = data_size;

    if (queue_->Offer(msgdat) != true) {
        err = USBCOMMUNI_E_IO;
        goto error1;
    }

    EventSignalSend(efd_, ESIG_SEND_USERDATA);

    send_bytes = data_size;

    return USBCOMMUNI_E_SUCCESS;

error1:
    free(msgdat->payload);
error0:
    free(msgdat);
    return err;
}

void USBIosCommuni::_SendThreadHandler()
{
#define EPOLL_EVENT_MAXNUM 5

    idevice_error_t err;
    uint32_t size;
    uint32_t sendbytes;
    struct MsgData* msgdat;

    int epollfd;
    int nfds;
    struct epoll_event ev, events[EPOLL_EVENT_MAXNUM];
    uint64_t u;

    if ((efd_ < 0) || (queue_ == nullptr))
        return;

    epollfd = epoll_create(EPOLL_EVENT_MAXNUM);
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = efd_;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, efd_, &ev) != 0)
        return;

    while (true) {
        if (found_device_ == false) {
            queue_->Clear();
            break;
        }

        nfds = epoll_wait(epollfd, events, EPOLL_EVENT_MAXNUM, 500);

        for (size_t i = 0; i < nfds; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                close(events[i].data.fd);
                continue;
            } else {
                if(events[i].data.fd == efd_) {
                    u = ESIG_NONE;
                    read(events[i].data.fd, &u, sizeof(u));
                    if (u == ESIG_DEVICE_REMOVE)
                        break;

                    do {
                        msgdat = static_cast<struct MsgData*>(queue_->Poll());

                        if (msgdat) {
                            size = PeertalkProtocolHeadPacket(sendbuffer, msgdat->payload, msgdat->length);
                            err = idevice_connection_send(connection_, sendbuffer, size, &sendbytes);
                            if (err == IDEVICE_E_SUCCESS) {
                            } else {
                                fprintf(stderr, "idevice_connection_send error !\n");
                            }

                            if (msgdat->payload)
                                free(msgdat->payload);
                            free(msgdat);
                        }
                    } while (msgdat);   
                }
            }
        }
    }
}

void USBIosCommuni::_RecvThreadHandler()
{
    idevice_error_t err;
    char recv_buffer[RECVBUFFER_SIZE];
    uint32_t recv_bytes;

    while (true) {
        if (found_device_ == false) {
            if (connect_status_) {
                idevice_disconnect(connection_);
                connection_ = nullptr;
            }
            connect_status_ = false;
            break;
        }   

        if (connect_status_ == false) {
            err = idevice_connect(device_, port_, &connection_);
            if (err != IDEVICE_E_SUCCESS) {
                fprintf(stderr, "[USB IOS][ERROR]: Device connect failed!\n");
            } else {
                connect_status_ = true;
            }
        }

        if (connect_status_ == false) {
            usleep(1000000);
            continue;
        }

        bzero(recv_buffer, sizeof(recv_buffer));
        err = idevice_connection_receive_timeout(connection_, 
                                                 recv_buffer, 
                                                 sizeof(recv_buffer), 
                                                 &recv_bytes, 
                                                 1000);
        switch (err) {
        case IDEVICE_E_SUCCESS:
            if (recv_handle_)
                recv_handle_(recv_buffer, recv_bytes);
            break;
        
        case IDEVICE_E_UNKNOWN_ERROR:
            connect_status_ = false;
            idevice_disconnect(connection_);
            connection_ = nullptr;
            break;

        case IDEVICE_E_TIMEOUT:
        default:
            break;
        }
    }

    if (device_) {
        idevice_free(device_);
        device_ = nullptr;
    }
}

static USBCommuniErrors_t EventSignalSend(int efd, enum EeventSignalTypes val)
{
    int r;
    uint64_t u;

    u = val;

    if (efd < 0)
        return USBCOMMUNI_E_INVAIL_ARG;

    do 
        r = write(efd, &u, sizeof(u));
    while (r == -1);

    if (r != sizeof(u))
        return USBCOMMUNI_E_IO;
    
    return USBCOMMUNI_E_SUCCESS;
}

static uint32_t PeertalkProtocolHeadPacket(char *msg, const char *user_data, uint32_t len)
{
    uint32_t payload_size;
    const uint32_t kProtocolVersion = 1;
    const uint32_t kFrameType = 101;
    const uint32_t kFrameFlag = 0;

    if ((msg == nullptr) || (user_data == nullptr) || (len == 0))
        return 0;

    payload_size = len + sizeof(uint32_t);

    msg[0]  = (kProtocolVersion >> 24u);
    msg[1]  = (kProtocolVersion >> 16u);
    msg[2]  = (kProtocolVersion >> 8u);
    msg[3]  = (kProtocolVersion & 0xFFu);
    msg[4]  = (kFrameType >> 24u);
    msg[5]  = (kFrameType >> 16u);
    msg[6]  = (kFrameType >> 8u);
    msg[7]  = (kFrameType & 0xFFu);
    msg[8]  = (kFrameFlag >> 24u);
    msg[9]  = (kFrameFlag >> 16u);
    msg[10] = (kFrameFlag >> 8u);
    msg[11] = (kFrameFlag & 0xFFu);
    msg[12] = (payload_size >> 24u);
    msg[13] = (payload_size >> 16u);
    msg[14] = (payload_size >> 8u);
    msg[15] = (payload_size & 0xFFu);
    msg[16] = (len >> 24u);
    msg[17] = (len >> 16u);
    msg[18] = (len >> 8u);
    msg[19] = (len & 0xFFu);

    memmove(msg+20, user_data, len);

    return (20 + len);
}

}
