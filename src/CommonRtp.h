/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_COMMONRTP_H
#define ZLMEDIAKIT_COMMONRTP_H

#include "Frame.h"
#include "global.hpp"

using namespace mediakit;

namespace mediakit{

/**
 * 通用 rtp解码类
 */
class CommonRtpDecoder : public ResourcePoolHelper<FrameImp> {
public:
    typedef std::shared_ptr <CommonRtpDecoder> Ptr;

    ~CommonRtpDecoder() override {}

    /**
     * 构造函数
     * @param codec 编码id
     * @param max_frame_size 允许的最大帧大小
     */
    CommonRtpDecoder(CodecId codec, int max_frame_size = 2 * 1024);

    /**
     * 返回编码类型ID
     */
    CodecId getCodecId() const;

    /**
     * 输入rtp并解码
     * @param rtp rtp数据包
     * @param key_pos 此参数内部强制转换为false,请忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false);

private:
    void obtainFrame();

private:
    bool _drop_flag = false;
    uint16_t _last_seq = 0;
    int _max_frame_size;
    CodecId _codec;
    FrameImp::Ptr _frame;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_COMMONRTP_H
