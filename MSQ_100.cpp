//
//  MSQ_100.cpp
//  msq_convert
//
//  Created by Michael T. Lauter on 3/10/13.
//
//  Roland MSQ-100 System Exclusive Sequencer Data
//  class
//


#include <stdio.h>
//#include <iomanip>
#include <string.h>
#include <cmath>

#include "AppConfig.h"
#include "MSQ_100.h"
#include "juce_MathsFunctions.h"


// Exclusive Message for MSQ-100 sequencer data
typedef union
{
    uint8_t raw[264];
    struct MSQ_SysEx_Hdr
    {
        uint8_t ex_status;    // 0xF0
        uint8_t man_id;       // 0x41 for Roland
        uint8_t funct_type;   // 0x57
        uint8_t data_type;    // 0x70 for 7-8 conversion
        uint8_t message_num;  // 0 - 127
    } field;
} msq100_sysex_hdr;

//
typedef union
{
    uint8_t raw[40];
    struct Q1_FCB
    {
        uint8_t header;        // 0xFD
        uint8_t block_type;    // 'F'
        uint8_t data_type[2];  // 'Q1'
        uint8_t file_name[30]; // 'MSQ-100.0                     ' 21 spaces
        uint8_t conductor_sw;  // 0x00 - off
        uint8_t track_num;     // 0x00 - none?
        uint8_t phrase_num[2]; // 0x01, 0x00
        uint8_t time_base;     // 0x78 - timebase = 120 PPQN
        uint8_t tempo;         // 0x64 - 100, but has no function
        uint8_t EOB[2];        // 0xFE, 0xFE
    } field;
} q1_file_ctrl_block;


typedef union
{
    uint8_t raw[4];
    struct Q1_PD_block_header
    {
        uint8_t header;        // 0xFD
        uint8_t block_type;    // 'P'
        uint8_t phrase_id[2];  // 0x00, 0x00
        // uint8_t EOB[2];
    } field;
} q1_phrase_block_hdr;


// In studying MSQ SysEx files, I hae found this is End Block is never sent.
// However, it is defined in the Roland specs.
/*
typedef union
{
    uint8_t raw[6];
    struct Q1_ED
    {
        uint8_t header;        // 0xFD
        uint8_t block_type;    // 'E'
        uint8_t data_type[2];  // 0x00, 0x00 (dummy)
        uint8_t EOB[2];        // 0xFE, 0xFE
    } field;
} q1_end_block;
 */

typedef union
{
    uint8_t raw[4];
    struct Q1_PD
    {
        uint8_t time;
        uint8_t midi_status;  //
        uint8_t key_num;
        uint8_t vel;   // only if status in 0xC0 - 0xDF
    } midi_voice;
    
} q1_phrase_data;





//
static const unsigned char msq_fcb_header[] =
{
    0xFD, 'F', 'Q', '1', 'M', 'S', 'Q', '-',
    '1', '0', '0', '.', '0', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', 0x01, 0x00, 0x78, 0x64, 0xFE, 0xFE
};



MSQ_100_SysEx::MSQ_100_SysEx()
{
    valid_Q1_data = FALSE;
    raw_sysex = FALSE;
    standard_midi = FALSE;
    num_syx_blks = 0;
    q1_data_size = 0;
    full_q1_data = new uint8_t[128*256];
}


MSQ_100_SysEx::~MSQ_100_SysEx()
{
    delete full_q1_data;
}


bool MSQ_100_SysEx::isRawSysEx()
{
    return (raw_sysex);
}


// check is we have MSQ SysEx data
bool MSQ_100_SysEx::validate_MSQ()
{
    valid_Q1_data = FALSE;
    
    return valid_Q1_data;
}


// validated MSQ-100 Q1 Sequencer data
bool MSQ_100_SysEx::is_MSQ_100()
{
    return(valid_Q1_data);
}



