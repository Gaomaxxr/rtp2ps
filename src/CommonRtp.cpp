/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "CommonRtp.h"

CommonRtpDecoder::CommonRtpDecoder(CodecId codec, int max_frame_size ){
    printf("CommonRtpDecoder\n");
    _codec = codec;
    _max_frame_size = max_frame_size;
    obtainFrame();
}

CodecId CommonRtpDecoder::getCodecId() const {
    return _codec;
}

void CommonRtpDecoder::obtainFrame() {
    printf("obtainFrame\n");
    _frame = ResourcePoolHelper<FrameImp>::obtainObj();
    _frame->_buffer.clear();
    _frame->_prefix_size = 0;
    _frame->_dts = 0;
    _frame->_codec_id = _codec;
}

bool CommonRtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool){
    printf("CommonRtpDecoder::inputRtp\n");
    auto payload = rtp->data() + rtp->offset;
    auto size = rtp->size() - rtp->offset;
    if (size <= 0) {
        //无实际负载
        return false;
    }

    std::shared_ptr<FILE> _save_file_ps;

    // InfoL << "rtp header: " << hexdump((uint8_t *) payload, 4) << endl;
    // InfoL << "rtp offset: " << rtp->offset << endl;
    // InfoL << "rtp->timeStamp: " << rtp->timeStamp << endl;
    // InfoL << "_frame->_dts: " << _frame->_dts << endl;
    if (_frame->_dts != rtp->timeStamp || _frame->_buffer.size() > _max_frame_size
        || (size > 4 && (uint8_t)payload[0] == 0x00 && (uint8_t)payload[1] == 0x00 && (uint8_t)payload[2] == 0x01 && (uint8_t)payload[3] == 0xba)) {
            printf("找到了ps头\n");
        //时间戳发生变化或者缓存超过MAX_FRAME_SIZE，则清空上帧数据
        // InfoL << "get frame ==== " << _frame->_buffer.size() << endl;
        if (!_frame->_buffer.empty()) {
            //有有效帧，则输出
            // RtpCodec::inputFrame(_frame);
            printf("写文件\n");
            FILE *fp = NULL;
 
            fp = fopen(outputfile.c_str(), "a+");
            fwrite((char *)_frame->data(), _frame->size(), 1, fp);
            fclose(fp);

        }

        //新的一帧数据
        obtainFrame();
        _frame->_dts = rtp->timeStamp;
        _drop_flag = false;
    } else if (_last_seq != 0 && (uint16_t)(_last_seq + 1) != rtp->sequence) {
        //时间戳未发生变化，但是seq却不连续，说明中间rtp丢包了，那么整帧应该废弃
        printf("rtp丢包:%d -> %d", _last_seq, rtp->sequence);
        _drop_flag = true;
        _frame->_buffer.clear();
    }

    if (!_drop_flag) {
        _frame->_buffer.append(payload, size);
    }

    _last_seq = rtp->sequence;
    return false;
}
