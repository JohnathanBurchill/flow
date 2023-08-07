/*

    flow: flow.c

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

#include "flow.h"
#include "video.h"
#include "midi.h"
#include "colour.h"
#include "physics.h"
#include "options.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <sys/time.h>

#include <SDL2/SDL_ttf.h>

static MidiNote noteStatus[MIDI_NOTE_RANGE][MIDI_CHANNELS] = {0};
uint64_t counter = 0;
double noteLengthCounter = 0.0;

int main(int argc, char **argv)
{
    int status = FLOW_OK;

    State state = {0};
    status = initState(&state);
    if (status != FLOW_OK)
    {
        fprintf(stderr, "Error initializing program state.\n");
        exit(1);
    }

    status = parseOptions(&state, argc, argv);
    if (status != FLOW_OK)
    {
        fprintf(stderr, "Error parsing options.\n");
        exit(1);
    }

    // Data

    if (!access(state.videoState.outputFilename, F_OK) && !state.overwrite)
    {
        printf("%s exists, skipping. Append -f option to force export.\n", state.videoState.outputFilename);
        goto cleanup;
    }

    // Loads audio file, prepares output MP4
    status = initVideoProcessor(&state.videoState);
    if (status < 0)
    {
        fprintf(stderr, "Problem intializing video: got status %d.\n", status);
        exit(EXIT_FAILURE);
    }

    // Audio setup
    if (strcmp("none", state.audioState.audioFilename) != 0 && !state.audioState.bypassAudio)
    {
        // av_log_set_level(AV_LOG_VERBOSE);
        status = initAudio(&state.audioState, state.videoState.videoContext);
        if (status != VIDEO_OK)
        {
            fprintf(stderr, "Could not initialize audio.\n");
            return VIDEO_AUDIO_OPEN;
        }
        state.audioState.haveAudio = true;
    }
    else
        state.audioState.haveAudio = false;


    // Write file header
    status = avformat_write_header(state.videoState.videoContext, &state.videoState.dict);
    if (status < 0)
    {
        fprintf(stderr, "Problem writing video header: %s\n", av_err2str(status));
        return status;
    }

    // Read MIDI notes, exit now if problem
    status = readMidi(&state);
    if (status != MIDI_OK)
    {
        fprintf(stderr, "Unable to read MIDI file %s\n", state.audioState.midiFilename);
        exit(EXIT_FAILURE);
    }

    // Construct frames and export MPEG
    status = flow(&state);

cleanup:

    cleanupAudio(&state.audioState);
    cleanupVideo(&state.videoState);

    // One-time operation, let the OS free Song memory after exit.

    fflush(stdout);

    TTF_Quit();

    exit(status);
}

int initState(State *state)
{
    if (state == NULL)
        return FLOW_ARGS;

    state->videoState.frameRate = DEFAULT_FRAMES_PER_SECOND;
    state->windowTimeSpan = DEFAULT_WINDOW_TIMESPAN;
    state->noteVisibilityHalfLife = DEFAULT_NOTE_VISIBILITY_HALFLIFE;
    state->noteHighlightHalfLife = DEFAULT_NOTE_HIGHLIGHT_HALFLIFE;
    state->maxNoteWidth = DEFAULT_MAX_NOTE_WIDTH;
    // state->videoFilterGraph = "gblur=sigma=2:steps=2";
    // state->videoFilterGraph = "avgblur=sizeX=2";
    state->videoState.videoFilterGraph = "boxblur";
    state->videoState.frameWidth = DEFAULT_IMAGE_WIDTH;
    state->videoState.frameHeight = DEFAULT_IMAGE_HEIGHT;
    state->videoState.applyVideoFilter = true;
    state->trackToDisplay = -1; // All tracks
    state->startTime = 0.0; // seconds
    state->stopTime = -1.0; // Automatic: use song duration
    state->audioState.startTime = state->startTime;
    state->audioState.stopTime = state->stopTime;
    state->extraTime = state->windowTimeSpan;
    state->backgroundColour = colourFromString("black");
    state->nShearYPoints = DEFAULT_SHEAR_Y_POINTS;
    state->shearDeltaT = DEFAULT_SHEAR_DELTAT;
    state->wigglePeriod = DEFAULT_WIGGLE_PERIOD;
    state->nShearTimes = 1;
    state->wiggleOffset = DEFAULT_WIGGLE_OFFSET;
    state->wiggleAmplitude = DEFAULT_WIGGLE_AMPLITUDE;
    state->wiggleWavelength = DEFAULT_WIGGLE_WAVELENGTH;
    state->flowShearScale = DEFAULT_FLOW_SHEAR_SCALE;
    state->noteAcceleration = DEFAULT_NOTE_ACCELERATION; // pixels per second per second
    state->randomSeed = -1; // Seed from system clock
    state->colourTable = DEFAULT_COLOUR_TABLE;
    state->cycleColourTables = -1; // Cycling is off

    state->videoState.videoTitleFont = DEFAULT_VIDEO_TITLE_FONT;
    state->videoState.videoTitlefontSize = DEFAULT_VIDEO_TITLE_FONTSIZE;
    state->videoState.videoTitleColour = DEFAULT_VIDEO_TITLE_COLOUR;
    state->videoState.videoTitleDecayTime = DEFAULT_VIDEO_TITLE_DECAY_TIME;

    return FLOW_OK;
}

int flow(State *state)
{
    if (state == NULL)
        return VIDEO_ARG;

    int frameCounter = 0;
    double elapsedAudioTime = 0;
    bool moreAudio = true;
    int status = VIDEO_OK;

    int w = 0, h = 0;
    SDL_QueryTexture(state->videoState.videoTexture, NULL, NULL, &w, &h);

    double fps = 0.0;

    SDL_Rect texr = {0};
    texr.x = state->videoState.frameWidth/2;
    texr.y = state->videoState.frameHeight/2;
    texr.w = w*2;
    texr.h = h*2; 

    // TODO Step in time from tStart to tEnd in steps of frame rate...
    int alpha = 255;
    double alphaF = 0;
    double videoTime = 0.;
    double framePeriod = 1.0 / state->videoState.frameRate;
    int yStart = 0;
    int yStop = 0;
    double acceleration = 0.5; // per second

    MidiSong *song = state->song;
    if (song == NULL)
        return VIDEO_MISSING_NOTES;

    int minNote = song->minNote - 5;
    int maxNote = song->maxNote + 5;
    song->noteSpan = maxNote - minNote + 1;
    
    MidiNote *n = NULL;

    MidiTrack *track = NULL;
    MidiNote *note = NULL;

    MidiNote titleTextNote = {0};
    titleTextNote.startTime = 0;
    titleTextNote.message = strdup(state->videoState.videoTitleText);
    if (titleTextNote.message == NULL)
        return VIDEO_MEMORY;
    titleTextNote.note = (minNote + maxNote) / 2;

    initializeNoteDynamics(state, &titleTextNote, song->noteSpan, minNote);
    titleTextNote.dynamics.y[0] = state->videoState.frameHeight / 2;

    double x = 0;
    double noteLength = 0;
    RGBAColour noteColour = {0};

    double maxTime = song->maxTime + state->extraTime;
    double startTime = state->startTime;
    double stopTime = state->stopTime;
    if (stopTime < 0.0)
        stopTime = maxTime;

    double videoSeconds = stopTime + state->windowTimeSpan - startTime;
    state->remainingTime = videoSeconds;

    int hours = 0;
    int minutes = 0;
    double seconds = 0.0;
    int hoursLeft = 0;
    int minutesLeft = 0;
    double secondsLeft = 0.0;

    if (state->verbose)
    {
        double hours = floor(videoSeconds / 3600.0);
        double minutes = floor((videoSeconds - (3600.0 * hours)) / 60.0);
        double seconds = videoSeconds - 3600.0 * hours - 60.0 * minutes;
        printf("Video duration: %02.0lf:%02.0lf:%03.1lf\n", hours, minutes, seconds);
    }

    state->nShearTimes = (int) floor(song->maxTime / state->shearDeltaT) + 1;

    RGBAColour c = state->backgroundColour;

    // Bezier curve control points
    Sint16 xp[NOTE_DYNAMICS_POINTS * 2] = {0};
    Sint16 yp[NOTE_DYNAMICS_POINTS * 2] = {0};
    // SDL_Point points[22] = {0};

    double x1 = 0;
    double lineWidth = 0;

    NoteDynamics *d = NULL;

    // Reproducible randomness
    if (state->randomSeed >= 0)
        srand(state->randomSeed);

    TTF_Font* Sans = TTF_OpenFont("DejaVuSans.ttf", 24);
    SDL_Color White = {255, 255, 255, 255};
    int ctWidth = 0;
    int ctHeight = 0;

    TTF_Font* titleFont = TTF_OpenFont(state->videoState.videoTitleFont, state->videoState.videoTitlefontSize);
    int titleWidth = 0;
    int titleHeight = 0;
    TTF_SizeText(titleFont, titleTextNote.message, &titleWidth, &titleHeight);

    SDL_SetRenderTarget(state->videoState.renderer, state->videoState.videoTexture);
    SDL_SetRenderDrawBlendMode(state->videoState.renderer, SDL_BLENDMODE_BLEND);
    SDL_Rect message_rect;
    message_rect.x = state->videoState.frameWidth / 2 - titleWidth / 2;
    message_rect.y = 0;
    message_rect.w = titleWidth;
    message_rect.h = titleHeight;

    SDL_Rect titleRect;
    titleRect.x = state->videoState.frameWidth / 2 - titleWidth / 2; 
    titleRect.y = state->videoState.frameHeight / 2 - titleHeight / 2;
    titleRect.w = titleWidth;
    titleRect.h = titleHeight;
    titleTextNote.dynamics.y[0] = titleRect.y;

    RGBAColour tc = state->videoState.videoTitleColour;
    double titleAl = 255.0;

    MidiNote *pedal = NULL;
    RGBAColour defaultBg = c;
    RGBAColour bg = defaultBg;
    double colourScaling = 1.0;
    int notePoints = 0;

    MidiNote *statusNote = NULL;
    MidiNote *refNote = NULL;

    SDL_Event sdlEvent = {0};

    double updateRate = state->videoState.frameRate;
    bool running = true;

    struct timeval clockTime = {0};
    gettimeofday(&clockTime, NULL);
    double lastRealtime = (double)clockTime.tv_sec + (double)clockTime.tv_usec/1000000.0;
    double currentRealtime = lastRealtime;

    videoTime = state->startTime - state->windowTimeSpan;
    if (videoTime < 0.)
        videoTime = 0.;
    for (; videoTime < stopTime && running == true; videoTime += framePeriod, state->remainingTime -= framePeriod)
    {
        if (state->videoState.sdlRendering)
        {
            if (SDL_PollEvent(&sdlEvent))
            {
                switch(sdlEvent.type)
                {
                    case SDL_QUIT:
                        fprintf(stderr, "\nInterrupted...\n");
                        running = false;
                        continue;
                        break;
                    default:
                        break;
                }
            }
        }
        hoursMinutesSeconds(videoTime, &hours, &minutes, &seconds);
        hoursMinutesSeconds(state->remainingTime, &hoursLeft, &minutesLeft, &secondsLeft);

        if (state->pedalModifiesBackground && pedal != NULL && pedal->startTime <= videoTime && pedal->stopTime > videoTime)
        {
            colourScaling = (double)pedal->speed / 127.0;
            bg.r = (int) (defaultBg.r * colourScaling);
            bg.g = (int) (defaultBg.g * colourScaling);
            bg.b = (int) (defaultBg.b * colourScaling);
        }
        else
            bg = defaultBg;

        SDL_SetRenderDrawColor(state->videoState.renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderClear(state->videoState.renderer);

        if (frameCounter % ((int)updateRate) == 0)
        {
            if (videoTime >= state->startTime)
            {
                fprintf(stdout, "\r                 \r%02d:%02d:%02.0lf, %02d:%02d:%02.0lf to go", hours, minutes, seconds, hoursLeft, minutesLeft, secondsLeft);
                if (state->verbose)
                {
                    gettimeofday(&clockTime, NULL);
                    currentRealtime = (double)clockTime.tv_sec + (double)clockTime.tv_usec/1000000.0;
                    if (currentRealtime - lastRealtime != 0.0)
                        fps /= (currentRealtime - lastRealtime);
                    else
                        fps = 0.0;
                    lastRealtime = currentRealtime;
                    printf("  (notes-per-frame: %llu, notelengths=%.1lf, fps=%.1lf          )", (uint64_t)((double)counter / updateRate), noteLengthCounter, fps);
                }
                counter = 0;
                noteLengthCounter = 0.0;
                fps = 0.0;
            }
            else
                fprintf(stdout, "\rFast-fowarding...");
            fflush(stdout);
        }

        // Video title
        if (strlen(titleTextNote.message) > 0 && videoTime < state->windowTimeSpan)
        {
            updateNoteDynamics(state, &titleTextNote, 0, framePeriod, videoTime, pedal);
            titleAl -= titleAl * framePeriod / (state->videoState.videoTitleDecayTime / 20.0);
            if (titleAl < 1.0)
                titleAl = 1.0;
            tc.a = (int)titleAl;
            SDL_Surface* videoTitleSurface = TTF_RenderText_Blended(titleFont, titleTextNote.message, (SDL_Color){tc.r, tc.g, tc.b, tc.a});
            SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(state->videoState.renderer, videoTitleSurface);
            titleRect.y = titleTextNote.dynamics.y[0];
            SDL_RenderCopy(state->videoState.renderer, titleTexture, NULL, &titleRect);
            SDL_FreeSurface(videoTitleSurface);
            SDL_DestroyTexture(titleTexture);        
        }

        // Loop over tracks
        for (int tr = 1; tr < song->nTracks; tr++)
        {
            track = &song->tracks[tr];
            if (track->tempoTrack || track->transportTrack)
                continue;

            if (state->replaceTrackColour)
            {
                noteColour = state->trackColour;
            }
            else if (state->cycleColourTables > -1)
            {
                int ct = (frameCounter/(int)state->videoState.frameRate) % NCOLOURTABLES;
                noteColour = colourFromTable(ct, state->cycleColourTables);
                // TTF howto at https://stackoverflow.com/questions/22886500/how-to-render-text-in-sdl2
                char msg[256] = {0};
                snprintf(msg, 256, "colourTables[%d][%d]", ct, state->cycleColourTables);
                SDL_Surface* surfaceMessage = TTF_RenderText_Blended(Sans, msg, White); 
                // printf("%p\n", Sans);
                SDL_Texture* message = SDL_CreateTextureFromSurface(state->videoState.renderer, surfaceMessage);
                status = SDL_RenderCopy(state->videoState.renderer, message, NULL, &message_rect);
                SDL_FreeSurface(surfaceMessage);
                SDL_DestroyTexture(message);
            }
            else
                noteColour = colourFromTable(state->colourTable, tr);


            // Draw each note that should be on the screen

            for (int n = 0; n < track->nNotes; n++)
            {
                // Operate only on notes that should appear on screen
                note = &track->notes[n];
                if ((note->startTime <= videoTime && note->stopTime + state->windowTimeSpan > videoTime))
                {
                    if (note->isPedal)
                    {
                        if (note->startTime <= videoTime && note->stopTime > videoTime)
                            pedal = note;
                        // const Sint16 px[4] = {0, 50, 50, 0};
                        // const Sint16 py[4] = {0, 0, 50, 50};
                        // filledPolygonRGBA(state->videoState.renderer, px, py, 4, 255, 255, 255, pedal->speed * 2);
                        continue;
                    }
                    if (!note->playing)
                    {
                        note->screenTime = videoTime - note->startTime;
                        note->playing = true;
                        status = initializeNoteDynamics(state, note, song->noteSpan, minNote);
                        if (status != PHYSICS_OK)
                            return status;
                    }
                    statusNote = &noteStatus[note->note][note->channel];
                    refNote = (MidiNote*)statusNote->referenceMidiNote;
                    if (refNote && statusNote->playing && refNote->stopTime < videoTime)
                    {
                        statusNote->playing = false;
                        statusNote->referenceMidiNote = NULL;
                    }

                    alphaF = (255.0 * (0.2 + exp(-note->screenTime / state->noteVisibilityHalfLife) * (double)note->speed / (double)NOTE_MAX_SPEED));

                    if (alphaF > 255)
                        alphaF = 255;
                    
                    alpha = (int) floor(alphaF);

                    // Update note dynamics
                    updateNoteDynamics(state, note, tr, framePeriod, videoTime, pedal);

                    d = &note->dynamics;
                    if (d->y[NOTE_DYNAMICS_POINTS-1] > state->videoState.frameHeight - 1)
                        continue;

                    lineWidth = (state->maxNoteWidth * note->speed) / 127.0;

                    // Fill polygon points
                    if (videoTime >= state->startTime)
                    {
                        noteLengthCounter += note->length;
                        for (int u = 0; u < NOTE_DYNAMICS_POINTS; u++)
                            if (d->y[u] >= (int)(-state->videoState.frameHeight / 100.0))
                                notePoints = u + 1;
                            else
                                break;

                        counter++;
                        for (int u = 0; u < notePoints; u++)
                        {
                            yp[u] = d->y[u];
                            yp[notePoints*2 - 1 - u] = yp[u];
                            x1 = d->x[u] - lineWidth / 2.0;
                            xp[u] = (int) x1;
                            xp[notePoints*2 - 1 - u] = (int) (x1 + lineWidth);
                        }
                        // Turn off any playing note
                        if (statusNote->playing)
                        {
                            if (refNote)
                            {
                                refNote->stopTime = videoTime;
                            }
                        }

                        statusNote->playing = true;
                        statusNote->referenceMidiNote = (void*)note;

                        filledPolygonRGBA(state->videoState.renderer, xp, yp, notePoints * 2, noteColour.r, noteColour.g, noteColour.b, alpha);
                        note->screenTime += framePeriod;
                    }
                }

            }
        }

        if (videoTime >= state->startTime)
        {
            generateFrame(&state->videoState, frameCounter);
            while (state->audioState.haveAudio && elapsedAudioTime < videoTime && moreAudio)
            {
                status = transcodeAudioFrames(&state->audioState, frameCounter, &elapsedAudioTime, state->videoState.videoContext, state->videoState.videoCodecContext);
                if (status == VIDEO_AUDIO_EOF)
                    moreAudio = false;
            }
            frameCounter++;
            fps++;
        }
    }
    
    finishVideo(&state->videoState);

    finishAudio(&state->audioState, state->videoState.videoContext, state->videoState.videoCodecContext);

    return VIDEO_OK;
}