// Results in data stored in one track sequence
int MSQ_100_SysEx::smf_to_msq_syx(int track_num, uint32_t filters)
{
    uint8_t cksum;
    int encoded_blk_size, i;
    uint8_t* temp_msg_data;
    
    filt_opts = filters;
    
    temp_msg_data = new uint8_t[256];
    i = 0;
    
    
    if ((track_num == 0) && (getNumTracks() > 1) )
    {
        juce::MidiMessageSequence& mt = *tracks.getUnchecked (1);
        //juce::MidiMessageSequence mtps = juce::MidiMessageSequence();
        // merge Fromat 1 tracks
        int tn = getNumTracks();
        double end_zeit = 0;
        
        for (int z = 1; z < tn; z++)
        {
            juce::MidiMessageSequence& tc = *tracks.getUnchecked(z);
            if ( tc.getEndTime() > end_zeit ) end_zeit = tc.getEndTime();
        }
        
        while (--tn >= 2)
        {
            juce::MidiMessageSequence& mtps = *tracks.getUnchecked(tn);
            if ( mtps.getEndTime() < end_zeit ) continue;
            
            
            mtps.deleteSysExMessages();
            mt.addSequence(mtps, 0, 0, end_zeit);
            mt.updateMatchedPairs();
        }
        mt.sort();
        
        // clear();
        track_num = 1;
    }
    
    
    if ( getNumTracks() > 0 )
    {
        juce::MidiMessageSequence& ms = *tracks.getUnchecked (track_num);
        //ms.getTrack(1);

        // delete any SysEx in track
        ms.deleteSysExMessages();

        if ( filt_opts & FILTER_OPT_CHNMUTE )
        {
            // mute MIDI channel
            ms.deleteMidiChannelMessages( filt_opts & FILTER_CHAN_MASK );
        }
        else if ( filt_opts & FILTER_OPT_CHNSOLO )
        {
            // isolate MIDI channel
            juce::MidiMessageSequence mswp = juce::MidiMessageSequence();
            ms.extractMidiChannelMessages(filt_opts & FILTER_CHAN_MASK, mswp, TRUE);
            ms.swapWith(mswp);
        }
        
        ms.updateMatchedPairs();

        q1_data_size = convert_to_Q1_data(ms);

        // clear the file
        clear();
        standard_midi = FALSE;
        timeFormat = (short)120;
        
        juce::MidiMessageSequence msyx = juce::MidiMessageSequence();

        for (int m_id = 0; m_id < num_syx_blks; m_id++)
        {
            int k = 0;
            temp_msg_data[k++] = 0xF0;   // SysEx start
            temp_msg_data[k++] = 0x41;
            temp_msg_data[k++] = 0x57;
            temp_msg_data[k++] = 0x70;

            // build up a new message block
            temp_msg_data[k++] = (uint8_t)m_id;

            // encode
            encoded_blk_size = encode_7_8_bytes(&temp_msg_data[k],
                                                &full_q1_data[i], 217);
            i += decode_8_7_size( encoded_blk_size );
                                                  
            cksum = byte_checksum(&temp_msg_data[k], encoded_blk_size);
            
            k += encoded_blk_size;
            temp_msg_data[k++] = cksum;
            temp_msg_data[k++] = 0xF7;   // SysEx end

            // Creates a midi message from a block of data.
            // use block number as timestamp
            juce::MidiMessage mm = juce::MidiMessage (temp_msg_data, k, (double)m_id);
            msyx.addEvent(mm);
        }
        addTrack (msyx);
    }
    else
    {
        //  error
    }

    delete temp_msg_data;

    return (getNumTracks());
}


// SysEx should initially be stored in one track sequence of SysEx messages
void MSQ_100_SysEx::msq_syx_to_smf(uint32_t filters)
{
    uint8_t cksum;
    int decoded_blk_size;
    const uint8_t* syx_msg_data;

    int syx_msg_size = 0;
    int i = 0;
    
    num_syx_blks = 0;
    q1_data_size = 0;
    valid_Q1_data = FALSE;
    raw_sysex = TRUE;
    filt_opts = filters;
    
    uint8_t* temp_data = new uint8_t[256];
    
    if ( getNumTracks() > 0 )
    {
        juce::MidiMessageSequence& msyx = *tracks.getUnchecked (0);
        
        for (int m_id = 0; m_id < msyx.getNumEvents(); m_id++)
        {
            int k = 0;
            
            // if m_di < getNumEvents()
            juce::MidiMessage& mm = msyx.getEventPointer(m_id)->message;
            syx_msg_size = mm.getRawDataSize();
            syx_msg_data = mm.getRawData();
    
            //memccpy(raw_msg_data, syx_msg_data, syx_msg_size, sizeof(uint8_t));
            
            // validate message header
            if ( *(syx_msg_data++) != (uint8_t) 0xF0 ) break;
            if ( *(syx_msg_data++) != (uint8_t) 0x41 ) break;
            if ( *(syx_msg_data++) != (uint8_t) 0x57 ) break;
            if ( *(syx_msg_data++) != (uint8_t) 0x70 ) break;
            if ( *(syx_msg_data++) != (uint8_t) m_id ) break;
    
            // decode
            k = syx_msg_size - 7;
            decoded_blk_size = decode_8_7_bytes(temp_data,
                                                (uint8_t *)syx_msg_data, k);
            
            int j = 4;
            while ( (temp_data[j] != 0xFE) && (j < decoded_blk_size) )
            {
                full_q1_data[i++] = temp_data[j++];
            }
            q1_data_size += (j-4);
            
            cksum = byte_checksum((uint8_t *)syx_msg_data, k);
            
            // validate message end
            syx_msg_data += k;
            if( *(syx_msg_data++) != cksum ) break;
            if( *(syx_msg_data++) != (uint8_t ) 0xF7) break;   // SysEx end
            num_syx_blks++;

        }
        
        
        if (num_syx_blks)
        {
            valid_Q1_data = TRUE;
            //Q1 data block chunks must be decoded and concatenated
            //  prior to calling this
            
            juce::MidiMessageSequence m_std = juce::MidiMessageSequence();
            parse_Q1_data(m_std);
            
            clear();
            timeFormat = 120;
            m_std.updateMatchedPairs();
            addTrack(m_std);
        }
    }
    
    delete temp_data;
}


