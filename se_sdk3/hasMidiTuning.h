#pragma once
//#include "mp_sdk_stdint.h"
#include <assert.h>
#include "../se_sdk3/mp_midi.h"

typedef int64_t timestamp_t;

class hasMidiTuning
{
public:
	hasMidiTuning()
    {
	    for( int i = 0 ; i < 128 ; i++ )
	    {
		    tuningTable[i] = i << 14;
	    }
    }
	virtual ~hasMidiTuning() {}

	bool onMidiMessage(int p_clock, const unsigned char* midiMessage, int /* size */)
	{
		if (midiMessage[0] == GmpiMidi::MIDI_SystemMessage &&
			(midiMessage[1] == GmpiMidi::MIDI_Universal_Realtime || midiMessage[1] == GmpiMidi::MIDI_Universal_NonRealtime) &&
			midiMessage[3] == GmpiMidi::MIDI_Sub_Id_Tuning_Standard)
		{
			OnMidiTuneMessage(p_clock, midiMessage);
			return true;
		}
		return false;
	}
	
	// Clock can be absolute or block-relative depending on caller's conventions. It's not used, only passed back via OnKeyTuningChanged().
	inline void OnMidiTuneMessage(int p_clock, const unsigned char* midi_bytes)
	{
		OnMidiTuneMessageA(static_cast<timestamp_t>(p_clock), midi_bytes);
	}

	void OnMidiTuneMessageA(timestamp_t p_clock, const unsigned char* midi_bytes)
    {
	    int tuningCount = 0;
	    int tuningDataPosition = 0;
	    bool entireBank = false;
	    bool retunePlayingNotes = true;

	    //#if defined( _DEBUG )
	    //	for( int i = 0 ; midi_bytes[i] != 0xF7 ; i++ )
	    //	{
	    //		_RPT1(_CRT_WARN, " %02X", (int) midi_bytes[i] );
	    //	}
	    //	_RPT0(_CRT_WARN, "\n");
	    //#endif
	    switch( midi_bytes[4] )
	    {
	    case 1:
		    /*
		    [BULK TUNING DUMP]
		    A bulk tuning dump comprises frequency data in the 3-byte format outlined in section 1, for all 128 MIDI key numbers, in order from note 0 (earliest sent) to note 127 (latest sent), enclosed by a system exclusive header and tail. This message is sent by the receiving instrument in response to a tuning dump request.

		    F0 7E <device ID> 08 01 tt <tuning name> [xx yy zz] ... chksum F7

		    F0 7E  Universal Non-Real Time SysEx header
		    <device ID>  ID of responding device
		    08  sub-ID#1 (MIDI Tuning)
		    01  sub-ID#2 (bulk dump reply)
		    tt  tuning program number (0 – 127)
		    <tuning name>  16 ASCII characters
		    [xx yy zz]  frequency data for one note (repeated 128 times)
		    chksum  cchecksum (XOR of 7E <device ID> nn tt <388 bytes>)
		    F7  EOX

			    */
		    {
			    tuningCount = 128;
			    entireBank = true;
			    tuningDataPosition = 22;
		    }
		    break;

	    case 2: // SINGLE NOTE TUNING CHANGE (REAL-TIME)
		    {
			    tuningCount = midi_bytes[6];
			    tuningDataPosition = 7;
		    }
		    break;

	    case 3: // BULK TUNING DUMP REQUEST (BANK)
		    break;

	    case 4: // KEY-BASED TUNING DUMP
		    {
			    tuningCount = 128;
			    entireBank = true;
			    tuningDataPosition = 23;
		    }
		    break;

	    case 7: // SINGLE NOTE TUNING CHANGE (REAL/NON REAL-TIME) (BANK)
		    {
			    /*
			    This is identical to the current SINGLE NOTE TUNING CHANGE (REAL-TIME)
			    except for the addition of the bank select byte (bb) and the change to
			    a NON REAL -TIME header. This message allows the sender to specify a new
			    tuning change that will NOT update the currently sounding notes. 
			    */
			    tuningCount = midi_bytes[7];
			    tuningDataPosition = 8;
			    retunePlayingNotes = false;
		    }
		    break;
	    }

	    const unsigned char* tuneEntry = & (midi_bytes[tuningDataPosition] );

	    for( int c = 0 ; c < tuningCount ; ++c )
	    {
		    int midiKeyNumber;
		    if( entireBank )
		    {
			    midiKeyNumber = c;
		    }
		    else
		    {
			    midiKeyNumber = *tuneEntry++;
		    }
    		
		    int tune = (tuneEntry[0] << 14) + (tuneEntry[1] << 7) + tuneEntry[2];

		    const int noChange = 0x1FFFFF;

		    if( tuningTable[midiKeyNumber] != tune && tune != noChange )
		    {
			    assert( midiKeyNumber >= 0 && midiKeyNumber < 128 );
			    tuningTable[midiKeyNumber] = tune;
			    //			_RPT2(_CRT_WARN, "%d, %x\n", (int) midiKeyNumber, (int) tune );
			    if( retunePlayingNotes )
			    {
					OnKeyTuningChangedA( p_clock, midiKeyNumber, tune );
			    }
		    }

		    tuneEntry += 3;
	    }
    }
	float GetKeyTune( int midiKeyNumber )
    {
		constexpr float c = 1.0f / (float) 0x4000; // convert 7.14 bit number to float.
	    return (float) tuningTable[midiKeyNumber] * c;
    }
	void SetKeyTune(int midiKeyNumber, float semitones)
	{
		// convert 7.14 bit number from float.
		tuningTable[midiKeyNumber] = static_cast<int>(semitones * (float)0x4000);
	}

    int GetIntKeyTune( int midiKeyNumber )
    {
	    return tuningTable[midiKeyNumber];
    }
	virtual void OnKeyTuningChanged(int /* blockrelativetimestamp */, int /* MidiKeyNumber */, int /* tune */) {}
	virtual void OnKeyTuningChangedA(timestamp_t absolutetimestamp, int MidiKeyNumber, int tune)
	{
		// call deprecated version by default to support older clients.
		OnKeyTuningChanged(static_cast<int>(absolutetimestamp), MidiKeyNumber, tune);
	}

protected:
	int tuningTable[128];
};
