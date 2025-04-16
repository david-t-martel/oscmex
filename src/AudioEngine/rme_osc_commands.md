# RME TotalMix FX OSC Command List

*Related TotalMix FX Version: 1.96, 22.07.2024*

## Comments

The OSC functionality in TotalMix was developed in 2014 for communication with "old" TouchOSC in a Mackie-Protocol style.

This concept has a mixer-bank style and is freely assignable to large matrix mixers (like TotalMix). Resulting from this is a control-element-orientated addressing with dynamic mapping to certain channels, depending on bank assignment.

Bank start, bus selection and submix selection can be controlled by buttons to navigate in the mixer matrix - like with MIDI Mackie Control. Single-channel settings (Page 2 and 4) are addressed by a "single channel offset" to the bank start.

Many labels were assigned to match the MultiPush elements of Touch OSC.

Later some "none-paged" parameters were attached to allow direct control of submix, bank-start and single-channel-page-offset in the bank. "Page" selection is mainly relevant for parameter re-sending: selecting a new page by sending any parameter containing a new page number triggers a complete re-send of all parameters of the newly selected page by TotalMix.

## Table Columns Description

* **Name:** string label for OSC-Command, extended by page prefix, e.g. `/1/bank+`; none paged parameters only, e.g. `/setSubmix`
* **Function Type:** short description of controlled TotalMix function
* **Scale Type Parameter:** scaling of received and sent back values (see list below)
* **Scale Type Display Value:** scaling of an additional label, sent only by TotalMix, with postfix `Val`, e.g. `/1/volume1Val` (see list of scale types below)
* **Channel:** Channel offset for multichannel functions (Page 1)

## Scale Types

* **KOSCScaleNoSend:** no value sent by TotalMix FX (e.g. for navigation buttons or if there is no additional Display Value)
* **KOSCScaleLin01:** linear receive/send value, scaled between 0.0 and 1.0 - e.g. 0.000, 0.333, 0.666, 1.000 for 4 types of EQ band characteristics
* **KOSCScaleFader:** receive/send value between 0.0 and 1.0 scaled in fader curve
* **KOSCScaleFreq:** receive/send value between 0.0 and 1.0 scaled in pseudo logarithmic curve for frequency knobs
* **KOSCScaleOnOff:** receive/send value 0.0 or 1.0 for off/on
* **KOSCScaleToggle:** receive value 1.0 toggles TotalMix value, TotalMix re-sends 0.0 or 1.0 for on/off
* **KOSCScaleDirect:** receive/send value in 1:1 scale as described in table
* **KOSCScalePrintVal:** send string with value (number) from TotalMix
* **KOSCScalePrintdB:** send string in dB and -oo
* **KOSCScalePrintPan:** send string Pan (e.g. "L5")
* **KOSCPrintSelection:** send string for selection values (e.g. "Bell" for EQ type)
* **KOSCScaleString:** special labels or information strings
* **KOSCScaleLevel:** (Appears on Page 3) Level in dB (likely similar scaling to Fader/PrintdB)

*(Note: Fader Curve and Frequency calculations were provided in the PDF but omitted here for brevity in Markdown format)*

## New in 1.96

* Room EQ in Page 4
* Mic/Preamp-Gain scaled device and channel dependent (turned off in 1.90 compatibility mode)
* EQ-Band-Type (characteristics) scaled device dependent (turned off in 1.90 compatibility mode)
* Expander-Threshold scaled to range -99.0 to -20.0 (turned off in 1.90 compatibility mode)
* Increment changed for Room Scale (Reverb) to 0.01 and Auto Level Max Gain to 0.5 (turned off in 1.90 compatibility mode)
* Checking of certain parameters (e.g. Phantom Power, Instrument) for bus and channel; re-sending 0 if not available
* Re-sending 0 for Cue in Input and Playback Bus and for Cue-To channel
* No more cyclic sending of Stop/Play/Record Buttons; Play Button alternating in Pause Mode
* New Record State as text string

## TouchOSC specific

* New TouchOSC version has different behaviour of MultiPush fields. This required a changed template (Page 3: Snapshots and Groups as single push buttons).
* Remaining MultiPush fields do not react on receive, while button is still touched. So long touching leads to wrong display state for Cue, Phantom, etc.

---

## Page 1 - Mixer

