#include "Frame.h"
#include <memory>
#include <bits/shared_ptr.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <stdexcept>
#include "ResourcePool.h"
#include "CommonRtp.h"



using namespace std;
using namespace mediakit;
using namespace toolkit;
namespace mediakit{

template<typename T, typename SEQ = uint16_t, uint32_t kMax = 256, uint32_t kMin = 10>
class PacketSortor {
public:
    PacketSortor() = default;
    ~PacketSortor() = default;

    void setOnSort(function<void(SEQ seq, T &packet)> cb) {
        printf("setOnSort\n");
        _cb = std::move(cb);
    }

    /**
     * 清空状态
     */
    void clear() {
        _seq_cycle_count = 0;
        _rtp_sort_cache_map.clear();
        _next_seq_out = 0;
        _max_sort_size = kMin;
    }

    /**
     * 获取排序缓存长度
     */
    int getJitterSize() {
        return _rtp_sort_cache_map.size();
    }

    /**
     * 获取seq回环次数
     */
    int getCycleCount() {
        return _seq_cycle_count;
    }

    /**
     * 输入并排序
     * @param seq 序列号
     * @param packet 包负载
     */
    void sortPacket(SEQ seq, T packet) {
        printf("sortPacket   seq : %d\n", seq);
        if (seq < _next_seq_out) {
            if (_next_seq_out - seq < kMax) {
                //过滤seq回退包(回环包除外)
                return;
            }
        } else if (_next_seq_out && seq - _next_seq_out > (0xFFFF >> 1)) {
            //过滤seq跳变非常大的包(防止回环时乱序时收到非常大的seq)
            return;
        }

        //放入排序缓存，使用map来排序
        _rtp_sort_cache_map.emplace(seq, std::move(packet)); 
        //尝试输出排序后的包
        tryPopPacket();
    }

    void flush(){
        printf("flush\n");
        //清空缓存
        while (!_rtp_sort_cache_map.empty()) {
            popIterator(_rtp_sort_cache_map.begin());
        }
    }

private:
    void popPacket() {
        printf("realpopPacket\n");
        auto it = _rtp_sort_cache_map.begin();
        if (it->first >= _next_seq_out) {
            //过滤回跳包
            popIterator(it);
            return;
        }

        if (_next_seq_out - it->first > (0xFFFF >> 1)) {
            //产生回环了
            if (_rtp_sort_cache_map.size() < 2 * kMin) {
                //等足够多的数据后才处理回环, 因为后面还可能出现大的SEQ
                return;
            }
            ++_seq_cycle_count;
            //找到大的SEQ并清空掉，然后从小的SEQ重新开始排序
            auto hit = _rtp_sort_cache_map.upper_bound((SEQ) (_next_seq_out - _rtp_sort_cache_map.size()));
            while (hit != _rtp_sort_cache_map.end()) {
                //回环前，清空剩余的大的SEQ的数据
                _cb(hit->first, hit->second);
                hit = _rtp_sort_cache_map.erase(hit);
            }
            //下一个回环的数据
            popIterator(_rtp_sort_cache_map.begin());
            return;
        }
        //删除回跳的数据包
        _rtp_sort_cache_map.erase(it);
    }

    void popIterator(typename map<SEQ, T>::iterator it) {
        printf("popIterator\n");
        _cb(it->first, it->second);
        _next_seq_out = it->first + 1;
        _rtp_sort_cache_map.erase(it);
    }

    void tryPopPacket() {
        printf("tryPopPacket\n");
        int count = 0;
        while ((!_rtp_sort_cache_map.empty() && _rtp_sort_cache_map.begin()->first == _next_seq_out)) {
            //找到下个包，直接输出
            popPacket();
            ++count;
        }

        if (count) {
            setSortSize();
        } else if (_rtp_sort_cache_map.size() > _max_sort_size) {
            //排序缓存溢出，不再继续排序
            popPacket();
            setSortSize();
        }
    }

    void setSortSize() {
        printf("setSortSize\n");
        _max_sort_size = kMin + _rtp_sort_cache_map.size();
        if (_max_sort_size > kMax) {
            _max_sort_size = kMax;
        }
    }

private:
    //下次应该输出的SEQ
    SEQ _next_seq_out = 0;
    //seq回环次数计数
    uint32_t _seq_cycle_count = 0;
    //排序缓存长度
    uint32_t _max_sort_size = kMin;  // 默认为10
    //rtp排序缓存，使用map，根据seq排序
    map<SEQ, T> _rtp_sort_cache_map;
    //回调
    function<void(SEQ seq, T &packet)> _cb;
};

class RtpReceiver {
public:
    RtpReceiver();
    virtual ~RtpReceiver();

protected:
    /**
     * 输入数据指针生成并排序rtp包
     * @param track_index track下标索引
     * @param type track类型
     * @param samplerate rtp时间戳基准时钟，视频为90000，音频为采样率
     * @param rtp_raw_ptr rtp数据指针
     * @param rtp_raw_len rtp数据指针长度
     * @return 解析成功返回true
     */
    bool handleOneRtp(int track_index, TrackType type, int samplerate, unsigned char *rtp_raw_ptr, unsigned int rtp_raw_len);

    /**
     * rtp数据包排序后输出
     * @param rtp rtp数据包
     * @param track_index track索引
     */
    void onRtpSorted(const RtpPacket::Ptr &rtp, int track_index);

    void clear();
    void setPoolSize(int size);
    int getJitterSize(int track_index);
    int getCycleCount(int track_index);

private:
    uint32_t _ssrc[2] = {0, 0};
    //ssrc不匹配计数
    uint32_t _ssrc_err_count[2] = {0, 0};
    //rtp排序缓存，根据seq排序
    PacketSortor<RtpPacket::Ptr> _rtp_sortor[2];
    //rtp循环池
    ResourcePool<RtpPacket> _rtp_pool;
    std::shared_ptr<CommonRtpDecoder> _rtp_decoder;
};
}