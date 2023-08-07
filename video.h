/*

    flow: video.h

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

#ifndef _VIDEO_H
#define _VIDEO_H

#include "colour.h"

#include <stdbool.h>
#include <stdint.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libswscale/swscale.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

// Getting started from https://gist.github.com/armornick/3434362
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>


#define DEFAULT_IMAGE_WIDTH 1920
#define DEFAULT_IMAGE_HEIGHT 1080
#define VIDEO_FPS DEFAULT_FRAMES_PER_SECOND

enum VIDEO_ERR {
    VIDEO_OK = 0,
    VIDEO_OUTPUT_CONTEXT = -1,
    VIDEO_OUTPUT_STREAM = -2,
    VIDEO_NO_CODEC = -3,
    VIDEO_NO_PACKET = -4,
    VIDEO_NO_ENCODER_CONTEXT = -5,
    VIDEO_CODEC_OPEN = -6,
    VIDEO_NO_FRAME = -7,
    VIDEO_STREAM_PARAMETERS = -8,
    VIDEO_FRAME_SEND = -9,
    VIDEO_FRAME_ENCODE = -10,
    VIDEO_FRAME_WRITE = -11,
    VIDEO_FILTER = -12,
    VIDEO_ARG = -13,
    VIDEO_MEMORY = -14,
    VIDEO_MISSING_NOTES = -15,
    VIDEO_AUDIO_OPEN = -16,
    VIDEO_FORMAT = -17,
    VIDEO_AUDIO_DECODE = -18,
    VIDEO_AUDIO_TRANSCODE = -19,
    VIDEO_AUDIO_EOF = -20

};

typedef struct VideoState
{

    char *outputFilename;
    char *videoTitleText;
    char *videoTitleFont;
    RGBAColour videoTitleColour;
    double videoTitleDecayTime;
    int videoTitlefontSize;
    double frameRate;
    int frameWidth;
    int frameHeight;
    bool uhd;
    bool sd;

    AVFormatContext *videoContext;
    AVDictionary *dict;
    AVStream *videoStream;
    AVFrame *videoFrame;
    AVPacket *videoPacket;

    AVFrame *filterFrame;

    const AVCodec *videoCodec;
    AVCodecContext *videoCodecContext;

    struct SwsContext *colorConversionContext;
    uint32_t *frameBuffer;

    AVFilterContext *filterSourceContext;
    AVFilterContext *filterSinkContext;
    AVFilterGraph *filterGraph;

    char *videoFilterGraph;
    bool applyVideoFilter;

    // Draw info
    SDL_Window *window;
    SDL_Surface *surface;
	SDL_Renderer *renderer;
	SDL_Texture *videoTexture;
	int w, h; // texture width & height

    // For RGB to YUV conversion
    int in_linesize[1];

    bool fastRgb2Yuv;

    bool sdlRendering;

    bool noMoreFrames;

    bool verbose;

} VideoState;

int initVideoProcessor(VideoState *state);

void rgba2Yuv420p(uint8_t *destination[8], uint8_t *rgb, size_t width, size_t height);
static void rgbToYuv(VideoState *state);

int generateFrame(VideoState *state, int frameNumber);

int finishVideo(VideoState *state);
void cleanupVideo(VideoState *state);

int videoFilter(VideoState *state);

void hoursMinutesSeconds(double seconds, int *h, int *m, double *secs);

#endif // _VIDEO_H
