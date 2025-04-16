# RME TotalMix FX OSC Command List

*Related TotalMix FX Version: 1.96, 22.07.2024*

## Comments

The OSC functionality in TotalMix was developed in 2014 for communication with "old" TouchOSC in a Mackie-Protocol style.

This concept has a mixer-bank style and is freely assignable to large matrix mixers (like TotalMix). Resulting from this is a control-element-orientated addressing with dynamic mapping to certain channels, depending on bank assignment.

Bank start, bus selection and submix selection can be controlled by buttons to navigate in the mixer matrix - like with MIDI Mackie Control. Single-channel settings (Page 2 and 4) are addressed by a "single channel offset" to the bank start.

Many labels were assigned to match the MultiPush elements of Touch OSC.

Later some "none-paged" parameters were attached to allow direct control of submix, bank-start and single-channel-page-offset in the bank. "Page" selection is mainly relevant for parameter re-sending: selecting a new page by sending any parameter containing a new page number triggers a complete re-send of all parameters of the newly selected page by TotalMix.

## Table Columns Description

- **Name:** string label for OSC-Command, extended by page prefix, e.g. `/1/bank+`; none paged parameters only, e.g. `/setSubmix`
- **Function Type:** short description of controlled TotalMix function
- **Scale Type Parameter:** scaling of received and sent back values (see list below)
- **Scale Type Display Value:** scaling of an additional label, sent only by TotalMix, with postfix `Val`, e.g. `/1/volume1Val` (see list of scale types below)
- **Channel:** Channel offset for multichannel functions (Page 1)

## Scale Types

- **KOSCScaleNoSend:** no value sent by TotalMix FX (e.g. for navigation buttons or if there is no additional Display Value)
- **KOSCScaleLin01:** linear receive/send value, scaled between 0.0 and 1.0
- **KOSCScaleFader:** receive/send value between 0.0 and 1.0 scaled in fader curve
- **KOSCScaleFreq:** receive/send value between 0.0 and 1.0 scaled in pseudo logarithmic curve for frequency knobs
- **KOSCScaleOnOff:** receive/send value 0.0 or 1.0 for off/on
- **KOSCScaleToggle:** receive value 1.0 toggles TotalMix value, TotalMix re-sends 0.0 or 1.0 for on/off
- **KOSCScaleDirect:** receive/send value in 1:1 scale as described in table
- **KOSCScalePrintVal:** send string with value (number) from TotalMix
- **KOSCScalePrintdB:** send string in dB and -oo
- **KOSCScalePrintPan:** send string Pan (e.g. "L5")
- **KOSCPrintSelection:** send string for selection values (e.g. "Bell" for EQ type)
- **KOSCScaleString:** special labels or information strings
- **KOSCScaleLevel:** (Appears on Page 3) Level in dB (likely similar scaling to Fader/PrintdB)

## New in 1.96

- Room EQ in Page 4
- Mic/Preamp-Gain scaled device and channel dependent (turned off in 1.90 compatibility mode)
- EQ-Band-Type (characteristics) scaled device dependent (turned off in 1.90 compatibility mode)
- Expander-Threshold scaled to range -99.0 to -20.0 (turned off in 1.90 compatibility mode)
- Increment changed for Room Scale (Reverb) to 0.01 and Auto Level Max Gain to 0.5 (turned off in 1.90 compatibility mode)
- Checking of certain parameters (e.g. Phantom Power, Instrument) for bus and channel; re-sending 0 if not available
- Re-sending 0 for Cue in Input and Playback Bus and for Cue-To channel
- No more cyclic sending of Stop/Play/Record Buttons; Play Button alternating in Pause Mode
- New Record State as text string

## TouchOSC specific

- New TouchOSC version has different behaviour of MultiPush fields. This required a changed template (Page 3: Snapshots and Groups as single push buttons).
- Remaining MultiPush fields do not react on receive, while button is still touched. So long touching leads to wrong display state for Cue, Phantom, etc.

---

# OSC Command Reference

## Page 1 - Mixer

