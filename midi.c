#include "flow.h"
#include "midi.h"

#include <stdio.h>

// TODO handle multiple voices per track?
static MidiNote storageNotes[MIDI_NOTE_RANGE][MIDI_CHANNELS] = {0};
static MidiNote pedal[MIDI_CHANNELS] = {0};
static int currentStatusByte = 0;
static int currentChannel = 0;
static int currentChannelMode = 0;
static int currentSystemChannel = 0;

int readMidi(State *state)
{

    int status = MIDI_OK;

    FILE *f = fopen(state->audioState.midiFilename, "r");
    if (f == NULL)
    {
        return MIDI_FILE;
    }

    MidiChunk c = {0};

    // Look for header
    status = readMidiChunk(f, &c);
    if (status != MIDI_OK || strcmp("MThd", (const char *)c.hdr) != 0)
        goto done;

    MidiSong *song = calloc(1, sizeof *song);
    state->song = song;
    // Default values
    song->tempo = 500000; // micro-seconds per quarter note (= 120 beats per minute)
    song->timeSignatureTop = 4; // 4/4 time
    song->timeSignatureBottom = 4;

    if (song == NULL)
    {
        fprintf(stderr, "Unable to allocate memory for MIDI song.\n");
        status = MIDI_MEMORY;
        goto done;
    }
    song->format = c.data[0] * 256 + c.data[1];
    int nTracks = c.data[2] * 256 + c.data[3];
    // if most-sig-bit is 0, this is the number of ticks per quarter note
    // if a 1, this is more complicated...
    song->division = c.data[4] * 256 + c.data[5];
    free(c.data);

    // Read Tracks
    if (song->format == 2)
    {
        fprintf(stderr, "Cannot parse format 2 MIDI at the moment.\n");
        status = MIDI_FILE;
        goto done;
    }

    if (state->trackToDisplay != -1)
    {
        if (state->trackToDisplay == 0)
            song->nTracks = 1;
        else
            song->nTracks = 2; // Read also the tempo track
    }
    else
        song->nTracks = nTracks;

    song->tracks = calloc(song->nTracks, sizeof * song->tracks);
    MidiTrack *tracks = song->tracks;
    if (tracks == NULL)
    {
        fprintf(stderr, "Unable to allocate memory for MIDI tracks.\n");
        status = MIDI_MEMORY;
        goto done;
    }

    MidiTrack *track = NULL;
    int trackByte = 0;
    uint64_t currentTick = 0;

    if (state->verbose)
        fprintf(stdout, "Tracks found in %s\n", state->audioState.midiFilename);

    int trackInd = 0;
    for (int tr = 0; tr < nTracks; tr++)
    {
        // Read track
        status = readMidiChunk(f, &c);
        if (status != MIDI_OK || strcmp("MTrk", (const char *)c.hdr) != 0)
            goto done;

        // Parse and store track
        trackByte = 0;
        currentTick = 0;
        if ((state->trackToDisplay == -1 || state->trackToDisplay == tr || tr == 0))
        {
            track = &song->tracks[trackInd];
            while (trackByte < c.length)
            {
                status = getTrackEvent(track, c.data, c.length, &trackByte, &currentTick);
                if (status != MIDI_OK)
                {
                    if (pedal[currentChannel].playing)
                    {
                        pedal[currentChannel].stopTick = currentTick;
                        status = addNote(track);
                        if (status != MIDI_OK)
                            return status;
                        track->notes[track->nNotes-1] = pedal[currentChannel];
                        track->notes[track->nNotes-1].isPedal = true;
                    }
                    goto done;
                }
            }
            if (state->verbose)
                fprintf(stdout, "%4d   \"%s\"\n", trackInd, track->trackName);
            trackInd++;
        }

        // Free data
        free(c.data);
    }

    // Get timings
    setNoteTimes(song);

done:
    fclose(f);
    return status;
}

int readMidiChunk(FILE *f, MidiChunk *c)
{
    if (f == NULL || c == NULL)
        return MIDI_ARG;

    int status = MIDI_OK;

    size_t bytesRead = 0;
    bytesRead = fread(c->hdr, 1, 4, f);
    if (bytesRead != 4)
        return MIDI_READ;

    status = readMidiU32(f, &c->length);
    if (status != MIDI_OK)
        return status;

    c->data = calloc(c->length, sizeof *c->data);
    if (c->data == NULL)
        return MIDI_MEMORY;

    bytesRead = fread(c->data, 1, c->length, f);
    if (bytesRead != c->length)
        return MIDI_READ;

    return MIDI_OK;

}