// inserts Q1 block break code, updates current block size
int MSQ_100_SysEx::insert_Q1_block_break(uint8_t* q_ptr, int* curr_blk_size, bool track_end)
{
    int i = 0;
    
    q_ptr[i++] = 0xFE;   // block break;
    (*curr_blk_size)++;
    
    if( ((*curr_blk_size & 0x01) != 0) && ((*curr_blk_size / 7) != 1) && !track_end )
    {
        // want even number of bytes
        q_ptr[i++] = 0xFE;   // extra break byte;
        (*curr_blk_size)++;
    }
    num_syx_blks++;

    // also start the next block's header, unless this was the last one
    if (!track_end)
    {
        q_ptr[i++] = 0xFD;
        q_ptr[i++] = 'P';  // 0x50
        q_ptr[i++] = 0x00;
        q_ptr[i++] = 0x00;
    }
    
    return(i);
}


// will break down time delta, updates current block size
int MSQ_100_SysEx::insert_Q1_delta(uint8_t* q_ptr, int m_delta, int to_meas_end, int* ticks_this_meas, int* blk_count)
{
    int i = 0;
    
    do
    {
        if ((m_delta >= to_meas_end) && (m_delta < 240))
        {
            // insert measure end MPU message
            q_ptr[i++] = (uint8_t) to_meas_end;
            q_ptr[i++] = 0xF9;
            *ticks_this_meas = 0;
            m_delta -= to_meas_end;
        }
        else if (m_delta > 239)
        {
            // insert time overflow MPU messege
            q_ptr[i++] = 0xF8;
            *ticks_this_meas += 240;
            m_delta -= 240;
        }
        else
        {
            // remaining delta
            q_ptr[i++] = (uint8_t) m_delta;
            *ticks_this_meas += m_delta;
            m_delta = 0;
        }
        if (*blk_count >= 210)
        {
            *blk_count += insert_Q1_block_break(q_ptr, blk_count, FALSE);
            // increment blk num, count
        }
    }
    while ( m_delta );
    
    return(i);
}