| Name                                | Function                         | ScaleTypeParameter | ScaleTypeDisplayValue | Channel | Remarks                                                                                                           |
| ----------------------------------- | -------------------------------- | ------------------ | --------------------- | ------- | ----------------------------------------------------------------------------------------------------------------- |
| `/1/businput`                       | Select Input Bus data            | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/busPlayback`                    | Select Playback Bus data         | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/busOutput`                      | Select Output Bus data           | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/track+`                         | shift bank start +1              | KOSCScaleNoSend    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/track-`                         | shift bank start-1               | KOSCScaleNoSend    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/bank+`                          | Change bank start +bank size     | KOSCScaleNoSend    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/bank-`                          | Change bank start-bank size      | KOSCScaleNoSend    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/globalMute`                     | Mute Enable (globally)           | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/globalSolo`                     | Solo Enable (globally)           | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/trim`                           | Trim mode for all channels       | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/mainDim`                        | DIM                              | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/mainSpeakerB`                   | Assign channel for speaker B     | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/speakerBLinked`                 | Speaker B volume linked to main  | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/mainRecall`                     | Recall Main Out Volume           | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/mainMuteFx`                     | Mute FX return on Main Out       | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/mainMono`                       | Mono for Main Out                | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/mainExtIn`                      | Enable ExtIn -Path               | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/mainTalkback`                   | Enable Talkback                  | KOSCScaleToggle    | KOSCScaleNoSend       | -       | -                                                                                                                 |
| `/1/volume1` - `/1/volume8`         | Mix-Fader/Output Volume          | KOSCScaleFader     | KOSCScalePrintdB      | 0 - 7   | -                                                                                                                 |
| `/1/volume9` to `/1/volume24`       | Mix-Fader/Output Volume          | KOSCScaleFader     | KOSCScalePrintdB      | 8-47    | available when selected in options                                                                                |
| `/1/mastervolume`                   | Main Out Volume                  | KOSCScaleFader     | KOSCScalePrintdB      | special | available when selected in options                                                                                |
| `/1/pan1` - `/1/pan8`               | Mix-Pan/Balance/Output Balance   | KOSCScaleLin01     | KOSCScaleNoSend       | 0 - 7   | -                                                                                                                 |
| `/1/pan9-24`                        | Mix-Pan/Balance/Output Balance   | KOSCScaleLin01     | KOSCScaleNoSend       | 8-47    | available when selected in options                                                                                |
| `/1/mute/1/1` - `/1/mute/1/8`       | Channel Mute                     | KOSCScaleOnOff     | KOSCScaleNoSend       | 0 - 7   | Only effective in TotalMix (Monitoring)                                                                           |
| `/1/mute/1/9-24`                    | Channel Mute                     | KOSCScaleOnOff     | KOSCScaleNoSend       | 8-47    | available when selected in options                                                                                |
| `/1/micgain1` - `/1/micgain8`       | Microphone or digital Input Gain | KOSCScaleLin01     | KOSCScalePrintdB      | 0 - 7   | Certain Inputs only (device dependent). Sent to both channels when stereo. Scale depending on channel and device. |
| `/1/micgain9-micgain24`             | Microphone or digital Input Gain | KOSCScaleLin01     | KOSCScalePrintdB      | 8-47    | available when selected in options                                                                                |
| `/1/phantom/1/1` - `/1/phantom/1/8` | Phantom Power                    | KOSCScaleOnOff     | KOSCScaleNoSend       | 0 - 7   | Certain Inputs only (device dependent)                                                                            |
| `/1/phantom/1/9-24`                 | Phantom Power                    | KOSCScaleOnOff     | KOSCScaleNoSend       | 8-47    | available when selected in options                                                                                |
| `/1/solo/1/1` - `/1/solo/1/8`       | Solo/PFL                         | KOSCScaleOnOff     | KOSCScaleNoSend       | 0 - 7   | PFL in PFL-Mode. Inputs and Playbacks only.                                                                       |
| `/1/solo/1/9-24`                    | Solo/PFL                         | KOSCScaleOnOff     | KOSCScaleNoSend       | 8-47    | available when selected in options. PFL in PFL-Mode. Inputs and Playbacks only.                                   |
| `/1/select/1/1` - `/1/select/1/8`   | Selection for ad-hoc fader group | KOSCScaleOnOff     | KOSCScaleNoSend       | 0 - 7   | -                                                                                                                 |
| `/1/select/1/9-24`                  | Selection for ad-hoc fader group | KOSCScaleOnOff     | KOSCScaleNoSend       | 8-47    | available when selected in options                                                                                |
| `/1/cue/1/1` - `/1/cue/1/8`         | Cue Output                       | KOSCScaleOnOff     | KOSCScaleNoSend       | 0 - 7   | Outputs only. Cue on Cue-To-Channel disables Cue.                                                                 |
| `/1/cue/1/9-24`                     | Cue Output                       | KOSCScaleOnOff     | KOSCScaleNoSend       | 8-47    | available when selected in options. Outputs only. Cue on Cue-To-Channel disables Cue.                             |
| `/1/labelSubmix`                    | Name of selected Submix          | KOSCScaleString    | KOSCScaleNoSend       | 0       | -                                                                                                                 |
| `/1/trackname1` - `/1/trackname8`   | Channel Name                     | KOSCScaleString    | KOSCScaleNoSend       | 0 - 7   | -                                                                                                                 |
| `/1/trackname9-24`                  | Channel Name                     | KOSCScaleString    | KOSCScaleNoSend       | 8-47    | available when selected in options                                                                                |
| `/1/level1Left` - `/1/level8Left`   | Level in dB                      | KOSCScaleLevel     | KOSCScalePrintdB      | 0 - 7   | Peak Hold can be enabled in TotalMix Settings. Measured on terminals (input pre FX, output post FX).              |
| `/1/level1Right` - `/1/level8Right` | Level in dB                      | KOSCScaleLevel     | KOSCScalePrintdB      | 0 - 7   | Peak Hold can be enabled in TotalMix Settings. Measured on terminals (input pre FX, output post FX).              |
| `/1/level9Left-level24Left`         | Level in dB                      | KOSCScaleLevel     | KOSCScalePrintdB      | 8-47    | available when selected in options                                                                                |
| `/1/level9Right-level24Right`       | Level in dB                      | KOSCScaleLevel     | KOSCScalePrintdB      | 8-47    | available when selected in options                                                                                |

---

## Page 2 - Channel Effects

*(Note: Commands on this page apply to the currently selected single channel offset)*

| Name                | Function                             | ScaleTypeParameter | ScaleTypeDisplayValue | Remarks                                                                                              |
| ------------------- | ------------------------------------ | ------------------ | --------------------- | ---------------------------------------------------------------------------------------------------- |
| `/2/businput`       | Select Input Bus data                | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/busPlayback`    | Select Playback Bus data             | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/busOutput`      | Select Output Bus data               | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/track+`         | select channel +1                    | KOSCScaleNoSend    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/track-`         | select channel -1                    | KOSCScaleNoSend    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/volume`         | Mix-Fader/Output Volume              | KOSCScaleFader     | KOSCScalePrintdB      | -                                                                                                    |
| `/2/pan`            | Mix-Pan/Balance/Output Balance       | KOSCScaleLin01     | kOSCScalePrintPan     | -                                                                                                    |
| `/2/mute`           | Channel Mute                         | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/solo`           | Solo/PFL                             | KOSCScaleToggle    | KOSCScaleNoSend       | PFL in PFL-Mode                                                                                      |
| `/2/phase`          | Phase Invert Left/Mono               | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/phaseRight`     | Phase Invert Right                   | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/phantom`        | Phantom Power                        | KOSCScaleToggle    | KOSCScaleNoSend       | Certain Inputs only (device dependent)                                                               |
| `/2/instrument`     | Instrument Option                    | kOSCScaleToggle    | KOSCScaleNoSend       | Certain Inputs only (device dependent)                                                               |
| `/2/pad`            | Pad Option                           | KOSCScaleToggle    | KOSCScaleNoSend       | Certain Inputs only (device dependent)                                                               |
| `/2/msProc`         | Input MS-Processing                  | KOSCScaleToggle    | KOSCScaleNoSend       | Only effective in TotalMix (Monitoring)                                                              |
| `/2/autoset`        | AutoSet Gain Option                  | kOSCScaleToggle    | KOSCScaleNoSend       | Certain Inputs only (device dependent)                                                               |
| `/2/loopback`       | Enable Loopback                      | KOSCScaleToggle    | KOSCScaleNoSend       | Outputs Only                                                                                         |
| `/2/stereo`         | Set Channel Stereo/Mono              | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/cue`            | Cue Output                           | KOSCScaleToggle    | KOSCScaleNoSend       | Outputs Only                                                                                         |
| `/2/talkbackSel`    | Include Channel in Talkback          | KOSCScaleToggle    | KOSCScaleNoSend       | Outputs Only                                                                                         |
| `/2/noTrim`         | Exclude Channel from Trim            | KOSCScaleToggle    | KOSCScaleNoSend       | Outputs Only                                                                                         |
| `/2/gain`           | Microphone or digital Input Gain     | KOSCScaleLin01     | KOSCScalePrintVal     | Scale depending on channel and device                                                                |
| `/2/gainRight`      | Microphone or digital Input Gain     | KOSCScaleLin01     | KOSCScalePrintVal     | Only way to set right channel different when channel is stereo                                       |
| `/2/refLevel`       | Analog Reference Value               | KOSCScaleDirect    | KOSCScalePrintVal     | Value 0...3 device and channel dependent only effective in TotalMix (Monitoring)                     |
| `/2/width`          | Width-Parameter in TotalMix          | KOSCScaleLin01     | kOSCScalePrintVal     | Only Inputs and Playbacks                                                                            |
| `/2/reverbSend`     | Reverb Send                          | KOSCScaleLin01     | KOSCScalePrintdB      | Only Inputs and Playbacks                                                                            |
| `/2/reverbReturn`   | Reverb Return                        | KOSCScaleLin01     | KOSCScalePrintdB      | Outputs Only                                                                                         |
| `/2/recordEnable`   | Record Enable for Channel            | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/playChannel`    | Playback-track (in file) for Channel | KOSCScaleDirect    | KOSCScalePrintVal     | Value 0 for off                                                                                      |
| `/2/lowcutEnable`   | Low Cut Enable                       | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/lowcutGrade`    | Low Cut Grade                        | KOSCScaleLin01     | KOSCPrintSelection    | -                                                                                                    |
| `/2/lowcutFreq`     | Low Cut Corner Freq                  | KOSCScaleFreq      | kOSCScalePrintVal     | -                                                                                                    |
| `/2/eqEnable`       | Parametric EQ enable                 | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/eqType1`        | Band 1 characteristics               | KOSCScaleLin01     | KOSCPrintSelection    | Device depending Bell, Low Shelf, High-Pass, (Low Pass) scaled 0...1                                 |
| `/2/eqGain1`        | Band 1 Gain                          | KOSCScaleLin01     | kOSCScalePrintdB      | -                                                                                                    |
| `/2/eqFreq1`        | Band 1 Freq.                         | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                    |
| `/2/eqQ1`           | Band 1 Q                             | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                    |
| `/2/eqGain2`        | Band 2 Gain                          | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                    |
| `/2/eqFreq2`        | Band 2 Freq.                         | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                    |
| `/2/eqQ2`           | Band 2 Q                             | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                    |
| `/2/eqType3`        | Band 3 characteristics               | KOSCScaleLin01     | KOSCPrintSelection    | variants Bell, High Shelf, (Low Pass, High Pass), scaled 0...1                                       |
| `/2/eqGain3`        | Band 3 Gain                          | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                    |
| `/2/eqFreq3`        | Band 3 Freq.                         | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                    |
| `/2/eqQ3`           | Band 3 Q                             | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                    |
| `/2/compexpEnable`  | Compressor/Expander en.              | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/compexpGain`    | Make-up-gain                         | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                    |
| `/2/compexpAttack`  | Compressor Attack Time               | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                    |
| `/2/compexpRelease` | Compressor Release Time              | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                    |
| `/2/compTrsh`       | Compressor Threshold                 | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                    |
| `/2/compRatio`      | Compressor Ratio                     | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                    |
| `/2/expTrsh`        | Expander Threshold                   | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                    |
| `/2/expRatio`       | Expander Ratio                       | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                    |
| `/2/alevEnable`     | Auto Level Enable                    | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/alevMaxgain`    | Auto Level Max. Gain                 | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                    |
| `/2/alevHeadroom`   | Auto Level Headroom                  | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                    |
| `/2/alevRisetime`   | Auto Level Rise Speed                | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                    |
| `/2/trackname`      | Channel Name                         | KOSCScaleString    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/labelSubmix`    | Name of selected Submix              | KOSCScaleString    | KOSCScaleNoSend       | -                                                                                                    |
| `/2/levelLeft`      | Level in dB                          | KOSCScaleLevel     | KOSCScalePrintdB      | Peak Hold can be enabled in TotalMix Settings. Measured on terminals (input pre FX, output post FX). |
| `/2/levelRight`     | Level in dB                          | KOSCScaleLevel     | KOSCScalePrintdB      | Peak Hold can be enabled in TotalMix Settings. Measured on terminals (input pre FX, output post FX). |

