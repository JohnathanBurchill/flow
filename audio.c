#include "audio.h"
#include "flow.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>

int initAudio(AudioState *state, AVFormatContext *videoContext)
{
    if (state == NULL)
        return AUDIO_ARG;

    int status = AUDIO_OK;

    state->audioStreamIndex = -1;

    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;


    status = avformat_open_input(&state->audioContext, state->audioFilename, NULL, NULL);
    if (status != 0)
    {
        fprintf(stderr, "Problem opening audio file %s\n", state->audioFilename);
        return AUDIO_OPEN;
    }
    AVFormatContext *audioContext = state->audioContext;

    status = avformat_find_stream_info(audioContext, NULL);    
    if (status < 0)
    {
        fprintf(stderr, "Problem getting audio stream info.\n");
        return AUDIO_OPEN;
    }

    // From ChatGPT 20230719
    for (int i = 0; i < audioContext->nb_streams; i++)
    {
        if (audioContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) 
        {
            state->audioStreamIndex = i;
            break;
        }
    }
    if (state->audioStreamIndex == -1)
    {
        fprintf(stderr, "Unable to find an audio stream in %s\n", state->audioFilename);
        return AUDIO_OPEN;
    }

    state->audioStream = audioContext->streams[state->audioStreamIndex];
    AVCodecParameters *codecParams = audioContext->streams[state->audioStreamIndex]->codecpar;

    state->audioDecoder = avcodec_find_decoder(codecParams->codec_id);
    if (state->audioDecoder == NULL)
    {
        fprintf(stderr, "Unsupported codec in %s\n", state->audioFilename);
        return AUDIO_OPEN;
    }

    state->audioDecoderContext = avcodec_alloc_context3(state->audioDecoder);
    if (state->audioDecoderContext == NULL) 
    {
        fprintf(stderr, "Error allocating input audio decoder context\n");
        return AUDIO_DECODE;
    }

    if (avcodec_parameters_to_context(state->audioDecoderContext, codecParams) < 0) 
    {
        fprintf(stderr, "Error copying audio decodec parameters to decodec context\n");
        return AUDIO_DECODE;
    }

    if (avcodec_open2(state->audioDecoderContext, state->audioDecoder, NULL) < 0) 
    {
        fprintf(stderr, "Error openening audio decodec\n");
        return AUDIO_DECODE;
    }

    state->audioPacket = av_packet_alloc();
    if (state->audioPacket == NULL)
    {
        fprintf(stderr, "Error allocating audio packet.\n");
        return AUDIO_TRANSCODE;
    }
    state->outAudioPacket = av_packet_alloc();
    if (state->outAudioPacket == NULL)
    {
        fprintf(stderr, "Error allocating output audio packet.\n");
        return AUDIO_TRANSCODE;
    }
    state->audioEncoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (state->audioEncoder == NULL)
    {
        fprintf(stderr, "Error getting audio encoder\n");
        return AUDIO_TRANSCODE;
    }

    // Output audio codec context
    state->audioEncoderContext = avcodec_alloc_context3(state->audioEncoder);
    if (state->audioEncoderContext == NULL)
    {
        fprintf(stderr, "Unable to allocate audio encoder context.\n");
        return AUDIO_NO_ENCODER_CONTEXT;
    }
    state->audioEncoderContext->sample_rate = state->audioDecoderContext->sample_rate;
    status = av_channel_layout_copy(&state->audioEncoderContext->ch_layout, &stereo);
    if (status != 0)
    {
        fprintf(stderr, "Unable to init audio encoder channel layout.\n");
        return AUDIO_INIT;
    }
    state->audioEncoderContext->bit_rate = 128000;  // Set your desired AAC bitrate
    state->audioEncoderContext->codec_id = AV_CODEC_ID_AAC;
    state->audioEncoderContext->codec_type = AVMEDIA_TYPE_AUDIO;
    state->audioEncoderContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
    state->audioEncoderContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    state->audioEncoderContext->frame_size = 1024;
    state->audioEncoderContext->profile = FF_PROFILE_AAC_LTP;

    // CheatGPT is providing inelegant code:
    // But, hey, I've asked it to work with libav :< 
    // Set the sample format of the AAC encoder context
    state->audioEncoderContext->sample_fmt = AV_SAMPLE_FMT_FLTP;

    status = avcodec_open2(state->audioEncoderContext, state->audioEncoder, NULL);
    if (status < 0)
    {
        fprintf(stderr, "Error opening audio encoder: %s\n", av_err2str(status));
        return AUDIO_NO_ENCODER_CONTEXT;
    }    

    state->audioFrame = av_frame_alloc();
    if (state->audioFrame == NULL) 
    {
        fprintf(stderr, "Error allocating audio frame\n");
        return AUDIO_TRANSCODE;
    }
    state->audioFrame->nb_samples = state->audioEncoderContext->frame_size;
    state->audioFrame->format = state->audioEncoderContext->sample_fmt;
    status = av_channel_layout_copy(&state->audioFrame->ch_layout, &state->audioEncoderContext->ch_layout);
    if (status != 0) 
    {
        fprintf(stderr, "Unable to init audioFrame channel layout\n");
        return AUDIO_INIT;
    }
    if (av_frame_get_buffer(state->audioFrame, 0) < 0) 
    {
        fprintf(stderr, "Error: Could not allocate buffer for AAC frame\n");
        return AUDIO_MEMORY;
    }

    state->outAudioFrame = av_frame_alloc();
    if (!state->outAudioFrame) 
    {
        fprintf(stderr, "Error: Could not allocate AAC frame\n");
        return AUDIO_TRANSCODE;
    }
    state->outAudioFrame->sample_rate = state->audioEncoderContext->sample_rate;
    state->outAudioFrame->format = state->audioEncoderContext->sample_fmt;
    state->outAudioFrame->nb_samples = state->audioFrame->nb_samples;
    status = av_channel_layout_copy(&state->outAudioFrame->ch_layout, &state->audioEncoderContext->ch_layout);
    if (status != 0)
    {
        fprintf(stderr, "Unable to init out audio frame.\n");
        return AUDIO_INIT;
    }
    status = av_frame_get_buffer(state->outAudioFrame, 0);
    if (status < 0)
    {
        fprintf(stderr, "Error allocating out audio frame buffer: %s\n", av_err2str(status));
        return AUDIO_TRANSCODE;
    }

    av_dump_format(audioContext, 0, state->audioFilename, 0);

    // Audio transcoding
    // state->swr = swr_alloc();
    status = swr_alloc_set_opts2(&state->swr, &stereo, AV_SAMPLE_FMT_FLTP, 44100, &stereo, state->audioStream->codecpar->format, state->audioStream->codecpar->sample_rate, 0, NULL);
    if (state->swr == NULL)
    {
        fprintf(stderr, "Unable to allocate SWR context.\n");
        return AUDIO_MEMORY;
    }
    
    status = swr_init(state->swr);
    if (status < 0)
    {
        fprintf(stderr, "Unable to initialize audio transcoder: %s\n", av_err2str(status));
        return AUDIO_TRANSCODE;
    }

    state->outAudioStream = avformat_new_stream(videoContext, NULL);
    if (state->outAudioStream == NULL)
    {
        fprintf(stderr, "Problem initializing output audio stream\n");
        return AUDIO_OUTPUT_STREAM;
    }
    state->outAudioStream->id = 1;


    // Copy codec parameters to the output audio stream
    if (avcodec_parameters_copy(state->outAudioStream->codecpar, codecParams) < 0) 
    {
        fprintf(stderr, "Error: Could not copy codec parameters to output audio stream\n");
        return AUDIO_TRANSCODE;
    }
    state->outAudioStream->codecpar = audioContext->streams[0]->codecpar;
    state->outAudioStream->codecpar->codec_id = AV_CODEC_ID_AAC;
    state->outAudioStream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    state->outAudioStream->codecpar->bit_rate = 64000;
    state->outAudioStream->codecpar->sample_rate = 44100;
    status = av_channel_layout_copy(&state->outAudioStream->codecpar->ch_layout, &stereo);
    if (status != 0)
    {
        fprintf(stderr, "Unable to init audio stream channel layout.\n");
        return AUDIO_INIT;
    }
    state->outAudioStream->codecpar->format = AV_SAMPLE_FMT_S16;
    state->outAudioStream->codecpar->frame_size = 1024;



    return AUDIO_OK;
}

