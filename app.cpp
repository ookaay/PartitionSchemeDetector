#include <iostream>
#include "libusb.h"
#include <iomanip>

int bulk_only_reset(libusb_device_handle *devHandle, int interface)
{

    int u = libusb_control_transfer(devHandle, 0b00100001, 0xff, 0, interface, NULL, 0, 1000);
    return u;
}
void reset_recovery(libusb_device_handle *devHandle, int interface, int in_Endpoint, int out_Endpoint)
{
    bulk_only_reset(devHandle, interface);
    libusb_clear_halt(devHandle, in_Endpoint);
    libusb_clear_halt(devHandle, out_Endpoint);
}

enum Command_list
{
    TRANSFER_READY,
    READ_CAP,
    READ_10_B1,
};
enum CWS_status_values
{
    Command_passed = 0x0,
    Command_failed = 0x01,
    Phase_error = 0x02,

};

int send_CBW(libusb_device_handle *devHandle, int out_Endpoint, u_int32_t tag, bool dataIN, Command_list command)
{

    unsigned char data_CBW[31] = {0};
    data_CBW[3] = 0x43;
    data_CBW[2] = 0x42;
    data_CBW[1] = 0x53;
    data_CBW[0] = 0x55;

    data_CBW[4] = (tag & 0xff000000) >> 24;
    data_CBW[5] = (tag & 0x00ff0000) >> 16;
    data_CBW[6] = (tag & 0x0000ff00) >> 8;
    data_CBW[7] = (tag & 0x000000ff);

    data_CBW[12] = 0b10000000 & (static_cast<int>(dataIN) << 7);

    int amTransfered = 0;

    switch (command)
    {

    case READ_CAP:
    {
        data_CBW[14] = 10;
        data_CBW[15] = 0x25;

        uint32_t data_len = 8;
        data_CBW[8] = (data_len >> 0) & 0xFF;
        data_CBW[9] = (data_len >> 8) & 0xFF;
        data_CBW[10] = (data_len >> 16) & 0xFF;
        data_CBW[11] = (data_len >> 24) & 0xFF;
    }
    break;
    case READ_10_B1:
    {
        data_CBW[14] = 10;
        data_CBW[15] = 0x28;
        data_CBW[23] = 0x01;

        uint32_t data_len = 512;
        data_CBW[8] = (data_len >> 0) & 0xFF;
        data_CBW[9] = (data_len >> 8) & 0xFF;
        data_CBW[10] = (data_len >> 16) & 0xFF;
        data_CBW[11] = (data_len >> 24) & 0xFF;
    }
    break;

    case TRANSFER_READY:
    default:
        data_CBW[14] = 6;
        break;
    }

    int s = libusb_bulk_transfer(devHandle, out_Endpoint, data_CBW, 31, &amTransfered, 1000);

    if ((s != 0))
    {
        return 1;
    }

    if (amTransfered != 31)
    {
        return 1;
    }

    return 0;
}

int get_CSW(libusb_device_handle *devHandle, int in_Endpoint, u_int32_t tag)
{
    unsigned char data_CSW[13] = {0};
    int amTransferedCSW = 0;

    int t = libusb_bulk_transfer(devHandle, in_Endpoint, data_CSW, 13, &amTransferedCSW, 1000);

    if (t != 0)
    {
        std::cout << libusb_error_name(t);
        std::cout << "a\n";
        return 1;
    }

    if ((data_CSW[0] != 0x55) || (data_CSW[1] != 0x53) || (data_CSW[2] != 0x42) || (data_CSW[3] != 0x53))
    {
        std::cout << "e\n";
        return 1;
    }

    if ((data_CSW[4] != (tag & 0xff000000) >> 24) ||
        (data_CSW[5] != (tag & 0x00ff0000) >> 16) ||
        (data_CSW[6] != (tag & 0x0000ff00) >> 8) ||
        (data_CSW[7] != (tag & 0x000000ff)))
    {
        std::cout << "b\n";
        return 1;
    }

    if ((data_CSW[12] != 0x00))
    {
        std::cout << "c\n";

        return 1;
    }

    if (amTransferedCSW != 13)
    {
        std::cout << "d\n";

        return 1;
    }

    return 0;
}

