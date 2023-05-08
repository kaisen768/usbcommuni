#include <unistd.h>
#include <iostream>
#include "usbcommuni.h"

int main(int argc, char const *argv[])
{
    std::cout << "USB Communication Module example 1.0.0" << std::endl;

    usbcommuni::USBCommuniErrors_t err;

    usbcommuni::USBCommuniRecvHandleCb recver = [](const char *data, uint32_t length) {
                                            fprintf(stderr, "[USB IOS][RECV][%d]: %s\n", length, data);
                                        };

    usbcommuni::USBCommuni usbm;

    usbm.Init();
    usbm.RecvHandleRegister(recver);

    int i = 0 ;
    bool connect_status = false;

    while (1) {
        sleep(1);

        if (connect_status != usbm.GetConnectStatus()) {
            connect_status = usbm.GetConnectStatus();
            fprintf(stderr, "USB Device connect status : %d\n", connect_status);
        }

#if 1
        if (usbm.GetConnectStatus()) {
            char data[256] = {0};
            uint32_t len;
            uint32_t send_bytes;

            len = sprintf(data, "Linux Message : %d\n", i++);
            err = usbm.SendData(data, len, send_bytes);
            fprintf(stderr, "SendData err:%d, send_bytes:%d\n", err, send_bytes);
        }
#endif
    }

    return 0;
}


