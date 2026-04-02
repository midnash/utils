#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace fs = std::filesystem;

static void print_help(const char *prog)
{
    std::cout
        << "Usage: " << prog << " VIDEO TIMESTAMP [OUTPUT]\n"
        << "\n"
        << "Extract a video frame at TIMESTAMP using FFmpeg C API.\n"
        << "\n"
        << "Examples:\n"
        << "  thumbgen video.mp4 00:01:30\n"
        << "  thumbgen clip.mkv 90 thumb.png\n";
}

static bool parse_timestamp(const std::string &s, double &seconds)
{
    try
    {
        std::size_t p1 = s.find(':');
        if (p1 == std::string::npos)
        {
            seconds = std::stod(s);
            return seconds >= 0.0;
        }

        std::size_t p2 = s.find(':', p1 + 1);
        if (p2 == std::string::npos)
        {
            double mm = std::stod(s.substr(0, p1));
            double ss = std::stod(s.substr(p1 + 1));
            seconds = mm * 60.0 + ss;
            return seconds >= 0.0;
        }

        double hh = std::stod(s.substr(0, p1));
        double mm = std::stod(s.substr(p1 + 1, p2 - p1 - 1));
        double ss = std::stod(s.substr(p2 + 1));
        seconds = hh * 3600.0 + mm * 60.0 + ss;
        return seconds >= 0.0;
    }
    catch (...)
    {
        return false;
    }
}

static fs::path default_output_for(const fs::path &video)
{
    fs::path out = video.stem();
    out += ".thumb.png";
    return out;
}

