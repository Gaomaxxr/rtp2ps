#include "stream.hpp"

void StreamClient::read_file(std::string filename, char**msg, long *size)
{
    char* text;
    FILE *pf = fopen(filename.c_str(),"r");
    fseek(pf,0,SEEK_END);
    long lSize = ftell(pf);
    text = (char*)malloc(lSize);
    rewind(pf); 
    fread(text,sizeof(char),lSize,pf);

    *size = lSize;
    *msg = text;
}

int StreamClient::on_stream(char* data, long size)
{

    struct EthernetHeader *eth;
    struct VLANHeader *vlh;
    struct IPHeader *iph;
    struct UDPHeader *udph;
    struct in_addr addr1, addr2;   
    long length = size;

    long ioc = 0;
    int rtplengthinudp;
    int rtplengthinip;
    char * rtp;
    ioc = ioc + 24;
    int ct;
    ct = 0;

 
    while (ioc <= length) {

        ct += 1;
        printf("count--->%d\n", ct);

        ioc += 16;
        printf("eth head data:%d, ioc:%d\n", data + (int)ioc, ioc);
        //接收到的数据帧头6字节是目的MAC地址，紧接着6字节是源MAC地址。
        eth = (struct EthernetHeader*)(data + (int)ioc);
        printf("Dest MAC addr:%02x:%02x:%02x:%02x:%02x:%02x\n", eth->dstmac[0], eth->dstmac[1], eth->dstmac[2], eth->dstmac[3], eth->dstmac[4], eth->dstmac[5]);
        printf("Source MAC addr:%02x:%02x:%02x:%02x:%02x:%02x\n", eth->srcmac[0], eth->srcmac[1], eth->srcmac[2], eth->srcmac[3], eth->srcmac[4], eth->srcmac[5]);
 

        ioc += sizeof(struct EthernetHeader);
        printf("vln head data:%d, ioc:%d\n", data + (int)ioc, ioc);
        vlh = (struct VLANHeader*)(data + (int)ioc);
        printf("VLANHeader:%02x:%02x:%02x:%02x\n", vlh->lan[0], vlh->lan[1], vlh->lan[2], vlh->lan[3]);

        ioc += sizeof(struct VLANHeader);

        // ioc += sizeof(struct EthernetHeader);
        printf("ip head data:%d, ioc:%d\n", data + (int)ioc, ioc);
        iph = (struct IPHeader*)(data + (int)ioc);
        rtplengthinip = (int)ntohs(iph->tot_len) - 20 - 8;
        printf("IPHeader total lentgh:%d\n", iph->tot_len);
        printf("data:%02x %02x \n", data[ioc], data[ioc+1]);

        ioc += sizeof(struct IPHeader);
        printf("udp head data:%d, ioc:%d\n", data + (int)ioc, ioc);
        udph = (struct UDPHeader*)(data + (int)ioc);
        printf("data:%x %x %x %x\n", ntohs(udph->src_port), udph->dst_port, udph->uhl, udph->chk_sum);
        printf("data:%x %x %x %x\n", data[ioc], data[ioc+1], data[ioc+2], data[ioc+3]);


        rtplengthinudp = (int)ntohs(udph->uhl) - 8;
        printf("rtplength:%d\n", rtplengthinudp);

        ioc += sizeof(struct UDPHeader);
        rtp = data + (int)ioc;
        if(rtplengthinudp < 18) {
            ioc += 18;
        } else {
            ioc += rtplengthinudp;
        }

        handleOneRtp(0, TrackVideo, 90000, (unsigned char *) rtp, rtplengthinudp);
        printf("***********************\n");

    }

}