**Name:** `/1/businput`
**Function:** Select Input Bus data
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/busPlayback`
**Function:** Select Playback Bus data
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/busOutput`
**Function:** Select Output Bus data
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/track+`
**Function:** shift bank start +1
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/track-`
**Function:** shift bank start-1
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/bank+`
**Function:** Change bank start +bank size
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/bank-`
**Function:** Change bank start-bank size
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/globalMute`
**Function:** Mute Enable (globally)
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/globalSolo`
**Function:** Solo Enable (globally)
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/trim`
**Function:** Trim mode for all channels
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/mainDim`
**Function:** DIM
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/mainSpeakerB`
**Function:** Assign channel for speaker B
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/speakerBLinked`
**Function:** Speaker B volume linked to main
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/mainRecall`
**Function:** Recall Main Out Volume
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/mainMuteFx`
**Function:** Mute FX return on Main Out
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/mainMono`
**Function:** Mono for Main Out
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/mainExtIn`
**Function:** Enable ExtIn -Path
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/mainTalkback`
**Function:** Enable Talkback
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** -
**Remarks:** -

**Name:** `/1/volume1` - `/1/volume8`
**Function:** Mix-Fader/Output Volume
**ScaleTypeParameter:** KOSCScaleFader
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** 0 - 7
**Remarks:** -

**Name:** `/1/volume9` to `/1/volume24`
**Function:** Mix-Fader/Output Volume
**ScaleTypeParameter:** KOSCScaleFader
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** 8-47
**Remarks:** available when selected in options

**Name:** `/1/mastervolume`
**Function:** Main Out Volume
**ScaleTypeParameter:** KOSCScaleFader
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** special
**Remarks:** available when selected in options

**Name:** `/1/pan1` - `/1/pan8`
**Function:** Mix-Pan/Balance/Output Balance
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 0 - 7
**Remarks:** -

**Name:** `/1/pan9-24`
**Function:** Mix-Pan/Balance/Output Balance
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 8-47
**Remarks:** available when selected in options

**Name:** `/1/mute/1/1` - `/1/mute/1/8`
**Function:** Channel Mute
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 0 - 7
**Remarks:** Only effective in TotalMix (Monitoring)

**Name:** `/1/mute/1/9-24`
**Function:** Channel Mute
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 8-47
**Remarks:** available when selected in options

**Name:** `/1/micgain1` - `/1/micgain8`
**Function:** Microphone or digital Input Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** 0 - 7
**Remarks:** Certain Inputs only (device dependent). Sent to both channels when stereo. Scale depending on channel and device.

**Name:** `/1/micgain9-micgain24`
**Function:** Microphone or digital Input Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** 8-47
**Remarks:** available when selected in options

**Name:** `/1/phantom/1/1` - `/1/phantom/1/8`
**Function:** Phantom Power
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 0 - 7
**Remarks:** Certain Inputs only (device dependent)

**Name:** `/1/phantom/1/9-24`
**Function:** Phantom Power
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 8-47
**Remarks:** available when selected in options

**Name:** `/1/solo/1/1` - `/1/solo/1/8`
**Function:** Solo/PFL
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 0 - 7
**Remarks:** PFL in PFL-Mode. Inputs and Playbacks only.

**Name:** `/1/solo/1/9-24`
**Function:** Solo/PFL
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 8-47
**Remarks:** available when selected in options. PFL in PFL-Mode. Inputs and Playbacks only.

**Name:** `/1/select/1/1` - `/1/select/1/8`
**Function:** Selection for ad-hoc fader group
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 0 - 7
**Remarks:** -

**Name:** `/1/select/1/9-24`
**Function:** Selection for ad-hoc fader group
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 8-47
**Remarks:** available when selected in options

**Name:** `/1/cue/1/1` - `/1/cue/1/8`
**Function:** Cue Output
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 0 - 7
**Remarks:** Outputs only. Cue on Cue-To-Channel disables Cue.

**Name:** `/1/cue/1/9-24`
**Function:** Cue Output
**ScaleTypeParameter:** KOSCScaleOnOff
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 8-47
**Remarks:** available when selected in options. Outputs only. Cue on Cue-To-Channel disables Cue.

**Name:** `/1/labelSubmix`
**Function:** Name of selected Submix
**ScaleTypeParameter:** KOSCScaleString
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 0
**Remarks:** -

**Name:** `/1/trackname1` - `/1/trackname8`
**Function:** Channel Name
**ScaleTypeParameter:** KOSCScaleString
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 0 - 7
**Remarks:** -

**Name:** `/1/trackname9-24`
**Function:** Channel Name
**ScaleTypeParameter:** KOSCScaleString
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Channel:** 8-47
**Remarks:** available when selected in options

**Name:** `/1/level1Left` - `/1/level8Left`
**Function:** Level in dB
**ScaleTypeParameter:** KOSCScaleLevel
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** 0 - 7
**Remarks:** Peak Hold can be enabled in TotalMix Settings. Measured on terminals (input pre FX, output post FX).

**Name:** `/1/level1Right` - `/1/level8Right`
**Function:** Level in dB
**ScaleTypeParameter:** KOSCScaleLevel
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** 0 - 7
**Remarks:** Peak Hold can be enabled in TotalMix Settings. Measured on terminals (input pre FX, output post FX).

**Name:** `/1/level9Left-level24Left`
**Function:** Level in dB
**ScaleTypeParameter:** KOSCScaleLevel
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** 8-47
**Remarks:** available when selected in options

**Name:** `/1/level9Right-level24Right`
**Function:** Level in dB
**ScaleTypeParameter:** KOSCScaleLevel
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Channel:** 8-47
**Remarks:** available when selected in options

---

## Page 2 - Channel Effects

*(Note: Commands on this page apply to the currently selected single channel offset)*

**Name:** `/2/businput`
**Function:** Select Input Bus data
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/busPlayback`
**Function:** Select Playback Bus data
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/busOutput`
**Function:** Select Output Bus data
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/track+`
**Function:** select channel +1
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/track-`
**Function:** select channel -1
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/volume`
**Function:** Mix-Fader/Output Volume
**ScaleTypeParameter:** KOSCScaleFader
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/2/pan`
**Function:** Mix-Pan/Balance/Output Balance
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** kOSCScalePrintPan
**Remarks:** -

**Name:** `/2/mute`
**Function:** Channel Mute
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/solo`
**Function:** Solo/PFL
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** PFL in PFL-Mode

