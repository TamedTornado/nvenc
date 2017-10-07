#ifdef USE_CPU_ENCODER

#include "EncoderFFMPEG.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

/**
 * @brief Constructor.
 * @param width
 * @param height
 * @param hevc
 * @param bitrate
 */
EncoderFFmpeg::EncoderFFmpeg(uint32_t width, uint32_t height, bool hevc, uint32_t bitrate)
{
    av_log_set_level(AV_LOG_QUIET);

    // Initialize codec
    avcodec_register_all();

    this->codec = avcodec_find_encoder(hevc ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264);
    if (!this->codec)
        throw std::runtime_error("Failed to initialize codec in FFmpeg");

    this->context = avcodec_alloc_context3(this->codec);
    if (!this->context)
        throw std::runtime_error("Failed to allocate video codec context in FFmpeg");


    this->context->width = width;
    this->context->height = height;
    this->context->bit_rate = bitrate;
	AVRational tb;
	tb.num = 1;
	tb.den = 90;
    this->context->time_base = tb;
	AVRational fr;
	fr.num = 90;
	fr.den = 1;
    this->context->framerate = fr;
    this->context->gop_size = 90;
    this->context->max_b_frames = 0;
    this->context->pix_fmt = AV_PIX_FMT_YUV420P;
    av_opt_set(this->context->priv_data, "preset", "ultrafast", 0);
    av_opt_set(this->context->priv_data, "tune", "zerolatency", 0);
    av_opt_set(this->context->priv_data, "x264opts","no-mbtree:sliced-threads:sync-lookahead=0", 0);
    av_opt_set(this->context->priv_data, "x265-params","log-level=error", 0);

    if (avcodec_open2(this->context, this->codec, NULL) < 0)
        throw std::runtime_error("Failed to open codec in FFmpeg");

    // Init conversion
    // TODO move resize handling to encode function
    this->conversionContext = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height, AV_PIX_FMT_YUV420P, 0, 0, 0, 0);

    this->frame = av_frame_alloc();
    this->frame->width = width;
    this->frame->height = height;
    this->frame->format = AV_PIX_FMT_YUV420P;

    if (av_frame_get_buffer(this->frame, 32) < 0)
        throw std::runtime_error("Failed to allocate YUV420P frame data in FFmpeg");

    av_frame_make_writable(this->frame);
}


/**
 * @brief Destructor.
 */
EncoderFFmpeg::~EncoderFFmpeg()
{
    if (this->context)
        avcodec_free_context(&this->context);

    if (this->conversionContext)
        sws_freeContext(this->conversionContext);

    if (this->frame)
        av_frame_free(&this->frame);
}


/**
 * @brief Encode a single frame.
 * @param rgba
 * @param width
 * @param height
 * @param iFrame
 * @param buffer
 */
void EncoderFFmpeg::Encode(const uint8_t* rgba, uint32_t width, uint32_t height, bool iFrame, std::vector<uint8_t>& buffer)
{
    // Convert input to YUV
    const uint8_t* inData[1] = { rgba };
    int inLinesize[1] = { 4 * (int) width };

    uint8_t* outData[3] = { this->frame->data[0], this->frame->data[1], this->frame->data[2] };
    int outLinesize[3] = { this->frame->linesize[0], this->frame->linesize[1], this->frame->linesize[2] };

    if (sws_scale(this->conversionContext, inData, inLinesize, 0, height, outData, outLinesize) < 0)
        throw std::runtime_error("Faild to convert RGBA to YUV in FFmpeg");


    // Encode
    if (iFrame)
        this->frame->pict_type = AV_PICTURE_TYPE_I;
    else
        this->frame->pict_type = AV_PICTURE_TYPE_NONE;

    frame->pts = this->frameNumber++;

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = NULL;    // packet data will be allocated by the encoder
    pkt.size = 0;

    int got_output;
    if (avcodec_encode_video2(this->context, &pkt, this->frame, &got_output) < 0)
        throw std::runtime_error("Faild to encode video frame in FFmpeg");

    if (got_output)
    {
        buffer.clear();
        buffer.reserve(pkt.size);
        buffer.insert(buffer.end(), pkt.data, pkt.data + pkt.size);
        av_packet_unref(&pkt);
    }
}

#endif
