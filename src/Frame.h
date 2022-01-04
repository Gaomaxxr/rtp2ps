/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FRAME_H
#define ZLMEDIAKIT_FRAME_H

#include <mutex>
#include <functional>
#include <memory>
#include <bits/shared_ptr.h>
#include <map>
#include <string.h>
#include "ResourcePool.h"

using namespace std;
using namespace toolkit;


namespace mediakit{

typedef enum {
    CodecInvalid = -1,
    CodecH264 = 0,
    CodecH265,
    CodecAAC,
    CodecG711A,
    CodecG711U,
    CodecOpus,
    CodecMax = 0x7FFF
} CodecId;

typedef enum {
    TrackInvalid = -1,
    TrackVideo = 0,
    TrackAudio,
    TrackTitle,
    TrackMax = 3
} TrackType;

/**
 * 获取编码器名称
 */
const char *getCodecName(CodecId codecId);

/**
 * 获取音视频类型
 */
TrackType getTrackType(CodecId codecId);

/**
 * 编码信息的抽象接口
 */
class CodecInfo {
public:
    typedef std::shared_ptr<CodecInfo> Ptr;

    CodecInfo(){}
    virtual ~CodecInfo(){}

    /**
     * 获取编解码器类型
     */
    virtual CodecId getCodecId() const = 0;

    /**
     * 获取编码器名称
     */
    const char *getCodecName();

    /**
     * 获取音视频类型
     */
    TrackType getTrackType();
};

//禁止拷贝基类
class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    //禁止拷贝
    noncopyable(const noncopyable &that) = delete;
    noncopyable(noncopyable &&that) = delete;
    noncopyable &operator=(const noncopyable &that) = delete;
    noncopyable &operator=(noncopyable &&that) = delete;
};

class Buffer : public noncopyable {
public:
    typedef std::shared_ptr<Buffer> Ptr;
    Buffer(){};
    virtual ~Buffer(){};
    //返回数据长度
    virtual char *data() const = 0 ;
    virtual uint32_t size() const = 0;

    virtual string toString() const {
        return string(data(),size());
    }

    virtual uint32_t getCapacity() const{
        return size();
    }
};
/**
 * 帧类型的抽象接口
 */
class Frame : public Buffer, public CodecInfo {
public:
    typedef std::shared_ptr<Frame> Ptr;
    virtual ~Frame(){}

    /**
     * 返回解码时间戳，单位毫秒
     */
    virtual uint32_t dts() const = 0;

    /**
     * 返回显示时间戳，单位毫秒
     */
    virtual uint32_t pts() const {
        return dts();
    }

    /**
     * 前缀长度，譬如264前缀为0x00 00 00 01,那么前缀长度就是4
     * aac前缀则为7个字节
     */
    virtual uint32_t prefixSize() const = 0;

    /**
     * 返回是否为关键帧
     */
    virtual bool keyFrame() const = 0;

    /**
     * 是否为配置帧，譬如sps pps vps
     */
    virtual bool configFrame() const = 0;

    /**
     * 是否可以缓存
     */
    virtual bool cacheAble() const { return true; }

    /**
     * 返回可缓存的frame
     */
    static Ptr getCacheAbleFrame(const Ptr &frame);
};
//指针式缓存对象，
class BufferRaw : public Buffer{
public:
    typedef std::shared_ptr<BufferRaw> Ptr;
    BufferRaw(uint32_t capacity = 0) {
        if(capacity){
            setCapacity(capacity);
        }
    }

    BufferRaw(const char *data,int size = 0){
        assign(data,size);
    }

    ~BufferRaw() {
        if(_data){
            delete [] _data;
        }
    }
    //在写入数据时请确保内存是否越界
    char *data() const override {
        return _data;
    }
    //有效数据大小
    uint32_t size() const override{
        return _size;
    }
    //分配内存大小
    void setCapacity(uint32_t capacity){
        if(_data){
            do{
                if(capacity > _capacity){
                    //请求的内存大于当前内存，那么重新分配
                    break;
                }

                if(_capacity < 2 * 1024){
                    //2K以下，不重复开辟内存，直接复用
                    return;
                }

                if(2 * capacity > _capacity){
                    //如果请求的内存大于当前内存的一半，那么也复用
                    return;
                }
            }while(false);

            delete [] _data;
        }
        _data = new char[capacity];
        _capacity = capacity;
    }
    //设置有效数据大小
    void setSize(uint32_t size){
        if(size > _capacity){
            throw std::invalid_argument("Buffer::setSize out of range");
        }
        _size = size;
    }
    //赋值数据
    void assign(const char *data,uint32_t size = 0){
        if(size <=0 ){
            size = strlen(data);
        }
        setCapacity(size + 1);
        memcpy(_data,data,size);
        _data[size] = '\0';
        setSize(size);
    }

