/*

    flow: flow.h

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

#ifndef _FLOW_H
#define _FLOW_H

#include "colour.h"
#include "audio.h"
#include "video.h"

#include <stdbool.h>

#define FLOW_VERSION "0.1.0"

#define DEFAULT_FRAMES_PER_SECOND 30.0

#define DEFAULT_WINDOW_TIMESPAN 5.0 // seconds
// #define DEFAULT_NOTE_VISIBILITY_HALFLIFE 2.5 // seconds
#define DEFAULT_NOTE_VISIBILITY_HALFLIFE 10.0 // seconds
#define DEFAULT_NOTE_HIGHLIGHT_HALFLIFE 0.25 // seconds
#define DEFAULT_MAX_NOTE_WIDTH 12
#define DEFAULT_SHEAR_Y_POINTS 5
#define DEFAULT_SHEAR_DELTAT 5.0 // seconds
#define DEFAULT_WIGGLE_PERIOD 2.0 // seconds
#define DEFAULT_WIGGLE_OFFSET 0.2
#define DEFAULT_WIGGLE_AMPLITUDE 1.0
#define DEFAULT_NOTE_ACCELERATION 1.0 // pixels per second per second
#define DEFAULT_FLOW_SHEAR_SCALE 1.0
#define DEFAULT_WIGGLE_WAVELENGTH 0.1
#define DEFAULT_COLOUR_TABLE 0

#define DEFAULT_VIDEO_TITLE_FONT "DejaVuSans.ttf"
#define DEFAULT_VIDEO_TITLE_FONTSIZE 160
#define DEFAULT_VIDEO_TITLE_COLOUR colourFromString("white")
#define DEFAULT_VIDEO_TITLE_DECAY_TIME -1.0 // automatic
enum FlowStatus
{
    FLOW_OK = 0,
    FLOW_ARGS,
    FLOW_MEMORY
};

struct MidiSong;

typedef struct State
{
    int nOptions;
    bool overwrite;

    double windowTimeSpan;

    struct MidiSong *song;

    AudioState audioState;
    VideoState videoState;

    double noteHighlightHalfLife;
    double noteVisibilityHalfLife;
    int maxNoteWidth;
    int nShearYPoints;
    double shearDeltaT;
    double wigglePeriod;
    int nShearTimes;
    double wiggleOffset;
    double wiggleAmplitude;
    double wiggleWavelength; // as a fraction of frame height
    double flowShearScale;
    double noteAcceleration;
    unsigned int randomSeed;
    RGBAColour backgroundColour;
    RGBAColour trackColour;
    bool replaceTrackColour;

    int trackToDisplay;
    int colourTable;
    int cycleColourTables;

    bool pedalModifiesBackground;
    bool pedalModifiesNotelength;

    double startTime;
    double stopTime;
    double extraTime;
    double remainingTime;

    bool verbose;

} State;

int initState(State *state);

int flow(State *state);

#endif // _FLOW_H