//  This creates concatenated Q1 FCB + PDB blocks
//  Blocks must not exceed 210 bytes, insert markers 0xFE for breaks
//  total Q1 data must not exceed 127 chunks
//  FCB will always be 42 bytes long
//  max total Q1 data = 26670 bytes
//  makes calls to helper function insert insert_Q1_block_break
//  
int MSQ_100_SysEx::convert_to_Q1_data(const juce::MidiMessageSequence& m_seq)
{
    q1_file_ctrl_block  *fcb;
    q1_phrase_block_hdr *fpd;
    int curr_block_size;
    int curr_block_start;
    bool trk_end = FALSE;
    bool sig_change_request = TRUE;
    bool sig_changed = FALSE;
    bool immediate_sig_chng = FALSE;
    
    bool filt_prog_change = TRUE;
    bool filt_contrls = TRUE;
    bool filt_aftrtch = TRUE;
    bool filt_ptchbnd = FALSE;
    
    bool mtl_debug = FALSE;
    
    int i, j;

    int curr_t_sig_numerator = 4;
    int curr_t_sig_denominator = 4;
    // int curr_meas_length = 120 * curr_t_sig_numerator;
    int curr_meas_length = (480 / curr_t_sig_denominator) * curr_t_sig_numerator;
    //int msq_tempo = 100;      // MSQ's default
  
    uint32_t midi_MPQN = (60000000 / 120);  // default
    
    //int meas_ticks = 0;
    //int last_meas_t = 0;
    int lastTick = 0;
    int last_sig_change = -480;
    int ticks_this_measure = 0;
    
    //uint8_t m_key_num;
    //uint8_t m_key_vel;

    uint8_t lastStatusByte = 0;
    bool running_stat = FALSE;
    
    
    filt_prog_change = (bool)(filt_opts & FILTER_OPT_PRGCHNG);
    filt_aftrtch = (bool)(filt_opts & FILTER_OPT_AFTRTCH);
    filt_ptchbnd = (bool)(filt_opts & FILTER_OPT_PTCHBND);
    filt_contrls = (bool)(filt_opts & FILTER_OPT_CCNTRLS);
    //filt_contrls = FALSE;
    
    // register uint8_t q_byte;  // for debugging purposes
    //m_seq.getNextIndexAtTime(double timeStamp);
    
    // First block is always the Q1 FCB (file Control Block)
    //blk_num = 0;
    num_syx_blks = 0;
    i = 0;
    curr_block_start = 0;
    
    // init, copy headers
    fcb = new q1_file_ctrl_block;
    fcb->field.header = 0xFD;
    fcb->field.block_type = 'F';     // 0x46
    fcb->field.data_type[0] = 'Q';   // 0x51
    fcb->field.data_type[1] = '1';   // 0x31
    std::strcpy((char *)fcb->field.file_name, "MSQ-100.0                     ");  // 21 spaces
    fcb->field.conductor_sw = 0x00;
    fcb->field.track_num = 0x00;
    fcb->field.phrase_num[0] = 0x01;
    fcb->field.phrase_num[1] = 0x00;
    fcb->field.time_base = 0x78;  // 120 PPQN
    fcb->field.tempo = 0x64;      // 100 BPM
    fcb->field.EOB[0] = 0xFE;
    fcb->field.EOB[1] = 0xFE;
    
    curr_block_size = sizeof(q1_file_ctrl_block);
    std::memcpy(full_q1_data, fcb, curr_block_size);  //??
    i = curr_block_size;
    delete fcb;
    num_syx_blks++;
    
    fpd = new q1_phrase_block_hdr;
    fpd->field.header = 0xFD;
    fpd->field.block_type = 'P';  // 0x50
    fpd->field.phrase_id[0] = 0x00;
    fpd->field.phrase_id[1] = 0x00;
        
    // all following blocks are Q1 PD (Phrase Data) chunks
    
    curr_block_size = 0;  // reset block size
    curr_block_start = i;
    
    // each Q1 Phrase Block header is always 4 bytes
    std::memcpy(&full_q1_data[i], fpd, sizeof(q1_phrase_block_hdr));

    i += sizeof(q1_phrase_block_hdr);
    delete fpd;
    
    // need to insert special fundtion at beginning of first phrase block
    full_q1_data[i++] = 0x00;
    full_q1_data[i++] = 0xFA;  // special function
    full_q1_data[i++] = 0x01;
    full_q1_data[i++] = 0x7F;  // switch to maintain Note On Velocity
    
    juce::MidiMessageSequence sig_chngs = juce::MidiMessageSequence();
    findAllTimeSigEvents(sig_chngs);
    
    
    j = 0;
    
    // move through Midi Message Sequence for events to convert
    while ( (j < m_seq.getNumEvents()) && !trk_end )
    {
        const juce::MidiMessage& mm = m_seq.getEventPointer(j++)->message;
        int delta;
        
        if (mm.isEndOfTrackMetaEvent() || (num_syx_blks > 126))
        {
            trk_end = TRUE;
        }

        else if( mm.isMetaEvent() && !trk_end )
        {
            if ( mm.isTimeSignatureMetaEvent() )
            {
                mm.getTimeSignatureInfo (curr_t_sig_numerator, curr_t_sig_denominator);
                /*
                if (ticks_this_measure == curr_meas_length)
                {
                    // reset measure
                    ticks_this_measure = 0;
                }
                */
                
                //curr_meas_length = 120 * curr_t_sig_numerator;
                // at measure end?
                //if ( !sig_changed || ticks_this_measure )
                if ( !sig_changed && (last_sig_change != juce::roundToInt (mm.getTimeStamp()) ) )
                {                    
                    sig_change_request = TRUE;
                }
                else
                {
                    // in case of duplicate change in same measure, ignore
                    sig_change_request = FALSE;
                    continue;
                }
            }
            else if ( mm.isTempoMetaEvent() )
            {
                midi_MPQN = mm.MidiMessage::getTempoMicroSecondsPerQuarterNote();
                
                // MSQ doesn't store tempo changes
                continue;
            }
            else
            {
                // ignore other types of Meta Events
                continue;
            }
        }
        else if( (mm.isProgramChange() || mm.isControllerOfType(0x00)) && filt_prog_change )
        {
            continue;
        }
        else if ( (mm.isController() && !mm.isControllerOfType(0x01)) && filt_contrls )
        {
            continue;  // let's mod wheel pass
        }
        else if ( mm.isPitchWheel() && filt_ptchbnd )
        {
            continue;
        }
        else if ( (mm.isAftertouch() || mm.isChannelPressure()) && filt_aftrtch )
        {
            continue;
        }
        
        const int tick = juce::roundToInt (mm.getTimeStamp());
        delta = juce::jmax (0, tick - lastTick);
        lastTick = tick;
        
        if ( (lastTick == -900) && mtl_debug )
        {
            std::cout << "time = " << lastTick << std::endl;
        }
        
        if (trk_end && (ticks_this_measure % curr_meas_length))
        {
            // complete last measure
            delta = curr_meas_length - ticks_this_measure;
        }

        bool processed_delta = FALSE;
        do
        {
            // break delta into muliple parts
            const int to_meas_end = curr_meas_length - ticks_this_measure;

            if (mm.isNoteOff() && (delta == to_meas_end) && (delta < 240))
            {
                // place Note Off messages before Measure Change
                ticks_this_measure += delta;
                processed_delta = TRUE;
            }
            else if ((delta >= to_meas_end) && (to_meas_end < 240))
            {
                // insert measure end MPU message
                full_q1_data[i++] = (uint8_t) to_meas_end;
                full_q1_data[i++] = 0xF9;
                
                sig_changed = FALSE;

                if (delta < 240)
                {
                    ticks_this_measure = delta - to_meas_end;
                    processed_delta = TRUE;
                }
                else
                {
                    ticks_this_measure = 0;
                }
                delta -= to_meas_end;
                
                if (mtl_debug) std::cout << " measure end";
                
                // check for signature change event at this time!
                // change code MUST occur immediately after meausre end if so
                // and before any note status
                int si = sig_chngs.getNextIndexAtTime( lastTick - to_meas_end );
                if (si != sig_chngs.getNumEvents())
                {
                    const juce::MidiMessage& msi = sig_chngs.getEventPointer(si)->message;
                    if( ((lastTick - ticks_this_measure) == (int)msi.getTimeStamp()) && msi.isTimeSignatureMetaEvent() )
                    {
                        msi.getTimeSignatureInfo (curr_t_sig_numerator, curr_t_sig_denominator);
                        immediate_sig_chng = TRUE;
                        last_sig_change = (int)msi.getTimeStamp();
                        
                        if (mtl_debug) std::cout << " and " << curr_t_sig_numerator << "/" << curr_t_sig_denominator << " sig change";
                    }
                }
                if (mtl_debug) std::cout << " at tick " << (lastTick - delta) << std::endl;
            }
            else if (delta >= 240)
            {
                // insert time overflow MPU messege
                full_q1_data[i++] = 0xF8;
                ticks_this_measure += 240;
                
                delta -= 240;
                
                if ( delta < 240 )
                {
                    
                    if ( (delta + 240) < to_meas_end )
                    {
                        ticks_this_measure += delta;
                        processed_delta = TRUE;
                    }
                }
            }
            else if (sig_change_request && (ticks_this_measure % curr_meas_length))
            {
                // insert measure end MPU message
                full_q1_data[i++] = 0x00;
                full_q1_data[i++] = 0xF9;
                
                sig_changed = FALSE;
                
                delta = 0;
                ticks_this_measure = 0;
                processed_delta = TRUE;
                
                if (mtl_debug) std::cout << "measure end at tick " << lastTick << std::endl;
            }
            else
            {
                // remaining delta, < 240
                ticks_this_measure += delta;
                if ( (ticks_this_measure % 120) != (lastTick % 120) )
                {
                    if (mtl_debug) std::cout << "ticks this measure = " << ticks_this_measure << " at time " << lastTick << std::endl;
                }
                processed_delta = TRUE;
            }

            curr_block_size = i - curr_block_start;
            if (curr_block_size >= 210)
            {
                i += insert_Q1_block_break(&full_q1_data[i], &curr_block_size, FALSE);
                curr_block_size = 4;
                curr_block_start = i-4;
            }

            if ( immediate_sig_chng )
            {
                full_q1_data[i++] = 0x00;  // always zero
                full_q1_data[i++] = 0xFA;  // special function
                full_q1_data[i++] = 0x00;  // Beats Per Measure change
                
                // considered status change for MSQ-100
                lastStatusByte = 0xFA;
                
                if (curr_t_sig_denominator == 8)
                {
                    if ( (curr_t_sig_numerator % 2) == 0)
                    {
                        curr_t_sig_numerator /= 2;
                        curr_t_sig_denominator /= 2;
                    }
                }
                else if (curr_t_sig_denominator == 16)
                {
                    if ( (curr_t_sig_numerator % 4) == 0)
                    {
                        curr_t_sig_numerator /= 4;
                        curr_t_sig_denominator /= 4;
                    }
                }
                 
                if ( (curr_t_sig_denominator == 4) && (curr_t_sig_numerator >= 1) &&  (curr_t_sig_numerator <= 8) )
                    full_q1_data[i++] = (uint8_t) curr_t_sig_numerator;
                else
                    full_q1_data[i++] = 0x00;
                
                // curr_meas_length = 120 * curr_t_sig_numerator;
                curr_meas_length = (480 / curr_t_sig_denominator) * curr_t_sig_numerator;
                
                sig_changed = TRUE;
                //sig_change_request = FALSE;
                immediate_sig_chng = FALSE;
                
                curr_block_size = i - curr_block_start;
                if (curr_block_size >= 210)
                {
                    i += insert_Q1_block_break(&full_q1_data[i], &curr_block_size, FALSE);
                    curr_block_size = 4;
                    curr_block_start = i-4;
                }
            }

            //if (delta >= to_meas_end) continue;
        }
        //while ( (delta > 239) );
        while ( !processed_delta );
        
        if( trk_end )
        {
            // data end
            full_q1_data[i++] = 0x00;
            full_q1_data[i++] = 0xFC;  // Track End MPU mark
        }
        else if( sig_change_request )
        {
            // sig_changed = TRUE;  // debug line
            if ( !sig_changed || (lastTick == 0))
            {
                full_q1_data[i++] = 0x00;  // always zero
                full_q1_data[i++] = 0xFA;  // special function
                full_q1_data[i++] = 0x00;  // Beats Per Measure change
                
                // considered status change for MSQ-100
                lastStatusByte = 0xFA;
                
                if (curr_t_sig_denominator == 8)
                {
                    if ( (curr_t_sig_numerator % 2) == 0)
                    {
                        curr_t_sig_numerator /= 2;
                        curr_t_sig_denominator /= 2;
                    }
                }
                else if (curr_t_sig_denominator == 16)
                {
                    if ( (curr_t_sig_numerator % 4) == 0)
                    {
                        curr_t_sig_numerator /= 4;
                        curr_t_sig_denominator /= 4;
                    }
                }
                
                if ( (curr_t_sig_denominator == 4) && (curr_t_sig_numerator >= 1) &&  (curr_t_sig_numerator <= 8) )
                    full_q1_data[i++] = (uint8_t) curr_t_sig_numerator;
                else
                    full_q1_data[i++] = 0x00;
                
                // curr_meas_length = 120 * curr_t_sig_numerator;
                curr_meas_length = (480 / curr_t_sig_denominator) * curr_t_sig_numerator;
                
                ticks_this_measure = 0;
                last_sig_change = lastTick;
                sig_changed = TRUE;
            }
            else
            {
                //sig_changed = FALSE;
            }
            sig_change_request = FALSE;
        }
        else
        {
            const uint8_t* data = mm.getRawData();
            int dataSize = mm.getRawDataSize();
            
            // dataSize should never exceed 3 at this point
            
            uint8_t statusByte = data[0];
            if ( (statusByte >= 0xC0) && (statusByte <= 0xDF) )
            {
                dataSize = 2;
            }
            else
            {
                dataSize = 3;
            }
            
            if(mm.isNoteOff())
            {
                // change to Note on with velocity = 0
                statusByte = 0x90 | (statusByte & 0x0F);
                //m_key_vel = 0x00;
            }
            
            if (statusByte == lastStatusByte
                && (statusByte & 0xf0) != 0xf0
                && dataSize > 1
                && j > 0)
            {
                running_stat = TRUE;
                ++data;
                --dataSize;
            }
            else
            {
                running_stat = FALSE;
                
                if (statusByte == 0xF0)  // sysex message
                {
                    // We cannot embed other devices' SysEx messages
                    // within MSQ's own SysEx sequencer data
                    // They should be filtered out before this method
                    // ignore regardless
                    continue;
                }
            }
            
            full_q1_data[i++] = (uint8_t) delta;
            while (dataSize--)
            {
                full_q1_data[i++] =  *(data++);
            }
            if(mm.isNoteOff()) full_q1_data[i-1] = 0x00;  //force key velocity zero;
            
            lastStatusByte = statusByte;
        }
        
        curr_block_size = i - curr_block_start;
        if ( trk_end || ((curr_block_size) >= 210))
        {
            i += insert_Q1_block_break(&full_q1_data[i], &curr_block_size, trk_end);
            curr_block_size = 4;
            curr_block_start = i-4;
        }
    }

    
    if (i) valid_Q1_data = TRUE;
    
    return (i);  // Q1 bytes processed
}