**Name:** `/2/phase`
**Function:** Phase Invert Left/Mono
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/phaseRight`
**Function:** Phase Invert Right
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/phantom`
**Function:** Phantom Power
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Certain Inputs only (device dependent)

**Name:** `/2/instrument`
**Function:** Instrument Option
**ScaleTypeParameter:** kOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Certain Inputs only (device dependent)

**Name:** `/2/pad`
**Function:** Pad Option
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Certain Inputs only (device dependent)

**Name:** `/2/msProc`
**Function:** Input MS-Processing
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Only effective in TotalMix (Monitoring)

**Name:** `/2/autoset`
**Function:** AutoSet Gain Option
**ScaleTypeParameter:** kOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Certain Inputs only (device dependent)

**Name:** `/2/loopback`
**Function:** Enable Loopback
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Outputs Only

**Name:** `/2/stereo`
**Function:** Set Channel Stereo/Mono
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/cue`
**Function:** Cue Output
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Outputs Only

**Name:** `/2/talkbackSel`
**Function:** Include Channel in Talkback
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Outputs Only

**Name:** `/2/noTrim`
**Function:** Exclude Channel from Trim
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Outputs Only

**Name:** `/2/gain`
**Function:** Microphone or digital Input Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** Scale depending on channel and device

**Name:** `/2/gainRight`
**Function:** Microphone or digital Input Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** Only way to set right channel different when channel is stereo

**Name:** `/2/refLevel`
**Function:** Analog Reference Value
**ScaleTypeParameter:** KOSCScaleDirect
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** Value 0...3 device and channel dependent only effective in TotalMix (Monitoring)

**Name:** `/2/width`
**Function:** Width-Parameter in TotalMix
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** kOSCScalePrintVal
**Remarks:** Only Inputs and Playbacks

**Name:** `/2/reverbSend`
**Function:** Reverb Send
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** Only Inputs and Playbacks

**Name:** `/2/reverbReturn`
**Function:** Reverb Return
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** Outputs Only

**Name:** `/2/recordEnable`
**Function:** Record Enable for Channel
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/playChannel`
**Function:** Playback-track (in file) for Channel
**ScaleTypeParameter:** KOSCScaleDirect
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** Value 0 for off

