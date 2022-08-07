/*
  Organ Serial to MIDI (over USB) converter.

  Receives from serial port 1, sends to the USB port as MIDI.
  
*/
#include "MIDIUSB.h"
#define ORGAN_BAUD 76800 

#define BUFFSIZE 40
#define HEADER_SIZE 5

// define the number of keys in each organ keyboard (manual)
#define MANUAL_SIZE 61
// define the number of pedals in the pedal board
#define PEDAL_SIZE  30 
#define SIZE_STOPS  63
// define the number of bits that map to note on/off messsages (keys, pedals and stops)
#define SIZE_NOTES  (3 * MANUAL_SIZE + PEDAL_SIZE + SIZE_STOPS)
#define MIDI_MIDDLE_C 60
#define OCTAVE_SEMITONES 12

unsigned char buffer[BUFFSIZE];
unsigned int  buffpos;
unsigned int  headerBytesToRead;
unsigned int  groupOffset;
bool          started;

void setup() {
  // initialize hardware serial port being used to receive organ data 
#ifdef CORE_TEENSY
  Serial1.begin(ORGAN_BAUD,SERIAL_8N1_RXINV);
#else
  Serial1.begin(ORGAN_BAUD);
#endif  
  memset(buffer,0,BUFFSIZE);
  buffpos=0;
  headerBytesToRead = HEADER_SIZE;
  started=false;
}

// send a 3-byte midi message
void midiCommand(byte cmd, byte data1, byte  data2) {
  // First parameter is the event type (top 4 bits of the command byte).
  // Second parameter is command byte combined with the channel.
  // Third parameter is the first data byte
  // Fourth parameter second data byte

  midiEventPacket_t midiMsg = {(byte)(cmd >> 4), cmd, data1, data2};
  MidiUSB.sendMIDI(midiMsg);
}



void processBitChange( unsigned int bit, bool onNotOff)
{
    byte chan,note;
    if (bit < (3 * MANUAL_SIZE))
    {
         chan = bit / MANUAL_SIZE;
         note = (bit % MANUAL_SIZE)  + MIDI_MIDDLE_C - (2 * OCTAVE_SEMITONES);
    }
    else if ( bit < 3 * MANUAL_SIZE + PEDAL_SIZE)
    {
        chan = 3;
        note = bit - (3*MANUAL_SIZE)  + MIDI_MIDDLE_C - (2 * OCTAVE_SEMITONES);
    }
    else
    {
        chan = 4;
        note = bit -(3*MANUAL_SIZE + PEDAL_SIZE);      
    }

    if (bit < SIZE_NOTES ) midiCommand ( (onNotOff?0x90:0x80) + chan, note, 0x7f);
    
}


void loop() {
  // read from port 1, send to port 0:
  if (Serial1.available()) 
  {
    int inByte  = Serial1.read();
    if (headerBytesToRead > 0)
    {
        if (inByte==0xff) headerBytesToRead--;
        else headerBytesToRead = HEADER_SIZE;
        buffpos=0;
        groupOffset=0;
    }
    else
    {
        if (4==groupOffset)
        {
            groupOffset=0;
        }
        else
        {
            byte oldByte = buffer[buffpos];
            byte diff = inByte ^ oldByte;
            if (diff != 0 && started)
            {
                unsigned int bitPos = buffpos << 3; 
                byte mask; 
                for (mask = 1; mask !=0 ; mask <<=1)
                {
                    if ( (diff & mask) !=0)
                    {
                        processBitChange( bitPos, (inByte & mask)!=0);
                    }
                    bitPos++;
                }              
            }
            buffer[buffpos++]=inByte;
            groupOffset++;
            if (buffpos >= BUFFSIZE) 
            {
              headerBytesToRead=HEADER_SIZE;
              started=true;
            }
        }       
    }
  }
}