---

## Page 3 - Reverb, Echo, Snapshots, Groups

| Name                                    | Function                 | ScaleTypeParameter | ScaleTypeDisplayValue | Remarks                                                |
| --------------------------------------- | ------------------------ | ------------------ | --------------------- | ------------------------------------------------------ |
| `/3/globalMute`                         | Mute Enable (globally)   | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/3/globalSolo`                         | Solo Enable (globally)   | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/3/reverbEnable`                       | Reverb Enable            | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/3/reverbType`                         | Reverb Type (Room)       | KOSCScaleLin01     | KOSCPrintSelection    | -                                                      |
| `/3/reverbPredelay`                     | Reverb Pre-Delay         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                      |
| `/3/reverbLowcut`                       | Reverb Low Cut           | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                      |
| `/3/reverbHighcut`                      | Reverb High-Cut          | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                      |
| `/3/reverbRoomscale`                    | Reverb Room Scale        | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                      |
| `/3/reverbAttack`                       | Reverb Attack Time       | KOSCScaleLin01     | KOSCScalePrintVal     | only for Envelope types                                |
| `/3/reverbHold`                         | Reverb Hold Time         | KOSCScaleLin01     | KOSCScalePrintVal     | only for Envelope types                                |
| `/3/reverbRelease`                      | Reverb Release Time      | KOSCScaleLin01     | KOSCScalePrintVal     | only for Envelope types                                |
| `/3/reverbTime`                         | Reverb Time              | KOSCScaleLin01     | KOSCScalePrintVal     | only for Space type                                    |
| `/3/reverbHighdamp`                     | Reverb High-Damp         | KOSCScaleFreq      | KOSCScalePrintVal     | only for Space type                                    |
| `/3/reverbSmooth`                       | Smoothness               | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                      |
| `/3/reverbWidth`                        | Reverb Stereo Width      | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                      |
| `/3/reverbVolume`                       | Reverb Volume            | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                      |
| `/3/echoEnable`                         | Echo Enable              | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/3/echoType`                           | Echo Type Selection      | KOSCScaleLin01     | KOSCPrintSelection    | -                                                      |
| `/3/echoDelaytime`                      | Echo Delay Time          | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                      |
| `/3/echoFeedback`                       | Echo Feedback Value      | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                      |
| `/3/echoWidth`                          | Echo Stereo Width        | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                      |
| `/3/echoVolume`                         | Echo Volume              | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                      |
| `/3/undo`                               | Trigger Undo             | KOSCScaleNoSend    | KOSCScaleNoSend       | -                                                      |
| `/3/redo`                               | Trigger Redo             | KOSCScaleNoSend    | KOSCScaleNoSend       | -                                                      |
| `/snapshots/8/1` - `/snapshots/1/1`     | Load Snapshot 1 - 8      | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/muteGroups/4/1` - `/muteGroups/1/1`   | Enable Mute Group 1 - 4  | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/soloGroups/4/1` - `/soloGroups/1/1`   | Enable Solo Group 1 - 4  | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/faderGroups/4/1` - `/faderGroups/1/1` | Enable Fader Group 1 - 4 | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/record/RecordStart`                   | Start Record (DuRec)     | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/record/PlayPause`                     | Play/Pause (DuRec)       | KOSCScaleToggle    | KOSCScaleNoSend       | alternating Value 0/1 send by TotalMix FX when paused  |
| `/record/Stop`                          | Stop Play/Record         | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                      |
| `/record/Time`                          | Current Time (DuRec)     | KOSCScaleString    | KOSCScaleNoSend       | -                                                      |
| `/record/State`                         | Current DuRec State      | KOSCScaleString    | KOSCScaleNoSend       | Strings "Not ready", "Stop", "Record", "Play", "Pause" |

---

## Page 4 - Room EQ

*(Note: Selecting Page 4 selects Output bus. Commands apply to the currently selected single channel offset)*

| Name               | Function                         | ScaleTypeParameter | ScaleTypeDisplayValue | Remarks                                                                                                          |
| ------------------ | -------------------------------- | ------------------ | --------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `/4/track+`        | select channel +1                | KOSCScaleNoSend    | KOSCScaleNoSend       | -                                                                                                                |
| `/4/track-`        | select channel -1                | KOSCScaleNoSend    | KOSCScaleNoSend       | -                                                                                                                |
| `/4/leftChannel`   | Selects left channel parameters  | KOSCScaleToggle    | KOSCScaleNoSend       | If leftChannel and rightChannel are active, parameters are displayed from left channel and sent to both channels |
| `/4/rightChannel`  | Selects right channel parameters | KOSCScaleToggle    | KOSCScaleNoSend       | If leftChannel and rightChannel are active, parameters are displayed from left channel and sent to both channels |
| `/4/reqDelay`      | Delay Value                      | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqVolumeCorr` | Volume Correction                | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqEnable`     | Parametric EQ enable             | KOSCScaleToggle    | KOSCScaleNoSend       | -                                                                                                                |
| `/4/reqType1`      | Band 1 characteristics           | KOSCScaleLin01     | KOSCPrintSelection    | variants Bell, Low Shelf, High Pass, Low Pass, scaled 0...1                                                      |
| `/4/reqGain1`      | Band 1 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq1`      | Band 1 Freq                      | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ1`         | Band 1 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqGain2`      | Band 2 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq2`      | Band 2 Freq.                     | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ2`         | Band 2 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqGain3`      | Band 3 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq3`      | Band 3 Freq.                     | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ3`         | Band 3 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqGain4`      | Band 4 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq4`      | Band 4 Freq.                     | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ4`         | Band 4 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqGain5`      | Band 5 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq5`      | Band 5 Freq.                     | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ5`         | Band 5 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqGain6`      | Band 6 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq6`      | Band 6 Freq.                     | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ6`         | Band 6 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqGain7`      | Band 7 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq7`      | Band 7 Freq.                     | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ7`         | Band 7 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqGain8`      | Band 8 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq8`      | Band 8 Freq.                     | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ8`         | Band 8 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqType9`      | Band 9 characteristics           | KOSCScaleLin01     | KOSCPrintSelection    | variants Bell, High Shelf, Low Pass, High Pass, scaled 0...1                                                     |
| `/4/reqGain9`      | Band 9 Gain                      | KOSCScaleLin01     | KOSCScalePrintdB      | -                                                                                                                |
| `/4/reqFreq9`      | Band 9 Freq.                     | KOSCScaleFreq      | KOSCScalePrintVal     | -                                                                                                                |
| `/4/reqQ9`         | Band 9 Q                         | KOSCScaleLin01     | KOSCScalePrintVal     | -                                                                                                                |
| `/4/trackname`     | Channel Name                     | KOSCScaleString    | KOSCScaleNoSend       | -                                                                                                                |
