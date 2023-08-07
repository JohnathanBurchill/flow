
#include "physics.h"
#include "midi.h"

int initializeNoteDynamics(State *state, MidiNote *note, int noteSpan, int minNote)
{
    if (state == NULL || state->song == NULL || note == NULL)
        return PHYSICS_ARG;


    NoteDynamics *d = &note->dynamics;
    d->mass = (double)note->speed / 127.0;
    note->length = note->stopTime - note->startTime;

    double yStart =  (int)((note->screenTime / state->windowTimeSpan ) * state->videoState.frameHeight);
    double yStop = yStart - (int) ((note->length / state->windowTimeSpan) * state->videoState.frameHeight);
    double length = yStart - yStop;

    for (int i = 0; i < NOTE_DYNAMICS_POINTS; i++)
    {
        d->x[i] = ((double)(note->note - minNote) / (double)noteSpan * state->videoState.frameWidth);
        d->y[i] = yStart - length * ((double) i / (double)(NOTE_DYNAMICS_POINTS - 1));

        d->vx[i] = 0;
        d->vy[i] = state->videoState.frameHeight / state->windowTimeSpan;
    }

    return PHYSICS_OK;
}

void updateNoteDynamics(State *state, MidiNote *note, int trackNumber, double framePeriod, double videoTime, MidiNote *pedal)
{
    if (state == NULL || state->song == NULL || note == NULL)
        return;

    NoteDynamics *d = &note->dynamics;

    double ax = 0.0;
    double yfraction = 0.0;

    double rMax = (double)RAND_MAX;
    double a = 0;
    double acceleration = 10.0 * state->noteAcceleration * state->videoState.frameHeight / state->windowTimeSpan / state->windowTimeSpan;

    double wiggleWavelenthPixels = state->wiggleWavelength * (double) state->videoState.frameHeight;

    double extraSustain = 0.0;
    double noteExtension = 0.0;
    if (pedal != NULL)
    {
        extraSustain = pedal->stopTime - videoTime;
        noteExtension = pedal->stopTime - note->stopTime;
    }
    if (extraSustain < 0.0)
        extraSustain = 0.0;
    if (noteExtension > 10.0)
        noteExtension = 10.0;

    int extInd = 0;

    for (int u=0; u < NOTE_DYNAMICS_POINTS; u++)
    {
        // Update vy and vx
        if (d->y[u] >= 0)
        {
            extInd++;
            d->vy[u] += acceleration * framePeriod;
            yfraction = d->y[u]/state->videoState.frameHeight;
            ax = 0;
            xAcceleration(state, d->y[u], videoTime, &ax);
            ax *= 1.0 * state->flowShearScale * sin(2.0 * 3.14159 * (double) note->note / (double) state->song->noteSpan);
            // ax *= 1.0 * state->flowShearScale * sin(2.0 * 3.14159 * (double) trackNumber / (double) state->song->nTracks);
            ax += 0.1 * state->videoState.frameWidth * state->wiggleAmplitude * yfraction * yfraction * sin(2. * 3.14159 * (d->y[u] / wiggleWavelenthPixels - (videoTime - note->startTime) / state->wigglePeriod + trackNumber / state->song->nTracks)) / (d->mass > 0 ? d->mass : 1.0);
            d->vx[u] += ax * framePeriod;
        }
        else if (state->pedalModifiesNotelength && pedal != NULL && pedal->startTime <= videoTime && pedal->stopTime > videoTime && note->stopTime >= pedal->startTime && note->stopTime < pedal->stopTime && pedal->speed > 0 && extraSustain > 0)
        {
            // Extend note stop time if pedal is down
            note->stopTime += noteExtension;
            note->length = note->stopTime - note->startTime;
            // printf(" (extra sustain = %lf)", extraSustain);
            for (int v = u; v < NOTE_DYNAMICS_POINTS; v++)
            {
                double before = d->y[v];
                d->y[v] = -d->vy[v] * (double)(v - extInd) * extraSustain / (double) (NOTE_DYNAMICS_POINTS - extInd);
            }
            u = NOTE_DYNAMICS_POINTS - 1;
        }
        // Update y and x
        d->y[u] += d->vy[u] * framePeriod;
        d->x[u] += d->vx[u] * framePeriod;
    }

    return;

}

int xAcceleration(State *state, double y, double videoTime, double *acceleration)
{
    if (state == NULL || acceleration == NULL)
        return PHYSICS_ARG;

    static bool initialized = false;

    static double *xAccel = NULL;

    double rMax = (double)RAND_MAX;
    double r = 0;
    double yFrac = 0;
    int nY = state->nShearYPoints;
    int nT = state->nShearTimes;

    if (!initialized)
    {
        xAccel = calloc(nT * nY, sizeof *xAccel);

        if (xAccel == NULL)
            return PHYSICS_MEMORY;

        for (int ti = 0; ti < nT; ti++)
        {
            for (int yi = 0; yi < nY; yi++)
            {

                // Pseudo random number between -1 and 1
                r = 2.0 * rand() / rMax - 1.0;
                yFrac = (double) yi / (double) (nY - 1);
                xAccel[ti * nY + yi] = r * yFrac * yFrac;
            }
        }

        initialized = true;
    }
    // Linear interpolation of force
    double deltaY = 1.0 / (double) (nY - 1);
    double yVal = (double)y / (double) state->videoState.frameHeight / deltaY;
    int yi1 = floor(yVal);
    int yi2 = yi1 + 1;
    if (yi1 < 0)
    {
        yi1 = 0;
        yi2 = 0;
    }
    if (yi2 >= nY)
    {
        yi2 = nY - 1;
        yi1 = nY - 1;
    }

    double deltaT = 1.0 / (double) (nT - 1);
    double tVal = (double)videoTime / (double) state->song->maxTime / deltaT;
    int ti1 = floor(tVal);
    int ti2 = ti1 + 1;
    if (ti1 < 0)
    {
        ti1 = 0;
        ti2 = 0;
    }
    if (ti2 >= nT)
    {
        ti2 = nT - 1;
        ti1 = nT - 1;
    }

    double a11 = xAccel[ti1 * nY + yi1];
    double a12 = xAccel[ti1 * nY + yi2];
    double a21 = xAccel[ti2 * nY + yi1];
    double a22 = xAccel[ti2 * nY + yi2];
    
    double a1 = a11 + (tVal - (double) ti1)*(a21 - a11) / deltaT; 
    double a2 = a12 + (tVal - (double) ti1)*(a22 - a12) / deltaT; 

    double a = a1 + (yVal - (double) yi1)*(a2 - a1) / deltaY;

    *acceleration = a;

    return PHYSICS_OK;
}