void MSQ_100_SysEx::mergeTimeSig(int trk_num)
{
    juce::MidiMessageSequence t_events = juce::MidiMessageSequence();

    findAllTimeSigEvents(t_events);
    
    tracks.getUnchecked(trk_num)->swapWith(t_events);
    tracks.getUnchecked(trk_num)->addSequence(t_events, 0, 0, 9600000);
    tracks.getUnchecked(trk_num)->updateMatchedPairs();
    tracks.getUnchecked(trk_num)->sort();
}



//  Parse MSQ-100's Q1 Header + Phrase Data Block (PDB)
//  to (Standard) Midi message Sequence
//  Q1 data block chunks must be decoded and concatenated
//  prior to calling this
//
int MSQ_100_SysEx::parse_Q1_data(juce::MidiMessageSequence& m_seq)
{
    int ticks_this_meas = 0;
    int last_meas_t = 0;

    int delta = 0;
    
    int last_time = 0;
    int curr_time = 0;
    int curr_t_sig = 4;
    int curr_measure_length;
    //int curr_tempo = 100;
    
    uint8_t m_key_num, m_key_vel;
    uint8_t last_status = 0xF9;
    uint8_t curr_status = 0xFA;
    //bool running_stat = FALSE;
    
    
    const unsigned char msq_track_title[] =
    {
        0xFF, 0x03, 0x10, 'M', 'S', 'Q', '-', '1',
        '0', '0', ' ', 'S', 'e', 'q', 'u', 'e', 'n', 'c', 'e'
    };
    
    int i = 0;
    register uint8_t q_byte;  // for debugging purposes
    
    // juce::MidiMessage& mm;  // temp pointer to new MIDI message

    i = 36; // skip over first 36 bytes or validate them
    
    if (valid_Q1_data)
    {
        raw_sysex = FALSE;
        m_seq.clear();
        
        const juce::MidiMessage mt = juce::MidiMessage (msq_track_title, sizeof(msq_track_title), 0.00f);
        m_seq.addEvent(mt);
        
        const juce::MidiMessage mtmpo = juce::MidiMessage::tempoMetaEvent( 60000000 / 100 );  // 100 BPM
        m_seq.addEvent(mtmpo);
        
        //const juce::MidiMessage mtsig = juce::MidiMessage::timeSignatureMetaEvent(4, 4);
        //m_seq.addEvent(mtsig);
    }
    
    
    last_time = 0;
    curr_time = 0;
    curr_measure_length = curr_t_sig * 120;
    q_byte = 0xF9;
    
    while ((q_byte != 0xFC) && (i < q1_data_size))
    {
        // get time
        q_byte = full_q1_data[i++];
        if (q_byte == 0xFE)
        {
            // just indicated end of data block, skip over next header
            q_byte = full_q1_data[i++];
            if (q_byte == 0xFE) i++;
            // i += 2;
            continue;
        }
        else if (q_byte == 0xF8)
        {
            delta = 240;  // half note, 2 X 120 PPQN
        }
        else
        {
            delta = (int)q_byte;
        }
        
        last_time = curr_time;
        curr_time += delta;
        
        ticks_this_meas += delta;
        if (i > 250)
        {
            // std::cout << " index " << i << " at time " << curr_time << std::endl;
        }
        
        if (q_byte == 0xF8) continue;
        
        // get status
        q_byte = full_q1_data[i++];
        if ((0xF0 & q_byte) == 0xF0)
        {
            // Meta type events
            if (q_byte == 0xF9)
            {
                // measure end
                if (ticks_this_meas != last_meas_t)
                {
                    // need to check time signature change
                    // creat time signature meta event
                }
                
                last_meas_t = ticks_this_meas;
                ticks_this_meas = 0;
                continue;
            }
            else if (q_byte == 0xFA)
            {
                // special functions
                q_byte = full_q1_data[i++];
                if (q_byte == 0x01)
                {
                    // switch to maintain NOTE ON Velocity ?
                    q_byte = full_q1_data[i++];
                }
                else
                {
                    // time sig change (delta) ??
                    curr_t_sig = full_q1_data[i++];
                    if (!curr_t_sig)
                        curr_t_sig = 4;
                    curr_measure_length = curr_t_sig * 120;
                    
                    // need to create Set Tempto Meta event
                    // Set tempo event MUST occur on MIDI clock grid (24 PPQN)
                    // in the MSQ-100 case (120 PPQN), the event time must be evenly
                    // divisible by 5. Any timing adjustment here require us to
                    // sort the midi message sequence at the end of this function
                    //event_time = 5 * (curr_time / 5);
                    //curr_tempo = 100; // ?
                    
                    juce::MidiMessage mtsg = juce::MidiMessage(juce::MidiMessage::timeSignatureMetaEvent(curr_t_sig, 4), (double) curr_time);
                    m_seq.addEvent(mtsg);
                }
                continue;
            }
            else if (q_byte == 0xFC)
            {
                // data end
                break;
            }
            else if (q_byte == 0xFE)
            {
                // just indicated end of data block, skip over next header
                q_byte = full_q1_data[i++];
                if (q_byte == 0xFE) i++;
                // i += 2;
                continue;
            }
        }
        else if (0x80 & q_byte)
        {
            // new status
            curr_status = q_byte;
            last_status = curr_status;

            m_key_num = full_q1_data[i++];  // key number
        }
        else
        {
            // q_data already contians key number
            // running status
            m_key_num = q_byte;
            
            if ( curr_status != last_status ) break;  // error - should never happen
        }
        
        
        if ( (curr_status >= 0xC0) && (curr_status <= 0xDF) )
        {
            // program change or channel aftertouch - one more byte only
            // Note
            const juce::MidiMessage mm = juce::MidiMessage (curr_status, m_key_num, double(curr_time));
            m_seq.addEvent(mm);
        }
        else if ( (curr_status >= 0x80) && (curr_status <= 0xEF) )
        {            
            // one more byte - key veolocity
            m_key_vel = full_q1_data[i++];
            
            uint8_t mod_status = curr_status;
            if ( (m_key_vel == 0) && ((curr_status & 0xF0) == 0x90) )
            {
                // Note Off
                mod_status &= 0xEF;
            }
            // Note
            const juce::MidiMessage mm = juce::MidiMessage (mod_status, m_key_num, m_key_vel, double(curr_time));
            m_seq.addEvent(mm);
        }
    }
    
    if ((int) curr_status)
    {
        const juce::MidiMessage mm_eot = juce::MidiMessage::endOfTrack();
        m_seq.addEvent( mm_eot );
        
        m_seq.updateMatchedPairs();
        m_seq.sort();
    }
    
    return ((int)curr_status);
}