int readMidiU32(FILE *f, uint32_t *length)
{
    if (f == NULL || length == NULL)
        return MIDI_ARG;

    uint8_t bytes[4] = {0};

    size_t nbytes = fread(bytes, 1, 4, f);
    if (nbytes != 4)
        return MIDI_READ;

    *length = bytes[3] + bytes[2] * 256 + bytes[1] * 256*256 + bytes[0] * 256 * 256 * 256;

    return MIDI_OK;
}

int readMidiU16(FILE *f, uint16_t *length)
{
    if (f == NULL || length == NULL)
        return MIDI_ARG;

    uint8_t bytes[2] = {0};

    size_t nbytes = fread(bytes, 1, 2, f);
    if (nbytes != 2)
        return MIDI_READ;

    *length = bytes[1] + bytes[0] * 256;

    return MIDI_OK;
}

int getTrackEvent(MidiTrack *track, uint8_t *data, int length, int *trackByte, uint64_t *currentTick)
{
    static int called = 0;
    int status = MIDI_OK;
    called++;
    if (track == NULL || data == NULL || trackByte == NULL || *trackByte >= length)
        return MIDI_MEMORY;

    uint8_t byte1;
    uint8_t byte2;
    void *mem = NULL;
    uint64_t nDataBytes = 0;

    // Read delta time

    uint64_t deltaTime = readVariableLengthQuantity(data, trackByte);
    (*currentTick) += deltaTime;

    // Read the event
     uint8_t midiByte = data[(*trackByte)++];

    if (midiByte & 0x80)
    {
        currentStatusByte = midiByte & CONTROLMASK;
        if (currentStatusByte != 0xF0)
            currentChannel = midiByte & CHANNELMASK;
        else
            currentSystemChannel = midiByte & CHANNELMASK;
    }
    else
    {
        // using previous control status byte (running status)
        // Back up one byte
        (*trackByte)--;
    }

    switch(currentStatusByte)
    {
        case NOTEON:
        case NOTEOFF:
            byte1 = data[(*trackByte)++];
            byte2 = data[(*trackByte)++];
            storageNotes[byte1][currentChannel].channel = currentChannel;
            // Do not add the note if it is already playing
            // Can happen with a note on a note due to playing glitch
            if (currentStatusByte == NOTEON && byte2 > 0 && !storageNotes[byte1][currentChannel].playing)
            {
                storageNotes[byte1][currentChannel].playing = true;
                storageNotes[byte1][currentChannel].startDeltaTick = deltaTime;
                storageNotes[byte1][currentChannel].startTick = *currentTick;
                storageNotes[byte1][currentChannel].speed = byte2;
            }
            else
            {
                // If the note was not playing, do not try to stop it
                if (storageNotes[byte1][currentChannel].playing)
                {
                    storageNotes[byte1][currentChannel].playing = false;
                    storageNotes[byte1][currentChannel].stopDeltaTick = deltaTime;
                    storageNotes[byte1][currentChannel].stopTick = *currentTick;
                    status = addNote(track);
                    if (status != MIDI_OK)
                        return status;
                    track->notes[track->nNotes-1] = storageNotes[byte1][currentChannel];
                    track->notes[track->nNotes-1].note = byte1;
                    storageNotes[byte1][currentChannel].startTick = 0;
                    storageNotes[byte1][currentChannel].stopTick = 0;
                }
            }
            break;

        case POLYKEYPRESSURE:
            byte1 = data[(*trackByte)++];
            byte2 = data[(*trackByte)++];
            // TODO adjust colour / opacity based on aftertouch
            break;

        case CONTROLCHANGE:
            byte1 = data[(*trackByte)++];
            byte2 = data[(*trackByte)++];
            // TODO handle control change
            if (byte1 >= 121 && byte1 <= 127)
            {
                // Channel mode change
                // TODO
            }
            else
            {
                // control change
                switch (byte1)
                {
                    case 0x40:
                        if (!pedal[currentChannel].playing)
                        {
                            pedal[currentChannel].playing = true;
                            pedal[currentChannel].startDeltaTick = deltaTime;
                            pedal[currentChannel].startTick = *currentTick;
                            pedal[currentChannel].speed = byte2;
                        }
                        else if (pedal[currentChannel].playing)
                        {
                            if (byte2 == 0)
                                pedal[currentChannel].playing = false;
                            else
                                pedal[currentChannel].playing = true;
                            
                            pedal[currentChannel].stopDeltaTick = deltaTime;
                            pedal[currentChannel].stopTick = *currentTick;
                            status = addNote(track);
                            if (status != MIDI_OK)
                                return status;
                            track->notes[track->nNotes-1] = pedal[currentChannel];
                            track->notes[track->nNotes-1].isPedal = true;

                            pedal[currentChannel].startDeltaTick = deltaTime;
                            pedal[currentChannel].startTick = *currentTick;
                            track->notes[track->nNotes-1].speed = byte2;
                        }
                    
                        break;
                    // TODO, handle others
                }
            }
            break;

        case PROGRAMCHANGE:
            byte1 = data[(*trackByte)++];
            // TODO handle program change
            break;

        case CHANNELPRESSURE:
            byte1 = data[(*trackByte)++];
            // TODO handle channel pressure / aftertouch
            break;

        case PITCHBEND:
            byte1 = data[(*trackByte)++];
            byte2 = data[(*trackByte)++];
            // TODO handle pitch bend change
            break;

        // System messages
        case SYSTEMMSG:
            if (midiByte == 0xFF)
            {
                // Meta event
                // Type
                byte1 = data[(*trackByte)++];
                switch(byte1)
                {
                    case 0x00:
                        // Sequence number
                        (*trackByte) += 3;
                        break;
                    case 0x01:
                    case 0x02:
                    case 0x03:
                    case 0x04:
                    case 0x05:
                    case 0x06:
                    case 0x07:
                    case 0x7F:
                        // Text
                        nDataBytes = readVariableLengthQuantity(data, trackByte);
                        mem = calloc(nDataBytes + 1, 1);
                        if (mem == NULL)
                            return MIDI_MEMORY;
                        switch(byte1)
                        {
                            case 0x01:
                                memcpy(mem, data + *trackByte, nDataBytes);
                                track->text = mem;
                                break;
                            case 0x02:
                                memcpy(mem, data + *trackByte, nDataBytes);
                                track->copyright = mem;
                                break;
                            case 0x03:
                                memcpy(mem, data + *trackByte, nDataBytes);
                                track->trackName = mem;
                                if (strcmp("Transport", track->trackName) == 0)
                                    track->transportTrack = true;
                                break;
                            case 0x04:
                                memcpy(mem, data + *trackByte, nDataBytes);
                                track->instrumentName = mem;
                                break;
                            case 0x05:
                                memcpy(mem, data + *trackByte, nDataBytes);
                                track->lyric = mem;
                                break;
                            case 0x06:
                                memcpy(mem, data + *trackByte, nDataBytes);
                                track->marker = mem;
                                break;
                            case 0x07:                       
                                memcpy(mem, data + *trackByte, nDataBytes);
                                track->cuePoint = mem;
                            case 0x7F:
                                memcpy(mem, data + *trackByte, nDataBytes);
                                track->sequencerMetaData = mem;
                        }
                        (*trackByte) += nDataBytes;
                        break;
                    case 0x20:
                        // MIDI channel prefix
                        (*trackByte) += 2;
                        break;
                    case 0x2F:
                        // End of track
                        (*trackByte) += 1;
                        break;
                    case 0x51:
                        // Set tempo
                        (*trackByte)++;
                        status = addNote(track);
                        if (status != MIDI_OK)
                            return status;
                        track->tempoTrack = true;
                        track->notes[track->nNotes-1].isTempo = true;
                        track->notes[track->nNotes-1].startTick = *currentTick;
                        track->notes[track->nNotes-1].tempo = data[*trackByte] * 256 * 256 + data[*trackByte + 1] * 256 + data[*trackByte + 2];
                        (*trackByte) += 3;
                        break;
                    case 0x54:
                        // SMPTE offset (start time of track)
                        (*trackByte) += 6; // Ignored
                        break;
                    case 0x58:
                        // Time Signature
                        (*trackByte) += 5; // Ignored
                        break;
                    case 0x59:
                        // Key Signature
                        (*trackByte) += 3; // Ignored
                        break;
                    default:
                        nDataBytes = readVariableLengthQuantity(data, trackByte);
                        (*trackByte) += nDataBytes;
                }

                
            }
            else if (midiByte == 0xF0)
            {
                // System Exclusive message
                while (byte1 != 0xF7)
                    byte1 = data[(*trackByte)++];
                
            }
            else if ((currentStatusByte & 0b11111000) == 0b11111000)
            {
                // 0 to two bytes, how many?
                byte1 = data[(*trackByte)++];
                byte2 = data[(*trackByte)++];
            }
            else if (midiByte <= 0xF7)
            {
                switch(midiByte)
                {
                    // Handel system exclusive message
                    case 0xF0:
                        // Read bytes
                        break;
                    case 0xF1:
                        // Midi time code quarter frame
                        byte1 = data[(*trackByte)++];
                        break;
                    case 0xF2:
                        // Song position pointer
                        // Least significant byte
                        byte1 = data[(*trackByte)++];
                        // Most significant byte
                        byte2 = data[(*trackByte)++];
                        break;
                    case 0xF3:
                        // Song select
                        byte1 = data[(*trackByte)++];
                        break;
                   case 0xF4:
                   case 0xF5:
                        // Undefined, no bytes
                        break;
                   case 0xF6:
                        // Tune request
                        break;
                   case 0xF7:
                        // EOX End of System Exclusive message
                        break;
 
                 }
            }
            else
            {
                // System real time
                switch(midiByte)
                {
                    case 0xF8:
                        // Timing clock
                        break;
                    case 0xF9:
                        // Undefined
                        break;
                    case 0xFA:
                        // Start
                        break;
                    case 0xFB:
                        // Continue
                        break;
                    case 0xFC:
                        // Stop
                        break;
                    case 0xFD:
                        // Undefine
                        break;
                    case 0xFE:
                        // Active Sensing
                        break;
                    case 0xFF:
                        // System Reset
                        break;
                }
            }
            break;


    }


    return MIDI_OK;
}