    uint32_t getCapacity() const override{
        return _capacity;
    }
private:
    char *_data = nullptr;
    uint32_t _capacity = 0;
    uint32_t _size = 0;
};

class RtpPacket : public BufferRaw{
public:
    typedef std::shared_ptr<RtpPacket> Ptr;
    uint8_t interleaved;
    uint8_t PT;
    bool mark;
    //时间戳，单位毫秒
    uint32_t timeStamp;
    uint16_t sequence;
    uint32_t ssrc;
    uint32_t offset;
    TrackType type;
};

class BufferLikeString : public Buffer {
public:
    ~BufferLikeString() override {}

    BufferLikeString() {
        _erase_head = 0;
        _erase_tail  = 0;
    }

    BufferLikeString(string str) {
        _str = std::move(str);
        _erase_head = 0;
        _erase_tail  = 0;
    }

    BufferLikeString& operator= (string str){
        _str = std::move(str);
        _erase_head = 0;
        _erase_tail  = 0;
        return *this;
    }

    BufferLikeString(const char *str) {
        _str = str;
        _erase_head = 0;
        _erase_tail  = 0;
    }

    BufferLikeString& operator= (const char *str){
        _str = str;
        _erase_head = 0;
        _erase_tail  = 0;
        return *this;
    }

    BufferLikeString(BufferLikeString &&that) {
        _str = std::move(that._str);
        _erase_head = that._erase_head;
        _erase_tail = that._erase_tail;
        that._erase_head = 0;
        that._erase_tail = 0;
    }

    BufferLikeString& operator= (BufferLikeString &&that){
        _str = std::move(that._str);
        _erase_head = that._erase_head;
        _erase_tail = that._erase_tail;
        that._erase_head = 0;
        that._erase_tail = 0;
        return *this;
    }

    BufferLikeString(const BufferLikeString &that) {
        _str = that._str;
        _erase_head = that._erase_head;
        _erase_tail = that._erase_tail;
    }

    BufferLikeString& operator= (const BufferLikeString &that){
        _str = that._str;
        _erase_head = that._erase_head;
        _erase_tail = that._erase_tail;
        return *this;
    }

    char* data() const override{
        return (char *)_str.data() + _erase_head;
    }

    uint32_t size() const override{
        return _str.size() - _erase_tail - _erase_head;
    }

    BufferLikeString& erase(string::size_type pos = 0, string::size_type n = string::npos) {
        if (pos == 0) {
            //移除前面的数据
            if (n != string::npos) {
                //移除部分
                if (n > size()) {
                    //移除太多数据了
                    throw std::out_of_range("BufferLikeString::erase out_of_range in head");
                }
                //设置起始便宜量
                _erase_head += n;
                data()[size()] = '\0';
                return *this;
            }
            //移除全部数据
            _erase_head = 0;
            _erase_tail = _str.size();
            data()[0] = '\0';
            return *this;
        }

        if (n == string::npos || pos + n >= size()) {
            //移除末尾所有数据
            if (pos >= size()) {
                //移除太多数据
                throw std::out_of_range("BufferLikeString::erase out_of_range in tail");
            }
            _erase_tail += size() - pos;
            data()[size()] = '\0';
            return *this;
        }

        //移除中间的
        if (pos + n > size()) {
            //超过长度限制
            throw std::out_of_range("BufferLikeString::erase out_of_range in middle");
        }
        _str.erase(_erase_head + pos, n);
        return *this;
    }

    BufferLikeString& append(const BufferLikeString &str){
        return append(str.data(), str.size());
    }

    BufferLikeString& append(const string &str){
        return append(str.data(), str.size());
    }

    BufferLikeString& append(const char *data, int len = 0){
        if(_erase_head > _str.capacity() / 2){
            moveData();
        }
        if (_erase_tail == 0) {
            if (len <= 0) {
                _str.append(data);
            } else {
                _str.append(data, len);
            }
            return *this;
        }
        if (len <= 0) {
            _str.insert(_erase_head + size(), data);
        } else {
            _str.insert(_erase_head + size(), data, len);
        }
        return *this;
    }

    void push_back(char c){
        if(_erase_tail == 0){
            _str.push_back(c);
            return;
        }
        data()[size()] = c;
        --_erase_tail;
        data()[size()] = '\0';
    }

    BufferLikeString& insert(string::size_type pos, const char* s, string::size_type n){
        _str.insert(_erase_head + pos, s, n);
        return *this;
    }

