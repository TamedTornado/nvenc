#pragma once

#include <stdint.h>
#include <vector>

struct AVCodec;
struct AVCodecContext;
struct SwsContext;
struct AVFrame;


class EncoderFFmpeg
{
public:
    EncoderFFmpeg(uint32_t width, uint32_t height, bool hevc, uint32_t bitrate);
    virtual ~EncoderFFmpeg();

    void Encode(const uint8_t* rgba, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer);

private:
    AVCodec* codec = nullptr;
    AVCodecContext* context = nullptr;
    SwsContext* conversionContext = nullptr;
    AVFrame* frame = nullptr;
    uint64_t frameNumber = 0;
};