int main()
{
    int VID = 0x0;
    int PID = 0x0;
    int Config_num = 0;
    int Interface_num = 0;
    int Alt_setting = 0;
    int Endpoint_out = 0x00;
    int Endpoint_in = 0x00;
    int Timeout = 1000;
    const int ENABLE = 1;
    unsigned char data_PL[512] = {0};
    int dTransferred = 0;
    int r;
    int s;
    int t;
    int v;

    std::cout << "-------------------\n";
    std::cout << "STARTING\n";
    std::cout << "-------------------\n";

    libusb_context *ctx = nullptr;
    r = libusb_init(&ctx);

    if (r != 0)
    {
        std::cout << libusb_error_name(r) << '\n';
        return 1;
    }

    libusb_device **list;
    int dev_count = libusb_get_device_list(ctx, &list);

    std::cout << '\n';
    // std::cout << "Num of devices: " << dev_count << '\n';
    // std::cout << "--------------------\n";

    std::cout << "Which of these device do you want to use?" << "[1 - " << dev_count << "] \n";

    for (int i = 0; i < dev_count; i++)
    {
        libusb_device *p_device = list[i];
        libusb_device_descriptor p_dev_desc;

        r = libusb_get_device_descriptor(p_device, &p_dev_desc);

        if (r == 0)
        {
            std::cout << std::dec << i + 1 << ")  ";

            std::cout << std::hex;
            std::cout << "BUS: " << std::setw(3) << std::right << std::setfill('0') << (int)libusb_get_bus_number(p_device) << " PORT: " << std::setw(3) << std::right << std::setfill('0') << (int)libusb_get_port_number(p_device) << " ID " << std::setw(4) << std::right << std::setfill('0') << p_dev_desc.idVendor << ":" << std::setw(4) << std::right << std::setfill('0') << p_dev_desc.idProduct << '\n';
        }
    }
    // std::cout << "------------------\n";

    int dev_num = 0;
    std::cout << "Your choice: ";
    std::cin >> dev_num;

    libusb_device *p_device = list[dev_num - 1];
    libusb_device_descriptor p_dev_desc;
    r = libusb_get_device_descriptor(p_device, &p_dev_desc);

    VID = p_dev_desc.idVendor;
    PID = p_dev_desc.idProduct;

    libusb_free_device_list(list, 1);

    libusb_device_handle *stHandle = libusb_open_device_with_vid_pid(ctx, VID, PID);

    if (stHandle == NULL)
    {
        std::cout << r << "Unable to open device\n";
        return 1;
    }

    std::cout << '\n';
    std::cout << '\n';

    libusb_device_descriptor *dev_desc;
    libusb_device *stDev = libusb_get_device(stHandle);

    libusb_get_device_descriptor(stDev, dev_desc);

    u_int8_t flag_check = 0b00000000;

    for (int i = 0; i < dev_desc->bNumConfigurations; i++)
    {
        if ((flag_check & 0b00000100) == 0b00000100)
        {
            break;
        }

        libusb_config_descriptor *config_desc;
        libusb_get_config_descriptor(stDev, i, &config_desc);

        for (int j = 0; j < config_desc->bNumInterfaces; j++)
        {

            if ((flag_check & 0b00000100) == 0b00000100)
            {
                break;
            }

            libusb_interface interf = (config_desc->interface)[j];

            for (int k = 0; k < interf.num_altsetting; k++)
            {

                if ((flag_check & 0b00000100) == 0b00000100)
                {
                    break;
                }

                libusb_interface_descriptor inter_desc = interf.altsetting[k];

                if ((inter_desc.bInterfaceClass == 0x08) && (inter_desc.bInterfaceSubClass == 0x06) && (inter_desc.bInterfaceProtocol == 0x50))
                {

                    for (int l = 0; l < inter_desc.bNumEndpoints; l++)
                    {

                        if ((flag_check & 0b00000100) == 0b00000100)
                        {
                            break;
                        }

                        libusb_endpoint_descriptor endp_desc = inter_desc.endpoint[l];

                        if ((endp_desc.bmAttributes & 0x03) != 0x02)
                        {
                            continue;
                        }

                        if (((endp_desc.bEndpointAddress & LIBUSB_ENDPOINT_IN) == LIBUSB_ENDPOINT_IN))
                        {
                            Endpoint_in = endp_desc.bEndpointAddress;
                            flag_check = flag_check | 0b00000001;
                        }
                        else if ((endp_desc.bEndpointAddress & 0x80) == 0x00)
                        {
                            Endpoint_out = endp_desc.bEndpointAddress;
                            std::cout << Endpoint_out << '\n';
                            flag_check = flag_check | 0b00000010;
                        }
                    }

                    flag_check = (flag_check & 0b00000011) == 0b00000011 ? (flag_check | 0b00000100) : (flag_check & 0b11111100);

                    if ((flag_check & 0b00000100) == 0b00000100)
                    {
                        Interface_num = j;
                        Alt_setting = k;
                        Config_num = config_desc->bConfigurationValue;
                    }
                }
            }
        }
        libusb_free_config_descriptor(config_desc);
    }

    if ((flag_check & 0b00000100) != 0b00000100)
    {
        std::cout << "This device is not a Mass Storage, SCSI, Bulk only device\n";
        return 0;
    }

    std::cout << std::dec;
    std::cout << (int)Config_num << ' ' << (int)Endpoint_out << ' ' << (int)Endpoint_in << ' '<<  (int)flag_check << '\n';

    libusb_set_auto_detach_kernel_driver(stHandle, ENABLE);
    // if (libusb_kernel_driver_active(stHandle, Interface_num) == 1)
    // {
    //     r = libusb_detach_kernel_driver(stHandle, Interface_num);
    //     if (r < 0 && r != LIBUSB_ERROR_NOT_SUPPORTED)
    //     {
    //         std::cout << "Failed to detach kernel driver: " << libusb_error_name(r) << '\n';
    //         return 1;
    //     }
    // }
    // r = libusb_set_configuration(stHandle, Config_num);
    // if (r != 0)
    // {
    //     std::cout << libusb_error_name(r) << 'a' << '\n';
    //     return 1;
    // }

    r = libusb_claim_interface(stHandle, Interface_num);
    if (r != 0)
    {
        std::cout << libusb_error_name(r) << '\n';
        return 1;
    }

    // r = libusb_set_interface_alt_setting(stHandle, Interface_num, Alt_setting);
    // if (r != 0)
    // {
    //     std::cout << libusb_error_name(r) << '\n';
    //     return 1;
    // }

    reset_recovery(stHandle, Interface_num, Endpoint_in, Endpoint_out);
    std::cout << "Pinging device .....\n";
    s = send_CBW(stHandle, Endpoint_out, 121, 1, TRANSFER_READY);
    t = get_CSW(stHandle, Endpoint_in, 121);
    if (s | t)
    {
        std::cout << "Unable to ping device\n";
        return 1;
    }
    std::cout << "Ping Successful\n";
    std::cout << '\n';

    reset_recovery(stHandle, Interface_num, Endpoint_in, Endpoint_out);
    std::cout << "Getting device size ....\n";
    s = send_CBW(stHandle, Endpoint_out, 1231, 1, READ_CAP);
    v = libusb_bulk_transfer(stHandle, Endpoint_in, data_PL, 8, &dTransferred, Timeout);
    std::cout << libusb_error_name(v) << (int)dTransferred << '\n';
    t = get_CSW(stHandle, Endpoint_in, 1231);
    if (s | v | t)
    {
        std::cout << "Unable to get device size\n";
        return 1;
    }
    std::cout << std::dec;
    u_int64_t last_lba = (data_PL[0] << 24) | (data_PL[1] << 16) | (data_PL[2] << 8) | (data_PL[3] << 0);
    u_int64_t block_size = (data_PL[4] << 24) | (data_PL[5] << 16) | (data_PL[6] << 8) | (data_PL[7] << 0);
    // std::cout << std::hex;
    // std::cout << last_lba;
    std::cout << "DISK SIZE: " << (last_lba + 1) * block_size / (1024 * 1024 * 1024) << " GB" << '\n';
    std::cout << '\n';

    reset_recovery(stHandle, Interface_num, Endpoint_in, Endpoint_out);
    std::cout << "Getting partition scheme ....\n";
    s = send_CBW(stHandle, Endpoint_out, 123, 1, READ_10_B1);
    v = libusb_bulk_transfer(stHandle, Endpoint_in, data_PL, 512, &dTransferred, Timeout);
    t = get_CSW(stHandle, Endpoint_in, 123);
    if (s | v | t)
    {
        std::cout << "Unable to get partition scheme ....\n";
        return 1;
    }

    // std::cout << std::hex;
    // for (int i = 0; i < 512; i++)
    // {
    //     std::cout << (int)data_PL[i] << ' ';
    // }

    // std::cout << '\n';

    // std::cout << (int)data_PL[0x1ff] << '\n';
    // std::cout << std::dec;

    if ((data_PL[0x1fe] != 0x55) || (data_PL[0x1ff] != 0xaa))
    {
        std::cout << "Partition Scheme: Unknown\n";
    }
    else if (data_PL[0x1c2] == 0xee)
    {
        std::cout << "Partition Scheme: GPT\n";
    }
    else
    {
        std::cout << "Partition Scheme: MBR\n";
    }

    libusb_release_interface(stHandle, Interface_num);
    libusb_close(stHandle);
    libusb_exit(ctx);

    std::cout << "\n\n-------------------\n";
    std::cout << "EXITING\n";
    std::cout << "-------------------\n";

    return 0;
}