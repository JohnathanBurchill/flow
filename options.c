#include "options.h"

void usage(const char * name)
{
    printf("\nflow version %s compiled %s %s UTC\n", FLOW_VERSION, __DATE__, __TIME__);
    printf("\nLicense: GPL 3.0 ");
    printf("Copyright 2023 Johnathan Kerr Burchill\n");
    printf("\nUsage:\n");
    printf("\n  %s <midifilename> <audiofilename> <outputfilename> <videoTitleText> [options] \n", name);
    printf("Options:\n");
    printf("%40s - %s\n", "-h or --help", "print this help message");

    printf("%40s - %s\n", "-f", "forces overwriting <outputfilename> if it exists");
    printf("%40s - %s\n", "--no-audio", "Do not include audio");
    printf("%40s - %s\n", "--cycle-colour-tables=<index>", "Cycle colour tables once per second, using the colour at offset <index>. Default -1 (disabled)");
    printf("%40s - %s\n", "--frame-rate=<rate>", "video frame rate (frames/s)");

    printf("%40s - %s\n", "--window-timespan=<duration>", "Approximate time for note to fall (s)");
    printf("%40s - %s\n", "--start-time=<startTime>", "Start video at <startTime> (s). Default: 0.0 s");
    printf("%40s - %s\n", "--stop-time=<startTime>", "Stop video at <stopTime> (s). Default: end of song");
    printf("%40s - %s\n", "--extra-time=<extraTime>", "Stop video <extraTime> seconds after end of song");
    printf("%40s - %s\n", "--note-highlight-halflife=<duration>", "Half life for note highlighting, from sounding time (s)");
    printf("%40s - %s\n", "--note-visibility-halflife=<duration>", "Half life for note visibility, from sounding time (s)");
    printf("%40s - %s\n", "--max-note-width=<width>", "Set the maximum note trail width to <width>");
    printf("%40s - %s\n", "--flow-shear-scale=<magnitude>", "Set the scale of the flow shear. Default 1.");
    printf("%40s - %s\n", "--flow-shear-y-points=<n>", "Set the number of control points for flow shear. Default: 5");
    printf("%40s - %s\n", "--random-seed=<seedInteger>", "Set the random seed integer. Pass -1 for clock-based seed");
    printf("%40s - %s\n", "--note-wiggle-time-delta-t=<seconds>", "Set the note wiggle time period");
    printf("%40s - %s\n", "--note-wiggle-offset=<amount>", "Set the note wiggle offset in denominator of scaling");
    printf("%40s - %s\n", "--note-wiggle-amplitude=<amount>", "Set the note wiggle amplitude scale");
    printf("%40s - %s\n", "--wiggle-wavelength=<amount>", "Set the note wiggle wavelength");
    printf("%40s - %s\n", "--wiggle-period=<amount>", "Set the note wiggle period");
    printf("%40s - %s\n", "--acceleration=<pixels/s/s>", "Set the note acceleration downward in pixels per second per second");
    printf("%40s - %s\n", "--uhd", "4k UDH (3840x2160)");
    printf("%40s - %s\n", "--frame-width=<width>", "Set video frame width");
    printf("%40s - %s\n", "--frame-height=<height>", "Set video frame height");
    printf("%40s - %s\n", "--video-filter-graph=<rules>", "Apply a simple FFMPEG video filter");
    printf("%40s - %s\n", "--SDL-window-renderer", "Render video with SDLWindow (i.e. hardware) instead of in software. Default: software rendering");
    printf("%40s - %s\n", "--faster-rgb2yuv", "Trade quality for a faster RGB to YUV420P converter");
    printf("%40s - %s\n", "--no-video-filter", "Do not apply the video filter");
    printf("%40s - %s\n", "--track-to-display=<track>", "Display only <track>. Default: -1 (all tracks)");
    printf("%40s - %s\n", "--background-colour=<r,g,b,a>", "Use colour r,g,b,a (or a named colour) for the background. Default: black");
    printf("%40s - %s\n", "--track-colour=<r,g,b,a>", "Use colour r,g,b,a (or a named colour) for the track colours. Default: uses colour table.");
    printf("%40s - %s\n", "--video-title-font=<fontname>", "Use <fontname> for video title. Default: DejaVuSans.ttf. Font must be in working directory.");
    printf("%40s - %s\n", "--video-title-fontsize=<size>", "Use <size> for video title font size. Default: 160.");
    printf("%40s - %s\n", "--video-title-colour=<colour>", "Use <colour> for video title. Default: white. <colour> can be a name or in the form <r,g,b,a> with values from 0 to 255.");
    printf("%40s - %s\n", "--video-title-decay-time=<seconds>", "Use <seconds> for video title decay. Default: window time span.");
    printf("%40s - %s\n", "--pedal-modulates-background", "Pedal down darkens the background. Default: off");
    printf("%40s - %s\n", "--pedal-sustains-notes", "Pedal down sustains applicable notes. Default: off");
    printf("%40s - %s\n", "--colour-table=<id>", "Use colour table <id>. Default: 0");
    printf("%40s - %s\n", "--verbose", "Display MIDI tracks. Default: not verbose");
    printf("%40s - %s\n", "--license", "Summary of distribution license.\n");

    return;
}