// returns new size
// required for allocating destination buffer
//
// examples 212 -> 243
//           42 ->  48
int MSQ_100_SysEx::encode_7_8_size(int size)
{
    int remainder;
    
    remainder = size % 7;
    if (remainder) remainder++;
    
    return (8 * (size / 7) + remainder);
}


// returns new size
// works on one block at a time
// stops at EOB (End of Block) 0xFE bytes or size limit
int MSQ_100_SysEx::encode_7_8_bytes(uint8_t* block_data, uint8_t* raw_data, int size)
{
    uint8_t msig_bits;
    int i = 0; int j = 0; int k = 0;
    bool end_of_block = FALSE;
    
    while ( !end_of_block )
    {
        // break down into 7 byte chunks and encode as 8
        msig_bits = 0x00;
        for (j = 0; j < 7; j++)
        {
            if (end_of_block)
            {
                if (raw_data[i] == 0xFE)
                {
                    //  seen two End Block marks
                }
                else
                {
                    break;
                }
            }
            else if( raw_data[i] == 0xFE )
            {
                end_of_block = TRUE;
            }
            else if (i >= size)
            {
                // safety - should never get here
                end_of_block = TRUE;
                break;
            }
            
            if(0x80 & raw_data[i])
            {
                msig_bits |= (0x01 << j);
            }
            block_data[++k] = 0x7F & raw_data[i++];
        }
        block_data[k-j]  = msig_bits;
        ++k;
        
    }
    
    return(k);
}


// returns new size
// required for allocating destination buffer
//
// new size = 7 * (size / 8) + (size % 8) - 1
//
// examples 243 -> 212
//           48 ->  42
int MSQ_100_SysEx::decode_8_7_size(int size)
{
    int remainder;
    
    remainder = size % 8;
    if (remainder) remainder--;
    
    return(7 * (size / 8) + remainder);
}


// returns new size 
int MSQ_100_SysEx::decode_8_7_bytes(uint8_t* raw_data, uint8_t* block_data, int size)
{
    uint8_t msig_bits;
    int i = 0; int k = 0;
    
    while ( i < size )
    {
        // break down into 8 byte chunks and extract 7
        msig_bits = block_data[i++];
        
        for (int j = 7; j > 0 ; j--)
        {
            raw_data[k++] = (0x80 & (msig_bits << j)) | block_data[i++];

            if ( i >= size ) break;
        }
    }
    
    return(k);
}


uint8_t MSQ_100_SysEx::byte_checksum(uint8_t* block_data, int b_size)
{
    uint8_t c_sum = block_data[--b_size];
    
    while ( b_size )
        c_sum += block_data[--b_size];
    
    return (c_sum & 0x7F);
}