int main(int argc, char **argv)
{
    if (argc >= 2)
    {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            return 0;
        }
    }

    if (argc < 3 || argc > 4)
    {
        print_help(argv[0]);
        return 1;
    }

    fs::path input = argv[1];
    std::string ts = argv[2];
    fs::path output = (argc == 4) ? fs::path(argv[3]) : default_output_for(input);

    double target_seconds = 0.0;
    if (!parse_timestamp(ts, target_seconds))
    {
        std::cerr << "thumbgen: invalid timestamp: " << ts << "\n";
        return 1;
    }

    av_log_set_level(AV_LOG_ERROR);

    AVFormatContext *fmt = nullptr;
    AVCodecContext *dec_ctx = nullptr;
    AVCodecContext *png_ctx = nullptr;
    SwsContext *sws = nullptr;
    AVPacket *pkt = nullptr;
    AVPacket *out_pkt = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *rgb = nullptr;

    int ret = 0;

    if ((ret = avformat_open_input(&fmt, input.c_str(), nullptr, nullptr)) < 0)
    {
        std::cerr << "thumbgen: failed to open input\n";
        return 1;
    }
    if ((ret = avformat_find_stream_info(fmt, nullptr)) < 0)
    {
        std::cerr << "thumbgen: failed to read stream info\n";
        avformat_close_input(&fmt);
        return 1;
    }

    int vstream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (vstream < 0)
    {
        std::cerr << "thumbgen: no video stream found\n";
        avformat_close_input(&fmt);
        return 1;
    }

    AVStream *stream = fmt->streams[vstream];
    const AVCodec *decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder)
    {
        std::cerr << "thumbgen: no decoder found\n";
        avformat_close_input(&fmt);
        return 1;
    }

    dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx)
    {
        std::cerr << "thumbgen: decoder context alloc failed\n";
        avformat_close_input(&fmt);
        return 1;
    }

    if ((ret = avcodec_parameters_to_context(dec_ctx, stream->codecpar)) < 0 ||
        (ret = avcodec_open2(dec_ctx, decoder, nullptr)) < 0)
    {
        std::cerr << "thumbgen: decoder init failed\n";
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&fmt);
        return 1;
    }

    int64_t seek_ts = static_cast<int64_t>(target_seconds / av_q2d(stream->time_base));
    av_seek_frame(fmt, vstream, seek_ts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(dec_ctx);

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    rgb = av_frame_alloc();
    out_pkt = av_packet_alloc();
    if (!pkt || !frame || !rgb || !out_pkt)
    {
        std::cerr << "thumbgen: allocation failure\n";
        ret = -1;
        goto cleanup;
    }

    {
        int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, dec_ctx->width, dec_ctx->height, 1);
        if (num_bytes <= 0)
        {
            std::cerr << "thumbgen: invalid frame size\n";
            ret = -1;
            goto cleanup;
        }

        std::vector<uint8_t> tmp(static_cast<std::size_t>(num_bytes));
        ret = av_image_fill_arrays(rgb->data, rgb->linesize, tmp.data(), AV_PIX_FMT_RGB24, dec_ctx->width, dec_ctx->height, 1);
        if (ret < 0)
        {
            std::cerr << "thumbgen: image buffer setup failed\n";
            goto cleanup;
        }

        sws = sws_getContext(dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                             dec_ctx->width, dec_ctx->height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws)
        {
            std::cerr << "thumbgen: swscale init failed\n";
            ret = -1;
            goto cleanup;
        }

        bool got_frame = false;
        while (av_read_frame(fmt, pkt) >= 0)
        {
            if (pkt->stream_index != vstream)
            {
                av_packet_unref(pkt);
                continue;
            }

            if ((ret = avcodec_send_packet(dec_ctx, pkt)) < 0)
            {
                av_packet_unref(pkt);
                continue;
            }
            av_packet_unref(pkt);

            while ((ret = avcodec_receive_frame(dec_ctx, frame)) >= 0)
            {
                int64_t pts = (frame->best_effort_timestamp == AV_NOPTS_VALUE) ? 0 : frame->best_effort_timestamp;
                double t = pts * av_q2d(stream->time_base);
                if (t + 0.001 < target_seconds)
                {
                    av_frame_unref(frame);
                    continue;
                }

                sws_scale(sws, frame->data, frame->linesize, 0, dec_ctx->height, rgb->data, rgb->linesize);
                got_frame = true;
                av_frame_unref(frame);
                break;
            }

            if (got_frame)
            {
                break;
            }
        }

        if (!got_frame)
        {
            std::cerr << "thumbgen: could not decode frame at requested timestamp\n";
            ret = -1;
            goto cleanup;
        }

        const AVCodec *png = avcodec_find_encoder(AV_CODEC_ID_PNG);
        if (!png)
        {
            std::cerr << "thumbgen: PNG encoder unavailable\n";
            ret = -1;
            goto cleanup;
        }

        png_ctx = avcodec_alloc_context3(png);
        if (!png_ctx)
        {
            std::cerr << "thumbgen: PNG context alloc failed\n";
            ret = -1;
            goto cleanup;
        }

        png_ctx->width = dec_ctx->width;
        png_ctx->height = dec_ctx->height;
        png_ctx->pix_fmt = AV_PIX_FMT_RGB24;
        png_ctx->time_base = AVRational{1, 25};

        if ((ret = avcodec_open2(png_ctx, png, nullptr)) < 0)
        {
            std::cerr << "thumbgen: PNG encoder init failed\n";
            goto cleanup;
        }

        rgb->format = AV_PIX_FMT_RGB24;
        rgb->width = dec_ctx->width;
        rgb->height = dec_ctx->height;

        if ((ret = avcodec_send_frame(png_ctx, rgb)) < 0)
        {
            std::cerr << "thumbgen: PNG encode send failed\n";
            goto cleanup;
        }
        if ((ret = avcodec_receive_packet(png_ctx, out_pkt)) < 0)
        {
            std::cerr << "thumbgen: PNG encode receive failed\n";
            goto cleanup;
        }

        FILE *f = fopen(output.c_str(), "wb");
        if (!f)
        {
            std::cerr << "thumbgen: cannot open output file: " << output.string() << "\n";
            ret = -1;
            goto cleanup;
        }
        fwrite(out_pkt->data, 1, static_cast<std::size_t>(out_pkt->size), f);
        fclose(f);

        std::cout << output.string() << "\n";
        ret = 0;
    }

cleanup:
    av_packet_free(&out_pkt);
    av_frame_free(&rgb);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    sws_freeContext(sws);
    avcodec_free_context(&png_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt);

    return ret == 0 ? 0 : 1;
}
