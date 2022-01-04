#include "RtpReceiver.hpp"


#define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])

#define RTP_MAX_SIZE (10 * 1024)

RtpReceiver::RtpReceiver() {
    int index = 0;
    for (auto &sortor : _rtp_sortor) {
        sortor.setOnSort([this, index](uint16_t seq, RtpPacket::Ptr &packet) {
            onRtpSorted(packet, index);
        });
        ++index;
    }
    _rtp_decoder = std::make_shared<CommonRtpDecoder>(CodecInvalid,  2 * 1024 * 1024);
}
RtpReceiver::~RtpReceiver() {}

void RtpReceiver::onRtpSorted(const RtpPacket::Ptr &rtp, int track_index) {

    _rtp_decoder->inputRtp(rtp);
    
}

bool RtpReceiver::handleOneRtp(int track_index, TrackType type, int samplerate, unsigned char *rtp_raw_ptr, unsigned int rtp_raw_len) {
    if (rtp_raw_len < 12) {
         printf("rtp包太小: %d\n", rtp_raw_len);
        return false;
    }

    printf("handleOneRtp\n");

    uint32_t version = rtp_raw_ptr[0] >> 6;
    uint8_t padding = 0;
    uint8_t ext = rtp_raw_ptr[0] & 0x10;
    uint8_t csrc = rtp_raw_ptr[0] & 0x0f;

    if (rtp_raw_ptr[0] & 0x20) {
        //获取padding大小
        padding = rtp_raw_ptr[rtp_raw_len - 1];
        //移除padding flag
        rtp_raw_ptr[0] &= ~0x20;
        //移除padding字节
        rtp_raw_len -= padding;
    }

    if (version != 2) {
        throw std::invalid_argument("非法的rtp，version != 2");
    }

    auto rtp_ptr = _rtp_pool.obtain();
    auto &rtp = *rtp_ptr;

    rtp.type = type;
    rtp.interleaved = 2 * type;
    rtp.mark = rtp_raw_ptr[1] >> 7;
    rtp.PT = rtp_raw_ptr[1] & 0x7F;

    //序列号,内存对齐
    memcpy(&rtp.sequence, rtp_raw_ptr + 2, 2);
    rtp.sequence = ntohs(rtp.sequence);

    //时间戳,内存对齐
    memcpy(&rtp.timeStamp, rtp_raw_ptr + 4, 4);
    rtp.timeStamp = ntohl(rtp.timeStamp);

    if (!samplerate) {
        //无法把时间戳转换成毫秒
        return false;
    }
    //时间戳转换成毫秒
    rtp.timeStamp = rtp.timeStamp * 1000LL / samplerate;

    //ssrc,内存对齐
    memcpy(&rtp.ssrc, rtp_raw_ptr + 8, 4);
    rtp.ssrc = ntohl(rtp.ssrc);



    //获取rtp中媒体数据偏移量
    rtp.offset = 12 + 4;
    rtp.offset += 4 * csrc;
    if (ext && rtp_raw_len >= rtp.offset) {
        /* calculate the header extension length (stored as number of 32-bit words) */
        ext = (AV_RB16(rtp_raw_ptr + rtp.offset - 2) + 1) << 2;
        rtp.offset += ext;
    }

    if (rtp_raw_len + 4 <= rtp.offset) {
        printf("无有效负载的rtp包:%d <= %d\n", rtp_raw_len, (int) rtp.offset);
        return false;
    }

    if (rtp_raw_len > RTP_MAX_SIZE) {
        printf("超大的rtp包::%d > %d\n", rtp_raw_len, RTP_MAX_SIZE);
        return false;
    }

    //设置rtp负载长度
    rtp.setCapacity(rtp_raw_len + 4);
    rtp.setSize(rtp_raw_len + 4);
    uint8_t *payload_ptr = (uint8_t *) rtp.data();
    payload_ptr[0] = '$';
    payload_ptr[1] = rtp.interleaved;
    payload_ptr[2] = rtp_raw_len >> 8;
    payload_ptr[3] = (rtp_raw_len & 0x00FF);
    //拷贝rtp负载
    memcpy(payload_ptr + 4, rtp_raw_ptr, rtp_raw_len);

    //排序rtp
    auto seq = rtp_ptr->sequence;
    _rtp_sortor[track_index].sortPacket(seq, std::move(rtp_ptr));
    return true;
}

void RtpReceiver::clear() {
    for (auto &sortor : _rtp_sortor) {
        sortor.clear();
    }
}

void RtpReceiver::setPoolSize(int size) {
    _rtp_pool.setSize(size);
}

int RtpReceiver::getJitterSize(int track_index){
    return _rtp_sortor[track_index].getJitterSize();
}

int RtpReceiver::getCycleCount(int track_index){
    return _rtp_sortor[track_index].getCycleCount();
}