**Name:** `/2/lowcutEnable`
**Function:** Low Cut Enable
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/lowcutGrade`
**Function:** Low Cut Grade
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCPrintSelection
**Remarks:** -

**Name:** `/2/lowcutFreq`
**Function:** Low Cut Corner Freq
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** kOSCScalePrintVal
**Remarks:** -

**Name:** `/2/eqEnable`
**Function:** Parametric EQ enable
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/eqType1`
**Function:** Band 1 characteristics
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCPrintSelection
**Remarks:** Device depending Bell, Low Shelf, High-Pass, (Low Pass) scaled 0...1

**Name:** `/2/eqGain1`
**Function:** Band 1 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** kOSCScalePrintdB
**Remarks:** -

**Name:** `/2/eqFreq1`
**Function:** Band 1 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/eqQ1`
**Function:** Band 1 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/eqGain2`
**Function:** Band 2 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/2/eqFreq2`
**Function:** Band 2 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/eqQ2`
**Function:** Band 2 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/eqType3`
**Function:** Band 3 characteristics
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCPrintSelection
**Remarks:** variants Bell, High Shelf, (Low Pass, High Pass), scaled 0...1

**Name:** `/2/eqGain3`
**Function:** Band 3 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/2/eqFreq3`
**Function:** Band 3 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/eqQ3`
**Function:** Band 3 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/compexpEnable`
**Function:** Compressor/Expander en.
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/compexpGain`
**Function:** Make-up-gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/2/compexpAttack`
**Function:** Compressor Attack Time
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/compexpRelease`
**Function:** Compressor Release Time
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/compTrsh`
**Function:** Compressor Threshold
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/2/compRatio`
**Function:** Compressor Ratio
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/expTrsh`
**Function:** Expander Threshold
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/2/expRatio`
**Function:** Expander Ratio
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/alevEnable`
**Function:** Auto Level Enable
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/alevMaxgain`
**Function:** Auto Level Max. Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/2/alevHeadroom`
**Function:** Auto Level Headroom
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/2/alevRisetime`
**Function:** Auto Level Rise Speed
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/2/trackname`
**Function:** Channel Name
**ScaleTypeParameter:** KOSCScaleString
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/labelSubmix`
**Function:** Name of selected Submix
**ScaleTypeParameter:** KOSCScaleString
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/2/levelLeft`
**Function:** Level in dB
**ScaleTypeParameter:** KOSCScaleLevel
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** Peak Hold can be enabled in TotalMix Settings. Measured on terminals (input pre FX, output post FX).

**Name:** `/2/levelRight`
**Function:** Level in dB
**ScaleTypeParameter:** KOSCScaleLevel
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** Peak Hold can be enabled in TotalMix Settings. Measured on terminals (input pre FX, output post FX).

---

## Page 3 - Reverb, Echo, Snapshots, Groups

**Name:** `/3/globalMute`
**Function:** Mute Enable (globally)
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/3/globalSolo`
**Function:** Solo Enable (globally)
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/3/reverbEnable`
**Function:** Reverb Enable
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/3/reverbType`
**Function:** Reverb Type (Room)
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCPrintSelection
**Remarks:** -

**Name:** `/3/reverbPredelay`
**Function:** Reverb Pre-Delay
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/reverbLowcut`
**Function:** Reverb Low Cut
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/reverbHighcut`
**Function:** Reverb High-Cut
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/reverbRoomscale`
**Function:** Reverb Room Scale
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/reverbAttack`
**Function:** Reverb Attack Time
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** only for Envelope types

**Name:** `/3/reverbHold`
**Function:** Reverb Hold Time
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** only for Envelope types

**Name:** `/3/reverbRelease`
**Function:** Reverb Release Time
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** only for Envelope types

**Name:** `/3/reverbTime`
**Function:** Reverb Time
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** only for Space type

**Name:** `/3/reverbHighdamp`
**Function:** Reverb High-Damp
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** only for Space type

**Name:** `/3/reverbSmooth`
**Function:** Smoothness
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/reverbWidth`
**Function:** Reverb Stereo Width
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/reverbVolume`
**Function:** Reverb Volume
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/3/echoEnable`
**Function:** Echo Enable
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/3/echoType`
**Function:** Echo Type Selection
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCPrintSelection
**Remarks:** -

**Name:** `/3/echoDelaytime`
**Function:** Echo Delay Time
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/echoFeedback`
**Function:** Echo Feedback Value
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/echoWidth`
**Function:** Echo Stereo Width
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/3/echoVolume`
**Function:** Echo Volume
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/3/undo`
**Function:** Trigger Undo
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/3/redo`
**Function:** Trigger Redo
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/snapshots/8/1` - `/snapshots/1/1`
**Function:** Load Snapshot 1 - 8
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/muteGroups/4/1` - `/muteGroups/1/1`
**Function:** Enable Mute Group 1 - 4
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/soloGroups/4/1` - `/soloGroups/1/1`
**Function:** Enable Solo Group 1 - 4
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/faderGroups/4/1` - `/faderGroups/1/1`
**Function:** Enable Fader Group 1 - 4
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/record/RecordStart`
**Function:** Start Record (DuRec)
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/record/PlayPause`
**Function:** Play/Pause (DuRec)
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** alternating Value 0/1 send by TotalMix FX when paused

