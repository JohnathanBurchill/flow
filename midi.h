#ifndef _MIDI_H
#define _MIDI_H

#include "flow.h"

#include <stdio.h>

#define NOTE_MAX_SPEED 127
#define MIDI_CHUNK_ALLOCATION_INCREMENT 1024
#define MIDI_CHANNELS 16
#define MIDI_NOTE_RANGE 127

#define NOTE_DYNAMICS_POINTS 41

// TODO check behaviour

enum MidiStatus
{
    MIDI_OK = 0,
    MIDI_FILE,
    MIDI_READ,
    MIDI_MEMORY,
    MIDI_ARG
};

enum MidiStatusByte
{
    CONTROLMASK = 0xf0,
    CHANNELMASK = 0x0f
};

enum MidiChannelMessages
{
    NOTEOFF = 0x80,
    NOTEON = 0x90,
    POLYKEYPRESSURE = 0xA0,
    CONTROLCHANGE = 0xB0,
    CHANNELMODE = 0b10111000,
    PROGRAMCHANGE = 0xC0,
    CHANNELPRESSURE = 0xD0,
    PITCHBEND = 0xE0,
    SYSTEMMSG = 0xF0
};

typedef struct TempoPoint
{
    double time;
    uint64_t ticks;
    uint64_t tempo;
} TempoPoint;

typedef struct NoteDynamics
{
    double x[NOTE_DYNAMICS_POINTS];
    double y[NOTE_DYNAMICS_POINTS];
    double vx[NOTE_DYNAMICS_POINTS];
    double vy[NOTE_DYNAMICS_POINTS];
    double mass;

} NoteDynamics;


typedef struct MidiNote
{
    int note;
    uint64_t startDeltaTick;
    uint64_t stopDeltaTick;
    uint64_t startTick;
    uint64_t stopTick;
    double startTime;
    double stopTime;
    double length;
    int speed;
    double screenTime;
    bool playing;
    int channel;
    int channelMode;
    bool isTempo;
    bool isPedal;
    uint32_t tempo;
    NoteDynamics dynamics;
    char *message;
    void *referenceMidiNote;
    
} MidiNote;

typedef struct MidiTrack
{
    MidiNote *notes;
    bool tempoTrack;
    bool transportTrack;
    int nNotes;
    int allocatedNotes;
    int instrument;
    char *trackName;
    char *copyright;
    char *text;
    char *instrumentName;
    char *lyric;
    char *marker;
    char *cuePoint;
    char *sequencerMetaData;
} MidiTrack;

typedef struct MidiSong
{
    MidiTrack *tracks;
    int format;
    int nTracks;
    int division;

    char *songName;
    char *filename;
    double tempo;
    double currentTempo;
    int timeSignatureTop;
    int timeSignatureBottom;

    double maxTime;
    int minNote;
    int maxNote;
    int noteSpan;
    
} MidiSong;


typedef struct MidiChunk
{
    uint8_t hdr[5];
    uint32_t length;
    uint8_t *data;
} MidiChunk;

struct State;

int readMidi(State *state);

int readMidiChunk(FILE *f, MidiChunk *chunk);

int readMidiU32(FILE *f, uint32_t *length);

int readMidiU16(FILE *f, uint16_t *length);

size_t readVariableLengthQuantity(uint8_t *data, int *offset);

int getTrackEvent(MidiTrack *track, uint8_t *data, int length, int *trackByte, uint64_t *currentTick);

int addNote(MidiTrack *track);

double songTime(MidiSong *song, uint64_t currentTick);

void setNoteTimes(MidiSong *song);


#endif // _MIDI_H
