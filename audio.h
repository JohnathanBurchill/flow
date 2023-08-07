#ifndef _FLOW_AUDIO_H
#define _FLOW_AUDIO_H

#include <stdbool.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>


enum AUDIO_ERR {
    AUDIO_OK = 0,
    AUDIO_OUTPUT_CONTEXT = -1,
    AUDIO_OUTPUT_STREAM = -2,
    AUDIO_NO_CODEC = -3,
    AUDIO_NO_PACKET = -4,
    AUDIO_NO_ENCODER_CONTEXT = -5,
    AUDIO_CODEC_OPEN = -6,
    AUDIO_NO_FRAME = -7,
    AUDIO_STREAM_PARAMETERS = -8,
    AUDIO_FRAME_SEND = -9,
    AUDIO_FRAME_ENCODE = -10,
    AUDIO_FRAME_WRITE = -11,
    AUDIO_FILTER = -12,
    AUDIO_ARG = -13,
    AUDIO_MEMORY = -14,
    AUDIO_MISSING_NOTES = -15,
    AUDIO_OPEN = -16,
    AUDIO_FORMAT = -17,
    AUDIO_DECODE = -18,
    AUDIO_TRANSCODE = -19,
    AUDIO_EOF = -20,
    AUDIO_INIT = -21

};

typedef struct AudioState
{
    AVFormatContext *audioContext;
    AVPacket *audioPacket;
    AVFrame *audioFrame;
    SwrContext *swr;

    AVPacket *outAudioPacket;
    AVFrame *outAudioFrame;


    AVStream *audioStream;
    AVStream *outAudioStream;

    const AVCodec *audioDecoder;
    AVCodecContext *audioDecoderContext;

    const AVCodec *audioEncoder;
    AVCodecContext *audioEncoderContext;

    int audioStreamIndex;

    char *midiFilename;
    char *audioFilename;

    double startTime;
    double stopTime;

    bool haveAudio;
    bool bypassAudio;

    bool verbose;

} AudioState;

int initAudio(AudioState *state, AVFormatContext *videoContext);

int transcodeAudioFrames(AudioState *state, int frameCounter, double *elapsedAudioTime, AVFormatContext *videoContext, AVCodecContext *videoCodecContext);

int finishAudio(AudioState *state, AVFormatContext *videoContext, AVCodecContext *videoCodecContext);

void cleanupAudio(AudioState *state);


#endif // _FLOW_AUDIO_H
