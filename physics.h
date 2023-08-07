#ifndef _PHYSICS_H
#define _PHYSICS_H

#include "flow.h"
#include "midi.h"

enum PHYSICS_ERR {
    PHYSICS_OK = 0,
    PHYSICS_ARG,
    PHYSICS_MEMORY,
    PHYSICS_MISSING_NOTES,
    PHYSICS_EOF
};

int initializeNoteDynamics(State *state, MidiNote *note, int noteSpan, int minNote);

void updateNoteDynamics(State *state, MidiNote *note, int trackNumber, double framePeriod, double videoTime, MidiNote *pedal);

int xAcceleration(State *state, double y, double videoTime, double *acceleration);



#endif // _PHYSICS_H