**Name:** `/record/Stop`
**Function:** Stop Play/Record
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/record/Time`
**Function:** Current Time (DuRec)
**ScaleTypeParameter:** KOSCScaleString
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/record/State`
**Function:** Current DuRec State
**ScaleTypeParameter:** KOSCScaleString
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** Strings "Not ready", "Stop", "Record", "Play", "Pause"

---

## Page 4 - Room EQ

*(Note: Selecting Page 4 selects Output bus. Commands apply to the currently selected single channel offset)*

**Name:** `/4/track+`
**Function:** select channel +1
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/4/track-`
**Function:** select channel -1
**ScaleTypeParameter:** KOSCScaleNoSend
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/4/leftChannel`
**Function:** Selects left channel parameters
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** If leftChannel and rightChannel are active, parameters are displayed from left channel and sent to both channels

**Name:** `/4/rightChannel`
**Function:** Selects right channel parameters
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** If leftChannel and rightChannel are active, parameters are displayed from left channel and sent to both channels

**Name:** `/4/reqDelay`
**Function:** Delay Value
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqVolumeCorr`
**Function:** Volume Correction
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqEnable`
**Function:** Parametric EQ enable
**ScaleTypeParameter:** KOSCScaleToggle
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

**Name:** `/4/reqType1`
**Function:** Band 1 characteristics
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCPrintSelection
**Remarks:** variants Bell, Low Shelf, High Pass, Low Pass, scaled 0...1

**Name:** `/4/reqGain1`
**Function:** Band 1 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq1`
**Function:** Band 1 Freq
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ1`
**Function:** Band 1 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqGain2`
**Function:** Band 2 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq2`
**Function:** Band 2 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ2`
**Function:** Band 2 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqGain3`
**Function:** Band 3 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq3`
**Function:** Band 3 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ3`
**Function:** Band 3 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqGain4`
**Function:** Band 4 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq4`
**Function:** Band 4 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ4`
**Function:** Band 4 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqGain5`
**Function:** Band 5 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq5`
**Function:** Band 5 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ5`
**Function:** Band 5 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqGain6`
**Function:** Band 6 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq6`
**Function:** Band 6 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ6`
**Function:** Band 6 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqGain7`
**Function:** Band 7 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq7`
**Function:** Band 7 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ7`
**Function:** Band 7 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqGain8`
**Function:** Band 8 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq8`
**Function:** Band 8 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ8`
**Function:** Band 8 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqType9`
**Function:** Band 9 characteristics
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCPrintSelection
**Remarks:** variants Bell, High Shelf, Low Pass, High Pass, scaled 0...1

**Name:** `/4/reqGain9`
**Function:** Band 9 Gain
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintdB
**Remarks:** -

**Name:** `/4/reqFreq9`
**Function:** Band 9 Freq.
**ScaleTypeParameter:** KOSCScaleFreq
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/reqQ9`
**Function:** Band 9 Q
**ScaleTypeParameter:** KOSCScaleLin01
**ScaleTypeDisplayValue:** KOSCScalePrintVal
**Remarks:** -

**Name:** `/4/trackname`
**Function:** Channel Name
**ScaleTypeParameter:** KOSCScaleString
**ScaleTypeDisplayValue:** KOSCScaleNoSend
**Remarks:** -