size_t readVariableLengthQuantity(uint8_t *data, int *offset)
{
    if (data == NULL || offset == NULL)
        return 0;

    int vlqByte = data[(*offset)++];
    long long vlq = 0;
    while (vlqByte & 0x80)
    {
        vlq += (vlqByte & 0x7f);
        vlq *= 128;
        vlqByte = data[(*offset)++];
    }
    vlq += vlqByte;

    return vlq;

    // register uint64_t value;
    // register uint8_t c;
    // if ((value = data[(*offset)++]) & 0x80)
    // {
    //     value &= 0x7f;
    //     do
    //     {
    //         value = (value << 7) + ((c = data[(*offset)++]) & 0x7f);
    //     } while (c & 0x80);
    // }
    // return value;
}

int addNote(MidiTrack *track)
{
    // Add the note to the track
    void *mem = NULL;
    size_t offset = 0;

    track->nNotes++;
    if (track->nNotes > track->allocatedNotes)
    {
        offset = track->allocatedNotes * sizeof *track->notes;
        track->allocatedNotes += MIDI_CHUNK_ALLOCATION_INCREMENT;
        mem = realloc(track->notes, track->allocatedNotes * sizeof *track->notes);
        if (mem == NULL)
            return MIDI_MEMORY;
        track->notes = mem;
        bzero((uint8_t*)track->notes + offset, MIDI_CHUNK_ALLOCATION_INCREMENT * sizeof *track->notes);
    }

    return MIDI_OK;
}