int parseOptions(State *state, int argc, char **argv)
{
    if (state == NULL || argv == NULL)
        return FLOW_ARGS;

    int status = FLOW_OK;
    
    for (int i = 0; i < argc; i++)
    {
        if (strcmp("-f", argv[i]) == 0)
        {
            state->nOptions++;
            state->overwrite = true;
        }
        else if (strcmp("--no-audio", argv[i]) == 0)
        {
            state->nOptions++;
            state->audioState.bypassAudio = true;
        }
        else if (strncmp("--cycle-colour-tables=", argv[i], 22) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 23)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->cycleColourTables = atoi(argv[i] + 22);
        }
        else if (strncmp("--frame-rate=", argv[i], 13) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 14)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->videoState.frameRate = atof(argv[i] + 13);
        }
        else if (strncmp("--frame-width=", argv[i], 14) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 15)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->videoState.frameWidth = atof(argv[i] + 14);
        }
        else if (strncmp("--frame-height=", argv[i], 15) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 16)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->videoState.frameHeight = atof(argv[i] + 15);
        }
        else if (strcmp("--uhd", argv[i]) == 0)
        {
            state->nOptions++;
            state->videoState.uhd = true;
        }
        else if (strcmp("--sd", argv[i]) == 0)
        {
            state->nOptions++;
            state->videoState.sd = true;
        }
        else if (strncmp("--video-title-font=", argv[i], 19) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 20)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->videoState.videoTitleFont = argv[i] + 19;
        }
        else if (strncmp("--video-title-fontsize=", argv[i], 23) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 24)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->videoState.videoTitlefontSize = atoi(argv[i] + 23);
        }
        else if (strncmp("--video-title-colour=", argv[i], 21) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 22)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->videoState.videoTitleColour = colourFromString(argv[i] + 21);
        }
        else if (strncmp("--video-title-decay-time=", argv[i], 25) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 26)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->videoState.videoTitleDecayTime = atof(argv[i] + 25);
        }
        else if (strcmp("--pedal-modulates-background", argv[i]) == 0)
        {
            state->nOptions++;
            state->pedalModifiesBackground = true;
        }
        else if (strcmp("--pedal-sustains-notes", argv[i]) == 0)
        {
            state->nOptions++;
            state->pedalModifiesNotelength = true;
        }
        else if (strcmp("--SDL-window-renderer", argv[i]) == 0)
        {
            state->nOptions++;
            state->videoState.sdlRendering = true;
        }
        else if (strcmp("--faster-rgb2yuv", argv[i]) == 0)
        {
            state->nOptions++;
            state->videoState.fastRgb2Yuv = true;
        }
        else if (strcmp("--no-video-filter", argv[i]) == 0)
        {
            state->nOptions++;
            state->videoState.applyVideoFilter = false;
        }
        else if (strcmp("--verbose", argv[i]) == 0)
        {
            state->nOptions++;
            state->verbose = true;
            state->videoState.verbose = true;
            state->audioState.verbose = true;
        }
        else if (strncmp("--track-to-display=", argv[i], 19) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 20)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->trackToDisplay = atoi(argv[i] + 19);
        }
        else if (strncmp("--colour-table=", argv[i], 15) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 16)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->colourTable = atoi(argv[i] + 15);
        }
        else if (strncmp("--window-timespan=", argv[i], 18) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 19)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->windowTimeSpan = atof(argv[i] + 18);
        }
        else if (strncmp("--start-time=", argv[i], 13) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 14)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->startTime = atof(argv[i] + 13);
            state->audioState.startTime = state->startTime;
        }
        else if (strncmp("--stop-time=", argv[i], 12) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 13)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->stopTime = atof(argv[i] + 12);
            state->audioState.stopTime = state->stopTime;
        }
        else if (strncmp("--extra-time=", argv[i], 13) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 14)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->extraTime = atof(argv[i] + 13);
        }
        else if (strncmp("--note-highlight-halflife=", argv[i], 26) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 27)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->noteHighlightHalfLife = atof(argv[i] + 26);
        }
        else if (strncmp("--note-visibility-halflife=", argv[i], 27) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 28)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->noteVisibilityHalfLife = atof(argv[i] + 27);
        }
        else if (strncmp("--max-note-width=", argv[i], 17) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 18)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->maxNoteWidth = atoi(argv[i] + 17);
        }
        else if (strncmp("--flow-shear-scale=", argv[i], 19) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 20)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->flowShearScale = atof(argv[i] + 19);
        }
        else if (strncmp("--flow-shear-y-points=", argv[i], 22) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 23)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->nShearYPoints = atof(argv[i] + 22);
        }
        else if (strncmp("--wiggle-wavelength=", argv[i], 20) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 21)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->wiggleWavelength = atof(argv[i] + 21);
        }
        else if (strncmp("--wiggle-period=", argv[i], 16) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 17)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->wigglePeriod = atof(argv[i] + 16);
        }
        else if (strncmp("--note-wiggle-time-delta-t=", argv[i], 27) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 28)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->shearDeltaT = atof(argv[i] + 27);
        }
        else if (strncmp("--note-wiggle-offset=", argv[i], 21) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 22)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->wiggleOffset = atof(argv[i] + 21);
        }
        else if (strncmp("--note-wiggle-amplitude=", argv[i], 24) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 25)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->wiggleAmplitude = atof(argv[i] + 24);
        }
        else if (strncmp("--acceleration=", argv[i], 15) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 16)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->noteAcceleration = atof(argv[i] + 15);
        }
        else if (strncmp("--random-seed=", argv[i], 14) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 15)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->randomSeed = atoi(argv[i] + 14);
        }
        else if (strncmp("--background-colour=", argv[i], 20) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 21)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->backgroundColour = colourFromString(argv[i] + 20);
        }
        else if (strncmp("--track-colour=", argv[i], 15) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 16)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->trackColour = colourFromString(argv[i] + 15);
            state->replaceTrackColour = true;
        }
        else if (strncmp("--video-filter-graph=", argv[i], 21) == 0)
        {
            state->nOptions++;
            if (strlen(argv[i]) < 22)
            {
                fprintf(stderr, "Unable to interpret %s\n", argv[i]);
                exit(EXIT_FAILURE);
            }
            state->videoState.videoFilterGraph = argv[i] + 21;
        }
        else if ((strcmp("-h", argv[i]) == 0) || (strcmp("--help", argv[i]) == 0))
        {
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else if (strcmp("--license", argv[i]) == 0)
        {
            fprintf(stdout, "flow Copyright (C) 2023  Johnathan K. Burchill\n");
            fprintf(stdout, "This program comes with ABSOLUTELY NO WARRANTY.\n");
            fprintf(stdout, "This is free software. You are welcome to redistribute\n"); fprintf(stdout, "it under certain conditions as set forth in the GNU\n");
            fprintf(stdout, "General Public License version 3.\n");
            exit(EXIT_SUCCESS);
        }
        else if (strncmp("--", argv[i], 2) == 0)
        {
            fprintf(stderr, "Cannot interpret requested option %s\n", argv[i]);
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (argc - state->nOptions != 5)
    {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // TODO check validity of all options
    if (state->nShearYPoints < 3)
    {
        fprintf(stderr, "Number of Y-position control points for wiggle must be greater than 2.\n");
        exit(EXIT_FAILURE);
    }

    state->audioState.midiFilename = argv[1];
    state->audioState.audioFilename = argv[2];
    state->videoState.outputFilename = argv[3];
    state->videoState.videoTitleText = argv[4];

    if (state->videoState.uhd)
    {
        // Twice resolution of HD (1920x1080)
        state->videoState.frameWidth = 3840;
        state->videoState.frameHeight = 2160;
        state->maxNoteWidth *= 2;
        state->flowShearScale *= 2;
        state->videoState.videoTitlefontSize *= 2;
        state->wiggleAmplitude *= 2;
    }
    else if (state->videoState.sd)
    {
        // 1/3 horizontal resolution of HD (1920x1080)
        state->videoState.frameWidth = 640;
        state->videoState.frameHeight = 480;
        state->maxNoteWidth /= 3;
        if (state->maxNoteWidth < 3)
            state->maxNoteWidth = 3;
        state->flowShearScale /= 3;
        state->videoState.videoTitlefontSize /= 3;
        state->wiggleAmplitude /= 3;
    }

    if (state->videoState.videoTitleDecayTime < 0)
        state->videoState.videoTitleDecayTime = state->windowTimeSpan;

    return FLOW_OK;

}

