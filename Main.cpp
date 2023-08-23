/*
  ==============================================================================

    ROLAND MSQ-100
      Q1 SysEx Sequencer Data converter
 
      by Michael T. Lauter
      last update 27 March 2013

  ==============================================================================
*/
//#include <string.h>
#include <iostream>
#include <fstream>
using namespace std;

#include "../JuceLibraryCode/JuceHeader.h"
//#include "juce_MidiFile.h"
#include "MSQ_100.h"



//==============================================================================
int main (int argc, char* argv[])
{
    int direction = 1, src_track = 0;
    char c;
    char* k;

    unsigned long filter_options = FILTER_OPT_CLEAR;
    unsigned int filt_chan = 0;
    
    short n_timebase = 120;  // Default PPQN only used for reading from MSQ SysEx
    bool cmd_error = FALSE;
    

    MSQ_100_SysEx *my_msq_sysex;
    String srcfile;
    String destfile;

    std::cout << "\nMSQ-100 SysEx Converter! v0.33 (beta) by Michael Lauter - www.lauterzeit.com/msq\n\n";
  
    int ai = 0;
    if ( argc > 1 )
    {
        srcfile = String (argv[1]);
        ++argv;
        ai++;
    }
    else
    {
        cmd_error = TRUE;
    }
    srcfile.trim();
    src_track = 1;
    
    while ( ( ++ai < argc ) && !cmd_error )
    {
        if ( (*++argv)[0] == '-' )
        {
            while ( (c = *++argv[0]) && !cmd_error )
            {
                k = (argv+1)[0];
                //ai++;
                
                switch (c)
                {
                    case 't':
                        if( ai < argc)
                            src_track = std::atoi( k );
                        break;
                        
                    case 'q':
                        if( ai < argc)
                            n_timebase = std::atoi( k );
                        
                        if (n_timebase < 96)
                            n_timebase = 96;
                        else if(n_timebase > 960)
                            n_timebase = 960;
                        else if (n_timebase % 24)
                            n_timebase = 24 * (n_timebase / 24);
                        break;
                        
                    case 'f':
                        if( ai >= argc) break;
                        
                        
                        // k = (argv+1)[0];

                        while ( (*k != '\0') && !cmd_error)
                        {
                            // parse filter options
                            switch (*k)
                            {
                                case 'p':
                                case 'P':
                                    filter_options |= FILTER_OPT_PRGCHNG;
                                    break;
                                    
                                case 'a':
                                case 'A':
                                    filter_options |= FILTER_OPT_AFTRTCH;
                                    break;
                                    
                                case 'b':
                                case 'B':
                                    filter_options |= FILTER_OPT_PTCHBND;
                                    break;
                                    
                                case 'l':
                                case 'L':
                                    filter_options |= FILTER_OPT_CCNTRLS;
                                    break;
                                    
                                case 'c':
                                case 'C':
                                    filter_options |= FILTER_OPT_CHNMUTE;
                                    break;
                                    
                                case 'x':
                                case 'X':
                                    filter_options |= FILTER_OPT_CHNSOLO;
                                    break;
                                    
                                case '0':
                                case '1':
                                case '2':
                                case '3':
                                case '4':
                                case '5':
                                case '6':
                                case '7':
                                case '8':
                                case '9':
                                    filt_chan = 10 * filt_chan + (*k - '0');
                                    break;
                                    
                                default:
                                    cmd_error = TRUE;
                                    break;
                            }
                            ++k;
                        }
                        break;
                        
                    default:
                        cmd_error = TRUE;
                        break;
                }
            }
        }
    }
    
    // for debug in check < 1, but should look for == 1
    if ( cmd_error || argc == 1 || !srcfile.isNotEmpty())
    {
        std::cout << "Usage: msqconvert sourcefile[.mid | .syx] [-t track] [-q PPQN] [-f filters]\n\n"
        "  msqconvert will translate a Standard MIDI File to\n"
        "  Roland MSQ-100 SysEx sequencer data.\n\n"
        "  If sourcefile is .mid then a target file will be\n"
        "  created having the same name appended with _msq.syx\n"
        "  If sourcefile is .syx then the reverse conversion is\n"
        "  performed, whereby the -q option sets the PPQN (timebase)\n"
        "  for the new MIDI file.  If converting FROM Format 1 MIDI\n"
        "  file, then the -t option specifies track num.\n"
        "  The -f option invokes message filtering as follows:\n"
        "      p = program change and bank select messages\n"
        "      l = controllers change\n"
        "      a = channel and/or polyphonic aftertouch\n"
        "      b = pitch bend\n"
        "      c = channel (mute)\n"
        "      x = all except channel (solo)\n\n"
        "Examples:\n"
        "  msqconvert my_song.mid -t 3 -f pax14\n"
        "      which converts only track 3 and filters\n"
        "      program changes, aftertouch and all\n"
        "      channel messages except Ch. 14\n"
        "      Output file is my_song_msq.syx\n\n"
        "  msqconvert my_step_seq.syx -q 480 -f c3\n"
        "      which reverse converts without Ch. 3\n"
        "      to std. Midi at 480 PPQN.  If -t is omitted,\n"
        "      then the default of 120 PPQN is used\n"
        "      Output file is my_step_seq_qsm.mid\n\n";
        
        return (0);
    }

    if ( filter_options & (FILTER_OPT_CHNMUTE | FILTER_OPT_CHNSOLO) );
        filter_options |= (FILTER_CHAN_MASK & (unsigned)filt_chan);
    
    my_msq_sysex = new MSQ_100_SysEx();
    
    const File sourceDirectory (File::getCurrentWorkingDirectory());
    const File destDirectory (File::getCurrentWorkingDirectory());
    
    if ( srcfile.endsWithIgnoreCase(".mid") )
    {
        srcfile = srcfile.dropLastCharacters(4);
        direction = 1;  // forward
    }
    else if ( srcfile.endsWithIgnoreCase(".syx") )
    {
        srcfile = srcfile.dropLastCharacters(4);
        direction = 0; // reverse
    }
    else
    {
        if (sourceDirectory.getChildFile(srcfile).withFileExtension(".mid").exists())
            direction = 1;
        else if (sourceDirectory.getChildFile(srcfile).withFileExtension(".syx").exists())
            direction = 0;
        else
        {
            direction = -1;  // error
            std::cout << "Couldn't find "
            << srcfile << std::endl << std::endl;
        }
    }
    
    if (direction != -1)
    {
        std::cout << "Timebase set to " << n_timebase << " PPQN" << std::endl;
    }
    
    if (direction == 1)
    {
        // read SMF and write MSQ-100 SysEx
        
        //destfile = String ("test_out_msq");
        String destfile = String (srcfile.unquoted());
        destfile.append("_msq.syx", 8);
        destfile.trim();
        srcfile.append(".mid", 4);

        const File std_midi_file (sourceDirectory.getChildFile(srcfile).withFileExtension(".mid"));
        ScopedPointer <FileInputStream> std_midi_stream (std_midi_file.createInputStream());
        
        if (std_midi_stream == 0)
        {
            std::cout << "Couldn't open "
            << srcfile << " for reading" << std::endl << std::endl;
        }
        else
        {
            const File sysex_file (destDirectory.getChildFile(destfile).withFileExtension(".syx"));
            sysex_file.deleteFile();
            ScopedPointer <FileOutputStream> sysex_stream (sysex_file.createOutputStream());
            
            // ScopedPointer <MSQ_100_SysEx> my_msq_sysex;
            my_msq_sysex->readFrom(*std_midi_stream);
            
            if (my_msq_sysex->getNumTracks())
            {
                // if at least one track then use the first
                // my_msq_sysex->getTrack(1);
                // future options: merge tracks
                if (my_msq_sysex->getNumTracks() > 1)
                {
                    // MIDI Fromat 1
                    if ( src_track >= my_msq_sysex->getNumTracks() )
                        src_track = 1;
                    my_msq_sysex->mergeTimeSig(src_track);
                }
                else
                {
                    // MIDI Fromat 0
                    src_track = 0;
                }
                
                // if timebase is different, change to 120 PPQN for MSQ-100
                if (my_msq_sysex->getTimeFormat() != 120)
                    my_msq_sysex->changePPQN((short) 120);
                
                my_msq_sysex->smf_to_msq_syx(src_track, filter_options);
                
                my_msq_sysex->write_RawSysEx(*sysex_stream);
            }
            
            cout << "Std. MIDI File converted to MSQ-100 SysEx\n";
        }
    }
    else if (direction == 0)
    {
        // read MSQ-100 SysEx and write to Standard Midi File
        
        destfile = String (srcfile.unquoted());
        destfile.append("_qsm.mid", 8);
        destfile.trim();
        srcfile.append(".syx", 4);
        
        const File sysex_file (sourceDirectory.getChildFile(srcfile).withFileExtension(".syx"));
        ScopedPointer <FileInputStream> sysex_stream (sysex_file.createInputStream());
        
        if (sysex_stream == 0)
        {
            std::cout << "Couldn't open "
            << srcfile << " for reading" << std::endl << std::endl;
        }
        else
        {
            const File std_midi_file (destDirectory.getChildFile(destfile).withFileExtension(".mid"));
            std_midi_file.deleteFile();
            ScopedPointer <FileOutputStream> std_midi_stream (std_midi_file.createOutputStream());
            
            // ScopedPointer <MSQ_100_SysEx> my_msq_sysex;

            // read the .SYX file
            my_msq_sysex->read_RawSysEx(*sysex_stream);
 
            // try to make MODE 0 Standard Midi File
            my_msq_sysex->msq_syx_to_smf(filter_options);
        
            // change to new PPQN - 96 is default for MC-500/300/50s and Ableton
            if (n_timebase != 120)
                my_msq_sysex->changePPQN((short) n_timebase);
        
            // Write the .MID file
            my_msq_sysex->writeTo(*std_midi_stream);
        
            cout << "MSQ-100 SysEx converted to Std. MIDI File, Format 0\n";
        }
    }

    delete my_msq_sysex;
    
    return 0;
}