double songTime(MidiSong *song, uint64_t currentTick)
{
    if (song == NULL)
        return MIDI_ARG;

    double tempo = song->tempo;
    double division = (double) song->division;

    double rate = tempo / division / 1000000.0;

    // Default assumes constant tempo
    double t = rate * (double) currentTick;
    double cumulativeTime = 0.0;
    double lastTick = 0;
    double newTicks = 0;
    // If there is a tempo track, revise the track time
    if (song->nTracks > 0 && song->tracks[0].tempoTrack)
    {
        MidiTrack *track = &song->tracks[0];
        double noteTime = 0.0;
        // Sum up times over intervals of constant tempo
        // Until we get to the current tick
        // Assumes constant tempo between ticks
        for (int i = 0; i < track->nNotes; i++)
        {
            if (track->notes[i].startTick <= currentTick)
            {
                newTicks = track->notes[i].startTick - lastTick;
                lastTick = track->notes[i].startTick;
                cumulativeTime += rate * newTicks;
                tempo = (double)track->notes[i].tempo;
                rate = tempo / division / 1000000.0;
            }
            else
                break;
        }
        newTicks = currentTick - lastTick;
        cumulativeTime += rate * newTicks;
        t = cumulativeTime;
    }

    return t;
}

void setNoteTimes(MidiSong *song)
{
    if (song == NULL || song->tracks == NULL)
        return;

    MidiTrack *track = NULL;
    MidiNote *note = NULL;

    double maxTime = 0.0;
    int minNote = 255;
    int maxNote = 0;

    for (int tr = 0; tr < song->nTracks; tr++)
    {
        track = &song->tracks[tr];
        for (int n = 0; n < track->nNotes; n++)
        {
            note = &track->notes[n];
            
            if (note->startTick != 0)
                note->startTime = songTime(song, note->startTick);
            if (note->stopTick != 0)
                note->stopTime = songTime(song, note->stopTick);

            if (!track->tempoTrack && !note->isPedal)
            {
                if (note->note > maxNote)
                    maxNote = note->note;
                if (note->note < minNote)
                    minNote = note->note;
                if (note->startTime > maxTime)
                    maxTime = note->startTime;
                if (note->stopTime > maxTime)
                    maxTime = note->stopTime;
            }

        }
    }
    song->maxTime = maxTime;
    song->minNote = minNote;
    song->maxNote = maxNote;

    return;
}
