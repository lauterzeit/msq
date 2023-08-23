//
//  MSQ_100.h
//  msq_convert
//
//  Created by Michael Lauter on 3/10/13.
//  Based upon JUCE (Jules' Utility Class Extensions) library
//
//

#ifndef __msq_convert__MSQ_100__
#define __msq_convert__MSQ_100__

#include <iostream>

#include "juce_audio_basics.h"

#endif /* defined(__msq_convert__MSQ_100__) */


#define FILTER_OPT_CLEAR    0x00000000UL
#define FILTER_OPT_PRGCHNG  0x00800000UL
#define FILTER_OPT_AFTRTCH  0x00400000UL
#define FILTER_OPT_PTCHBND  0x00200000UL
#define FILTER_OPT_CCNTRLS  0x00100000UL
#define FILTER_CCNUM_MASK   0x00007F00UL
#define FILTER_OPT_CHNMUTE  0x00000080UL
#define FILTER_OPT_CHNSOLO  0x00000040UL
#define FILTER_OPT_MASK     0xFFFF80C0UL
#define FILTER_CHAN_MASK    0x0000001FUL


//==============================================================================
/**

 
    MM    MM   SSSSSS    QQQQQQ           11   00000    00000
    MMM  MMM  SS     S  QQ    QQ         111  00   00  00   00
    MM MM MM    SSSS    QQ QQ QQ   ====   11  00   00  00   00
    MM    MM  s     SS  QQ   QQQ          11  00   00  00   00
    MM    MM   SSSSSS    QQQQQQ QQ        11   00000    00000

 
 
    Roland MSQ-100 SysEx "Q1" MIDI Sequencer Data class
 
    > Manipulates MSQ-100 System Exclusive messages

    > Converts to/from MSQ-100 Q1 SysEx and Standard Midi File, MODE 0
 
    > Handles internal sequencer timebase converstion
        (MSQ-100 has 120 PPQN resolution)
 
 */
//

class MSQ_100_SysEx: public juce::MidiFile
{
public:
    MSQ_100_SysEx();

    ~MSQ_100_SysEx();

    bool isRawSysEx();

    bool validate_MSQ();      // validated MSQ-100 SysEx sequencer data
    bool is_MSQ_100();

    int smf_to_msq_syx(int trk_num, uint32_t filters);
    void msq_syx_to_smf(uint32_t filters);
    
    void mergeTimeSig(int trk_num);
    
private:
    bool valid_Q1_data;
    bool raw_sysex;
    bool standard_midi;

    int num_syx_blks;    // includes FCB and all PD blocks
    int q1_data_size;    // decoded size in bytes, not encoded size
 
    uint8_t* full_q1_data;
    
    uint32_t filt_opts;

    int insert_Q1_block_break(uint8_t* q_ptr, int* blk_count, bool track_end);
    int insert_Q1_delta(uint8_t* q_ptr, int m_delta, int to_meas_end, int* ticks_tm,  int* blk_count);
    
    //  Creates concatenated MSQ-100 Q1 FCB + PDB raw data blocks
    //  from Standard MIDI message sequence
    //
    int convert_to_Q1_data(const juce::MidiMessageSequence& m_seq);
    
    //  Parse MSQ-100's Q1 Header + Phrase Data Block (PDB)
    //  to (Standard) MIDI message sequence
    //  Q1 data block chunks must be decoded and concatenated
    //  prior to calling this
    //
    int parse_Q1_data(juce::MidiMessageSequence& m_seq);
 
    // helper methods
    int encode_7_8_size(int size);
    int decode_8_7_size(int size);
    
    int encode_7_8_bytes(uint8_t* pdata, uint8_t* raw_data, int size);
    int decode_8_7_bytes(uint8_t* raw_data, uint8_t* pdata, int size);
    uint8_t byte_checksum(uint8_t* data, int size);
};