    BufferLikeString& assign(const char *data, int len = 0) {
        if (data >= _str.data() && data < _str.data() + _str.size()) {
            _erase_head = data - _str.data();
            len = len ? len : strlen(data);
            if (data + len > _str.data() + _str.size()) {
                throw std::out_of_range("BufferLikeString::assign out_of_range");
            }
            _erase_tail = _str.data() + _str.size() - (data + len);
            return *this;
        }
        if (len <= 0) {
            _str.assign(data);
        } else {
            _str.assign(data, len);
        }
        _erase_head = 0;
        _erase_tail = 0;
        return *this;
    }

    void clear() {
        _erase_head = 0;
        _erase_tail = 0;
        _str.clear();
    }

    char& operator[](string::size_type pos){
        if (pos >= size()) {
            throw std::out_of_range("BufferLikeString::operator[] out_of_range");
        }
        return data()[pos];
    }

    const char& operator[](string::size_type pos) const{
        return (*const_cast<BufferLikeString *>(this))[pos];
    }

    string::size_type capacity() const{
        return _str.capacity();
    }

    void reserve(string::size_type size){
        _str.reserve(size);
    }

    bool empty() const{
        return size() <= 0;
    }

    string substr(string::size_type pos, string::size_type n = string::npos) const{
        if (n == string::npos) {
            //获取末尾所有的
            if (pos >= size()) {
                throw std::out_of_range("BufferLikeString::substr out_of_range");
            }
            return _str.substr(_erase_head + pos, size() - pos);
        }

        //获取部分
        if (pos + n > size()) {
            throw std::out_of_range("BufferLikeString::substr out_of_range");
        }
        return _str.substr(_erase_head + pos, n);
    }

private:
    void moveData(){
        if (_erase_head) {
            _str.erase(0, _erase_head);
            _erase_head = 0;
        }
    }

private:
    uint32_t _erase_head;
    uint32_t _erase_tail;
    string _str;
};

class FrameImp : public Frame {
public:
    typedef std::shared_ptr<FrameImp> Ptr;

    char *data() const override{
        return (char *)_buffer.data();
    }

    uint32_t size() const override {
        return _buffer.size();
    }

    uint32_t dts() const override {
        return _dts;
    }

    uint32_t pts() const override{
        return _pts ? _pts : _dts;
    }

    uint32_t prefixSize() const override{
        return _prefix_size;
    }

    CodecId getCodecId() const override{
        return _codec_id;
    }

    bool keyFrame() const override {
        return false;
    }

    bool configFrame() const override{
        return false;
    }

public:
    CodecId _codec_id = CodecInvalid;
    BufferLikeString _buffer;
    uint32_t _dts = 0;
    uint32_t _pts = 0;
    uint32_t _prefix_size = 0;
};

/**
 * 一个Frame类中可以有多个帧，他们通过 0x 00 00 01 分隔
 * ZLMediaKit会先把这种复合帧split成单个帧然后再处理
 * 一个复合帧可以通过无内存拷贝的方式切割成多个子Frame
 * 提供该类的目的是切割复合帧时防止内存拷贝，提高性能
 */
template<typename Parent>
class FrameInternal : public Parent{
public:
    typedef std::shared_ptr<FrameInternal> Ptr;
    FrameInternal(const Frame::Ptr &parent_frame, char *ptr, uint32_t size, int prefix_size)
            : Parent(ptr, size, parent_frame->dts(), parent_frame->pts(), prefix_size) {
        _parent_frame = parent_frame;
    }
    bool cacheAble() const override {
        return _parent_frame->cacheAble();
    }
private:
    Frame::Ptr _parent_frame;
};

/**
 * 循环池辅助类
 */
template <typename T>
class ResourcePoolHelper{
public:
    ResourcePoolHelper(int size = 8){
        _pool.setSize(size);
    }
    virtual ~ResourcePoolHelper(){}

    std::shared_ptr<T> obtainObj(){
        printf("obtainObj\n");
        return _pool.obtain();
    }
private:
    ResourcePool<T> _pool;
};

/**
 * 写帧接口的抽象接口类
 */
class FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameWriterInterface> Ptr;
    FrameWriterInterface(){}
    virtual ~FrameWriterInterface(){}

    /**
     * 写入帧数据
     */
    virtual void inputFrame(const Frame::Ptr &frame) = 0;
};

/**
 * 写帧接口转function，辅助类
 */
class FrameWriterInterfaceHelper : public FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameWriterInterfaceHelper> Ptr;
    typedef std::function<void(const Frame::Ptr &frame)> onWriteFrame;

    /**
     * inputFrame后触发onWriteFrame回调
     */
    FrameWriterInterfaceHelper(const onWriteFrame& cb){
        _writeCallback = cb;
    }

    virtual ~FrameWriterInterfaceHelper(){}

    /**
     * 写入帧数据
     */
    void inputFrame(const Frame::Ptr &frame) override {
        _writeCallback(frame);
    }