int transcodeAudioFrames(AudioState *state, int frameCounter, double *elapsedAudioTime, AVFormatContext *videoContext, AVCodecContext *videoCodecContext)
{
    uint8_t **input = NULL;
    uint8_t *output = NULL;
    int in_samples = 0;
    int out_samples = 0;
    int status = 0;
    static int64_t audio_pts_offset = 0;
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;

    if (state != NULL)
    {
        status = av_read_frame(state->audioContext, state->audioPacket);
        if (status < 0)
        {
            return AUDIO_EOF;
        }
        if (state->audioPacket->stream_index == state->audioStreamIndex)
        {
            if (avcodec_send_packet(state->audioDecoderContext, state->audioPacket) < 0)
            {
                fprintf(stderr, "Error sending packet to decoder.\n");
                return AUDIO_DECODE;
            }
            while (avcodec_receive_frame(state->audioDecoderContext, state->audioFrame) >= 0) 
            {
                if (elapsedAudioTime != NULL)
                    *elapsedAudioTime = (double)state->audioFrame->pts / (double) state->audioDecoderContext->sample_rate;
                if (*elapsedAudioTime < state->startTime)
                {
                    audio_pts_offset = state->audioFrame->pts;
                    return AUDIO_OK;
                }
                av_frame_make_writable(state->outAudioFrame);
                state->outAudioFrame->nb_samples = av_rescale_rnd(state->audioFrame->nb_samples, state->audioEncoderContext->sample_rate, state->audioFrame->sample_rate, AV_ROUND_UP);
                state->outAudioFrame->pts = state->audioFrame->pts - audio_pts_offset;
                status = av_channel_layout_copy(&state->audioFrame->ch_layout, &stereo);
                if (status != 0)
                {   
                    fprintf(stderr, "Unable to init audio frame channel layout.\n");
                    return AUDIO_INIT;
                }
                state->outAudioFrame->format = AV_SAMPLE_FMT_FLTP;
                status = swr_convert_frame(state->swr, state->outAudioFrame, state->audioFrame) < 0;
                if (status < 0) 
                {
                    fprintf(stderr, "Error: Failed to convert input frame to AAC-compatible format: %s\n", av_err2str(status));
                    return AUDIO_TRANSCODE;
                }

                status = avcodec_send_frame(state->audioEncoderContext, state->outAudioFrame);
                if (status < 0) 
                {
                    fprintf(stderr, "Error: Failed to send frame for AAC encoding: %s\n", av_err2str(status));
                    return AUDIO_TRANSCODE;
                }
                if (state->outAudioFrame)
                    while (status >= 0)
                    {
                        status = avcodec_receive_packet(state->audioEncoderContext, state->outAudioPacket);
                        if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
                            break;
                        else if (status < 0) 
                        {
                            fprintf(stderr, "Error: Failed to receive encoded packet: %s\n", av_err2str(status));
                            return AUDIO_TRANSCODE;
                        }
                        state->outAudioPacket->stream_index = 1;
                        av_interleaved_write_frame(videoContext, state->outAudioPacket);
                    }                    
            }
        }
    }
    else
    {
        status = avcodec_send_frame(videoCodecContext, NULL);
    }
    
    return status;
}

int finishAudio(AudioState *state, AVFormatContext *videoContext, AVCodecContext *videoCodecContext)
{
    if (state->haveAudio)
        transcodeAudioFrames(NULL, 0, NULL, videoContext, videoCodecContext);

    return AUDIO_OK;

}

void cleanupAudio(AudioState *state)
{
    if (state == NULL)
        return;

    if (state->haveAudio && !state->bypassAudio)
    {
        av_packet_free(&state->audioPacket);
        av_frame_free(&state->audioFrame);
        swr_free(&state->swr);
    }

    return;
}


