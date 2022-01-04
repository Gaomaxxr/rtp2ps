#include<stdint.h>
#include<string>
#include<netinet/in.h>
#include"RtpReceiver.hpp"


class StreamClient : public RtpReceiver{


public:

    struct EthernetHeader
    {
        uint8_t dstmac[6]; //目标mac地址
        uint8_t srcmac[6]; //源mac地址
        uint16_t eth_type; //以太网类型
    };


    struct VLANHeader
    {
        uint8_t lan[4];
    };

    struct IPHeader
    {

    #if __BYTE_ORDER == __LITTLE_ENDIAN
        unsigned int ihl:4;     //首部的长度
        unsigned int version:4;     //协议版本号，4/6 ,表示IPV4/IPV6
    #elif __BYTE_ORDER == __BIG_ENDIAN
        unsigned int version:4;
        unsigned int ihl:4;
    #endif
        uint8_t tos;    //服务类型
        uint16_t tot_len; //总长度
        uint16_t id;     //标志
        uint16_t frag_off; //分片偏移
        uint8_t ttl;    //生存时间
        uint8_t protocol; //协议
        uint16_t chk_sum; //检验和
        struct in_addr srcaddr; //源IP地址
        struct in_addr dstaddr; //目的IP地址
    };

    struct UDPHeader
    {
        uint16_t src_port; //远端口号
        uint16_t dst_port; //目的端口号
        uint16_t uhl;    //udp头部长度
        uint16_t chk_sum; //16位udp检验和
    };

    static void read_file(std::string filename, char**msg, long *size);

    int on_stream(char* data, long size);

};
