/*

    flow: video.c

    Copyright (C) 2023  Johnathan K Burchill

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "video.h"
#include "audio.h"
#include "midi.h"
#include "physics.h"
#include "colour.h"

#include <math.h>
#include <libavutil/pixdesc.h>

int initVideoProcessor(VideoState *state)
{
    int status = VIDEO_OK;

    if (state->sdlRendering)
    {
        status = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_AUDIO);
        if (status != 0)
        {
            if (state->verbose)
                fprintf(stderr, "SDL VIDEO not available. Using software rendering\n");
            state->sdlRendering = false;
        }
    }

    TTF_Init();


    state->frameBuffer = malloc(state->frameWidth * state->frameHeight * sizeof *state->frameBuffer);
    if (state->frameBuffer == NULL)
        return VIDEO_MEMORY;

    // Try to be quiet
    av_log_set_level(AV_LOG_FATAL);

    // Something to draw on
    state->surface = SDL_CreateRGBSurface(0, state->frameWidth, state->frameHeight, 32, 0, 0, 0, 255);
    if (state->sdlRendering)
    {
        state->window = SDL_CreateWindow(state->videoTitleText, 0, 0, state->frameWidth, state->frameHeight, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
        state->renderer = SDL_CreateRenderer(state->window, -1, SDL_RENDERER_ACCELERATED);
    }
    else
        state->renderer = SDL_CreateSoftwareRenderer(state->surface);
    state->videoTexture = SDL_CreateTexture(state->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, state->frameWidth, state->frameHeight);

    // Output video
    state->videoPacket = av_packet_alloc();

    av_dict_set(&state->dict, "profile", "baseline", 0);
    av_dict_set(&state->dict, "preset", "medium", 0);
    av_dict_set(&state->dict, "level", "3", 0);
    av_dict_set(&state->dict, "crf", "23", 0);
    av_dict_set(&state->dict, "tune", "grain", 0);
    av_dict_set(&state->dict, "loglevel", "quiet", 0);
    av_dict_set(&state->dict, "movflags", "faststart", 0);

    const AVOutputFormat *outputFormat = av_guess_format("mp4", NULL, NULL);
    if (outputFormat == NULL)
    {
        fprintf(stderr, "Could not set up MP4 format.\n");
        return VIDEO_FORMAT;
    }

    avformat_alloc_output_context2(&state->videoContext, NULL, NULL, state->outputFilename);
    if (!state->videoContext)
    {
        fprintf(stderr, "Problem initializing output context\n");
        return VIDEO_OUTPUT_CONTEXT;
    }

    // H264 and AAC
    state->videoContext->oformat = outputFormat;

    // Get the encoder
    state->videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!state->videoCodec)
    {
        fprintf(stderr, "Problem finding the H.264 videoCodec.\n");
        return VIDEO_NO_CODEC;
    }

    // Get a stream
    state->videoStream = avformat_new_stream(state->videoContext, NULL);
    if (!state->videoStream)
    {
        fprintf(stderr, "Problem initializing output video stream\n");
        return VIDEO_OUTPUT_STREAM;
    }
    state->videoStream->id = 0;

    // Get packet
    state->videoPacket = av_packet_alloc();
    if (!state->videoPacket)
    {
        fprintf(stderr, "Problem allocating packet.\n");
        return VIDEO_NO_PACKET;
    }

    state->videoCodecContext = avcodec_alloc_context3(state->videoCodec);
    if (!state->videoCodecContext)
    {
        fprintf(stderr, "Problem allocating encoder context.\n");
        return VIDEO_NO_ENCODER_CONTEXT;
    }
    state->videoCodecContext->codec_id = AV_CODEC_ID_H264;
    state->videoCodecContext->bit_rate = 1000000;
    state->videoCodecContext->width = state->frameWidth;
    state->videoCodecContext->height = state->frameHeight;
    state->videoStream->time_base = (AVRational){1, state->frameRate};
    state->videoCodecContext->time_base = state->videoStream->time_base;

    state->videoStream->avg_frame_rate = (AVRational){state->frameRate, 1};

    state->videoCodecContext->gop_size = 250;
    state->videoCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    if (state->videoContext->oformat->flags & AVFMT_GLOBALHEADER)
        state->videoCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    status = avcodec_open2(state->videoCodecContext, state->videoCodec, &state->dict);
    if (status < 0)
    {
        fprintf(stderr, "Problem opening videoCodec.\n");
        return VIDEO_CODEC_OPEN;
    }

    state->videoFrame = av_frame_alloc();
    if (!state->videoFrame)
    {
        fprintf(stderr, "Problem allocating video frame.\n");
        return VIDEO_NO_FRAME;
    }
    state->videoFrame->format = state->videoCodecContext->pix_fmt;
    state->videoFrame->width = state->videoCodecContext->width;
    state->videoFrame->height = state->videoCodecContext->height;
    status = av_frame_get_buffer(state->videoFrame, 0);
    if (status < 0)
    {
        fprintf(stderr, "Problem getting video frame buffer.\n");
        return status;
    }

    state->filterFrame = av_frame_alloc();
    if (!state->filterFrame)
    {
        fprintf(stderr, "Problem allocating filter frame.\n");
        return VIDEO_NO_FRAME;
    }
    state->filterFrame->format = state->videoCodecContext->pix_fmt;
    state->filterFrame->width = state->videoCodecContext->width;
    state->filterFrame->height = state->videoCodecContext->height;
    status = av_frame_get_buffer(state->filterFrame, 0);
    if (status < 0)
    {
        fprintf(stderr, "Problem getting filter frame buffer.\n");
        return status;
    }

    status = avcodec_parameters_from_context(state->videoStream->codecpar, state->videoCodecContext);
    if (status < 0)
    {
        fprintf(stderr, "Problem copying stream parameters.\n");
        return VIDEO_STREAM_PARAMETERS;
    }
    status = avio_open(&state->videoContext->pb, state->outputFilename, AVIO_FLAG_WRITE);
    if (status < 0)
    {
        fprintf(stderr, "Problem opening video file for writing\n");
        char err[255];
        av_strerror(status, err, 255);
        fprintf(stderr, "Message: %s\n", err);
        return status;
    }

    // Video filter setup
    if (state->applyVideoFilter)
    {
        status = videoFilter(state);
        if (status < 0)
        {
            fprintf(stderr, "Problem setting up video filter.\n");
            return status;
        }
    }

    // For RBG to YUV conversion
    state->in_linesize[0] = state->videoFrame->width * sizeof *state->frameBuffer;
    state->colorConversionContext = sws_getCachedContext(state->colorConversionContext, state->videoFrame->width, state->videoFrame->height, AV_PIX_FMT_RGBA, state->videoFrame->width, state->videoFrame->height, AV_PIX_FMT_YUV420P, 0, NULL, NULL, NULL);

    return VIDEO_OK;

}

// Based on https://stackoverflow.com/questions/9465815/rgb-to-yuv420-algorithm-efficiency
void rgba2Yuv420p(uint8_t *destination[8], uint8_t *rgb, size_t width, size_t height)
{
    size_t i = 0;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    size_t upos = 0;
    size_t vpos = 0;
    size_t imageSize = width * height;

    for( size_t line = 0; line < height; line+=2)
    {
        for( size_t x = 0; x < width; x += 2 )
        {
            r = rgb[4 * i];
            g = rgb[4 * i + 1];
            b = rgb[4 * i + 2];
            destination[1][upos++] = ((-38*r + -74*g + 112*b) >> 8) + 128;
            destination[2][vpos++] = ((112*r + -94*g + -18*b) >> 8) + 128;
            i+=2;
        }
        i += width;
    }
    for( size_t p = 0; p < imageSize; p++ )
    {
        r = rgb[4 * p];
        g = rgb[4 * p + 1];
        b = rgb[4 * p + 2];
        destination[0][p] = ((66*r + 129*g + 25*b) >> 8) + 16;
    }

    return;
}

// FFMPEG API version
static void rgbToYuv(VideoState *state)
{
    sws_scale(state->colorConversionContext, (const uint8_t * const *)&state->frameBuffer, state->in_linesize, 0, state->videoFrame->height, state->videoFrame->data, state->videoFrame->linesize);

    return;
}

int generateFrame(VideoState *state, int frameNumber)
{
    if (state == NULL)
        return VIDEO_ARG;

    int status = 0;
    int audioStatus = 0;
    int got_output = 0;
    int color = 0;

    if (!state->noMoreFrames)
    {
        SDL_RenderReadPixels(state->renderer, NULL, 0, state->frameBuffer, state->frameWidth * sizeof *state->frameBuffer);

        // Faster but lower quality
        if (state->fastRgb2Yuv)
            rgba2Yuv420p(state->videoFrame->data, (uint8_t*)state->frameBuffer, state->frameWidth, state->frameHeight);
        else
           rgbToYuv(state);        
        state->videoFrame->pts = frameNumber;

        // Filter the frame
        if (state->applyVideoFilter)
        {
            status = av_buffersrc_add_frame_flags(state->filterSourceContext, state->videoFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
            if (status < 0)
                return VIDEO_FILTER;

            while (1)
            {
                status = av_buffersink_get_frame(state->filterSinkContext, state->filterFrame);
                if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
                    goto frameSent;
                if (status < 0)
                    return VIDEO_FILTER;
                state->filterFrame->pts = frameNumber;
                status = avcodec_send_frame(state->videoCodecContext, state->filterFrame);
                av_frame_unref(state->filterFrame);
                goto frameSent;

            }
        }
        else
        {
            status = avcodec_send_frame(state->videoCodecContext, state->videoFrame);
        }
    }
    else 
    {
        status = avcodec_send_frame(state->videoCodecContext, NULL);
    }
frameSent:
    if (status < 0)
        return VIDEO_FRAME_SEND;

    while (status >= 0)
    {
        status = avcodec_receive_packet(state->videoCodecContext, state->videoPacket);
        if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
        {
            break;
        }
        else if (status < 0)
        {
            fprintf(stderr, "Problem encoding frame: %s\n", av_err2str(status));
            return VIDEO_FRAME_ENCODE;
        }
        av_packet_rescale_ts(state->videoPacket, state->videoCodecContext->time_base, state->videoStream->time_base);
        state->videoPacket->stream_index = state->videoStream->index;

        status = av_interleaved_write_frame(state->videoContext, state->videoPacket);
        if (status < 0)
        {
            fprintf(stderr, "Problem writing packet: %s\n", av_err2str(status));
            return VIDEO_FRAME_WRITE;
        }

    }
    av_packet_unref(state->videoPacket);

    return status;
}

int finishVideo(VideoState *state)
{
    fprintf(stdout, "\r                          \n");
    state->noMoreFrames = true;
    generateFrame(state, 0);
    av_write_trailer(state->videoContext);

    return VIDEO_OK;

}

void cleanupVideo(VideoState *state)
{
    // free memory, contexts, etc.
    avfilter_graph_free(&state->filterGraph);
    if (state->videoCodecContext)
        avcodec_free_context(&state->videoCodecContext);
    av_frame_free(&state->videoFrame);
    av_frame_free(&state->filterFrame);
    av_packet_free(&state->videoPacket);
    sws_freeContext(state->colorConversionContext);
    avio_closep(&state->videoContext->pb);
    avformat_free_context(state->videoContext);
    free(state->frameBuffer);

    // Seems to be the convention
    SDL_DestroyTexture(state->videoTexture);
    SDL_DestroyRenderer(state->renderer);
    SDL_FreeSurface(state->surface);
    SDL_DestroyWindow(state->window);


    return;
}

int videoFilter(VideoState *state)
{
    // Based on https://video.stackexchange.com/questions/27554/call-ffmpeg-filter-from-source-code-c-api
    // and https://ffmpeg.org/doxygen/trunk/decode_filter_video_8c-example.html

    int status = VIDEO_OK;

    char args[1024] = {0};

    const AVFilter *source = avfilter_get_by_name("buffer");;
    const AVFilter *sink = avfilter_get_by_name("buffersink");

    AVFilterInOut *in = avfilter_inout_alloc();
    AVFilterInOut *out = avfilter_inout_alloc();

    AVRational timebase = state->videoStream->time_base;

    state->filterGraph = avfilter_graph_alloc();

    snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d", state->videoCodecContext->width, state->videoCodecContext->height, state->videoCodecContext->pix_fmt, timebase.num, timebase.den, state->videoCodecContext->sample_aspect_ratio.num, state->videoCodecContext->sample_aspect_ratio.den);

    status = avfilter_graph_create_filter(&state->filterSourceContext, source, "in",
                                       args, NULL, state->filterGraph);
    if (status < 0)
    {
        fprintf(stderr, "%s\n", av_err2str(status));
        status = VIDEO_FILTER;
        goto cleanup;
    }

    status = avfilter_graph_create_filter(&state->filterSinkContext, sink, "out", NULL, NULL, state->filterGraph);
    if (status < 0)
    {
        status = VIDEO_FILTER;
        goto cleanup;
    }

    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };

    status = av_opt_set_int_list(state->filterSinkContext, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (status < 0)
    {
        status = VIDEO_FILTER;
        goto cleanup;
    }

    out->name       = av_strdup("in");
    out->filter_ctx = state->filterSourceContext;
    out->pad_idx    = 0;
    out->next       = NULL;

    in->name       = av_strdup("out");
    in->filter_ctx = state->filterSinkContext;
    in->pad_idx    = 0;
    in->next       = NULL;    

    status = avfilter_graph_parse_ptr(state->filterGraph, state->videoFilterGraph, &in, &out, NULL);
    if (status < 0)
    {
        status = VIDEO_FILTER;
        goto cleanup;
    }

    status = avfilter_graph_config(state->filterGraph, NULL);
    if (status < 0)
    {
        status = VIDEO_FILTER;
        goto cleanup;
    }

cleanup:
    avfilter_inout_free(&in);
    avfilter_inout_free(&out);

    return status;

}

void hoursMinutesSeconds(double seconds, int *h, int *m, double *secs)
{
    double secondsLeft = seconds;
    int hours = (int)floor(secondsLeft / 3600.);
    secondsLeft -= (double)(3600 * hours);
    int minutes = (int)floor(secondsLeft / 60.);
    secondsLeft -= (double)(60 * minutes);

    if (h != NULL)
        *h = hours;

    if (m != NULL)
        *m = minutes;

    if (secs != NULL)
        *secs = secondsLeft;

    return;
}