private:
    onWriteFrame _writeCallback;
};

/**
 * 支持代理转发的帧环形缓存
 */
class FrameDispatcher : public FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameDispatcher> Ptr;

    FrameDispatcher(){}
    virtual ~FrameDispatcher(){}

    /**
     * 添加代理
     */
    void addDelegate(const FrameWriterInterface::Ptr &delegate){
        //_delegates_write可能多线程同时操作
        lock_guard<mutex> lck(_mtx);
        _delegates_write.emplace(delegate.get(),delegate);
        _need_update = true;
    }

    /**
     * 删除代理
     */
    void delDelegate(FrameWriterInterface *ptr){
        //_delegates_write可能多线程同时操作
        lock_guard<mutex> lck(_mtx);
        _delegates_write.erase(ptr);
        _need_update = true;
    }

    /**
     * 写入帧并派发
     */
    void inputFrame(const Frame::Ptr &frame) override{
        if(_need_update){
            //发现代理列表发生变化了，这里同步一次
            lock_guard<mutex> lck(_mtx);
            _delegates_read = _delegates_write;
            _need_update = false;
        }

        //_delegates_read能确保是单线程操作的
        for(auto &pr : _delegates_read){
            pr.second->inputFrame(frame);
        }
    }

    /**
     * 返回代理个数
     */
    int size() const {
        return _delegates_write.size();
    }
private:
    mutex _mtx;
    map<void *,FrameWriterInterface::Ptr>  _delegates_read;
    map<void *,FrameWriterInterface::Ptr>  _delegates_write;
    bool _need_update = false;
};

/**
 * 通过Frame接口包装指针，方便使用者把自己的数据快速接入ZLMediaKit
 */
class FrameFromPtr : public Frame{
public:
    typedef std::shared_ptr<FrameFromPtr> Ptr;

    FrameFromPtr(CodecId codec_id, char *ptr, uint32_t size, uint32_t dts, uint32_t pts = 0, int prefix_size = 0)
            : FrameFromPtr(ptr, size, dts, pts, prefix_size) {
        _codec_id = codec_id;
    }

    FrameFromPtr(char *ptr, uint32_t size, uint32_t dts, uint32_t pts = 0, int prefix_size = 0){
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _pts = pts;
        _prefix_size = prefix_size;
    }

    char *data() const override{
        return _ptr;
    }

    uint32_t size() const override {
        return _size;
    }

    uint32_t dts() const override {
        return _dts;
    }

    uint32_t pts() const override{
        return _pts ? _pts : dts();
    }

    uint32_t prefixSize() const override{
        return _prefix_size;
    }

    bool cacheAble() const override {
        return false;
    }

    CodecId getCodecId() const override {
        if (_codec_id == CodecInvalid) {
            throw std::invalid_argument("FrameFromPtr对象未设置codec类型");
        }
        return _codec_id;
    }

    void setCodecId(CodecId codec_id) {
        _codec_id = codec_id;
    }

    bool keyFrame() const override {
        return false;
    }

    bool configFrame() const override{
        return false;
    }

protected:
    FrameFromPtr() {}

protected:
    char *_ptr;
    uint32_t _size;
    uint32_t _dts;
    uint32_t _pts = 0;
    uint32_t _prefix_size;
    CodecId _codec_id = CodecInvalid;
};

/**
 * 该对象可以把Buffer对象转换成可缓存的Frame对象
 */
template <typename Parent>
class FrameWrapper : public Parent{
public:
    ~FrameWrapper() = default;

    /**
     * 构造frame
     * @param buf 数据缓存
     * @param dts 解码时间戳
     * @param pts 显示时间戳
     * @param prefix 帧前缀长度
     * @param offset buffer有效数据偏移量
     */
    FrameWrapper(const Buffer::Ptr &buf, int64_t dts, int64_t pts, int prefix, int offset) : Parent(buf->data() + offset, buf->size() - offset, dts, pts, prefix){
        _buf = buf;
    }

    /**
     * 构造frame
     * @param buf 数据缓存
     * @param dts 解码时间戳
     * @param pts 显示时间戳
     * @param prefix 帧前缀长度
     * @param offset buffer有效数据偏移量
     * @param codec 帧类型
     */
    FrameWrapper(const Buffer::Ptr &buf, int64_t dts, int64_t pts, int prefix, int offset, CodecId codec) : Parent(codec, buf->data() + offset, buf->size() - offset, dts, pts, prefix){
        _buf = buf;
    }

    /**
     * 该帧可缓存
     */
    bool cacheAble() const override {
        return true;
    }

private:
    Buffer::Ptr _buf;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_FRAME_H