# RME Interface OSC Command Set Configuration

This document details how to use OSC (Open Sound Control) commands to configure RME audio interfaces, specifically targeting the Fireface UCX II and potentially the UFX II, via the `oscmix` software. This document includes examples derived from `src/oscmix/tools/rme.lua`.

## Overview

The `oscmix` software acts as a bridge between OSC messages and the RME audio interface. It translates OSC commands into MIDI SysEx messages that the RME device understands. To control your RME device, you will need to send OSC messages to `oscmix`, which then relays them to the device.

## General OSC Protocol Concepts

* **Messages:** OSC messages consist of an address pattern (or path) and optionally some arguments.
* **Addresses/Paths:** Addresses are hierarchical strings that describe the target of a command. For example, `/output/1/volume` might indicate the volume level of output channel 1.
* **Arguments:** Arguments are the data values associated with the command. These could be numbers (integers or floats), strings, etc.
* **Data Types**: OSC specifies data types for arguments. Common types include:
  * `i`: integer
  * `f`: float
  * `s`: string
  * `b`: blob (binary data)
* **UDP:** OSC often uses UDP (User Datagram Protocol) for communication, which is fast but not always reliable. This means messages may be lost.

## `oscmix` Specifics

* **MIDI SysEx:** `oscmix` relies on MIDI SysEx messages for communication with RME devices, which implies that the OSC command set will be closely tied to the underlying SysEx structure of these devices.
* **File Descriptors:** `oscmix` needs read and write file descriptors for MIDI communication, typically set up using `alsarawio` or `alsaseqio` (on Linux). These are usually file descriptors 6 and 7.
* **Default Ports:** By default, `oscmix` listens for OSC messages on UDP port 7222 (`127.0.0.1:7222`) and sends OSC messages on UDP port 8222 (`127.0.0.1:8222`).

## OSC Command Structure (Inferred)

**Note:** The exact OSC paths and arguments that `oscmix` understands are primarily derived from the `oscmix.c` file, the provided examples in `src/oscmix/tools/rme.lua`, and assumptions based on RME mixer structure. This document will be updated as more information is found in the source code.

### General Structure

OSC paths in `oscmix` generally follow a hierarchical structure:
/category/sub-category/parameter

Where:

* `category` refers to a part of the RME device (e.g., `/input`, `/output`, `/mixer`, `/phones`, `/dsp`).
* `sub-category` refers to a specific channel, bus, setting, or DSP parameter (e.g., `/1`, `/2`, `/main`, `/a`, `/b`, `/eq1`, `/lowcut`).
* `parameter` is the specific control to modify (e.g., `/volume`, `/mute`, `/gain`, `/level`, `/enable`, `/freq`).

### RME Specific OSC Paths (with examples from `rme.lua`)

Here's a more detailed list of potential OSC paths, based on RME mixer structure, SysEx messages, and examples found in `src/oscmix/tools/rme.lua`.  Note that these are based on the Lua code, so it is highly likely that they are correct.

* **Input Channels**
  * `/input/[1-12]/gain`:  Input gain for channels 1-12 (float, in dB).
  * `/input/[1-12]/trim`: Input trim for channels 1-12 (float, in dB).
  * `/input/[1-12]/mute`: Mute control for channels 1-12 (integer, 0=off, 1=on).
  * `/input/[1-12]/phantom`: Phantom power control for channels 1-12 (integer, 0=off, 1=on). Note that some inputs may not have phantom power.
  * `/input/[1-12]/stereo`: Stereo link control for channels 1-12 (integer, 0=off, 1=on).
  * `/input/[1-12]/invert`: Polarity invert control for channels 1-12 (integer, 0=off, 1=on).
  * `/input/[1-12]/instrument`:  Instrument input mode for channels 1-12 (integer, 0=off, 1=on).
* **Output Channels**
  * `/output/[1-12]/volume`: Output volume for channels 1-12 (float, in dB).
  * `/output/[1-12]/mute`: Mute for output channels 1-12 (integer, 0=off, 1=on).
  * `/output/[1-12]/source`: The source of the output channel (integer corresponding to input or mixer, check the manual for the full mapping).
* **Mixer Channels (Sends)**
  * `/mixer/[1-4]/send/[1-4]`: Send level from mixer 1-4 to send buses 1-4 (float, in dB).
  * `/mixer/[1-4]/pan`: Pan position for mixer channel 1-4 (float, from -1.0 to 1.0).
  * `/mixer/[1-4]/volume`: Volume for mixer channel 1-4 (float, in dB).
  * `/mixer/[1-4]/mute`: Mute control for mixer channels 1-4 (integer, 0=off, 1=on).
  * `/mixer/[1-4]/source`: The source of the mixer channel (integer corresponding to input or other mixer channel, check the manual for the full mapping).
* **Phones Outputs**
  * `/phones/a/volume`: Volume of headphone output A (float, in dB).
  * `/phones/b/volume`: Volume of headphone output B (float, in dB).
  * `/phones/a/source`: The source of headphone output A (integer corresponding to input or mixer, check the manual for the full mapping).
  * `/phones/b/source`: The source of headphone output B (integer corresponding to input or mixer, check the manual for the full mapping).
* **DSP Parameters**
  * `/dsp/lowcut/enable`: Enable/disable the low cut filter (integer, 0=off, 1=on). See line 140 of `rme.lua` for an example of the 'Low Cut Enable' parameter and its formatting using the function `format_bool`.
  * `/dsp/lowcut/freq`:  Frequency of the low cut filter (integer, in Hz, or some device-specific units) See line 141 of `rme.lua` for an example of the 'Low Cut Freq' parameter, and its formatting using the function `format_int`.
  * `/dsp/lowcut/db_oct`: Low cut filter slope in dB/octave (integer or enum, e.g., 6, 12, 18, 24). See line 142 of `rme.lua` for an example of the 'Low Cut dB/oct' parameter and its formatting using the function `format_enum`.
  * `/dsp/eq1/enable`: Enable/disable the first parametric EQ band (integer, 0=off, 1=on).
  * `/dsp/eq1/type`: Type of EQ band (integer or enum based on `bandtypes` in line 137 of `rme.lua`, like "Peak", "Shelf", "High Cut").
  * `/dsp/eq1/freq`: Center frequency of EQ band 1 (integer or float, in Hz, or device-specific units).
  * `/dsp/eq1/gain`: Gain of EQ band 1 (float, in dB).
  * `/dsp/eq1/q`: Q factor of EQ band 1 (float or device-specific units).
  * Similarly, `/dsp/eq[2-4]/...` for other EQ bands.
* **Global Device Settings**
  * `/device/sampleRate`:  Set the sample rate of the device (integer, in Hz, e.g., 44100, 48000, 96000).
  * `/device/clockSource`: Set the clock source of the device (integer, check device manual).
  * `/device/loopback`: Set the loopback mode on or off (integer, 0=off, 1=on).
  * `/device/inputGainMode`: Set the input gain mode, possibly different values for analog, digital, etc (integer, check device manual).
  * `/device/cue`: Set the cue mix output for each channel (formatted as "from to" or "No Cue", see line 106 of `rme.lua` for an example of the `format_cue` function).
  * `/device/controlroom`: Set various control room settings for the device (formatted as a comma separated string, see line 115 of `rme.lua` for an example of the `format_controlroom` function).
* **FX Load**
  * `/fx/load`: Set the loaded FX preset (integer, see line 125 of `rme.lua` for the `format_fxload` function).

### Data Types

The data type of each argument is typically one of the following:

* **Integers:** Used for discrete values, flags (e.g., mute on/off), or enumerated types (e.g., sample rate, clock source, lowcut slope).
* **Floats:** Used for continuous analog controls like gain, volume, send levels (typically in decibels), and frequencies or Q factors.
* **Strings:** May be used for device names, routing selections, or status strings. The Lua `format_controlroom` and `format_cue` functions provide examples of output strings that may be present as incoming OSC messages.

### How to Determine Actual OSC Paths

1. **Inspect `oscmix.c`:** The primary source of truth for the OSC command set is the `oscmix.c` file in the repository. Analyze the code to identify OSC address patterns, the data types expected, and corresponding MIDI SysEx generation logic. Trace the code from OSC message reception to MIDI SysEx message creation.
2. **OSC Monitoring:** Use an OSC monitoring tool to intercept OSC messages sent to `oscmix`. Send test messages to observe the device responses to different addresses and data types.
3. **RME Manuals**: RME manuals often contain details of the sysex commands for the devices. Combining the information from the RME manual with the analysis of `oscmix.c` will likely be the most complete method of documenting the command set.
4. **Examine `rme.lua`:** The `rme.lua` file provides many formatting functions (such as `format_bool`, `format_int`, `format_enum`, `format_cue`, `format_controlroom`, `format_fxload`, `format_samplerate`, `format_durecinfo`), which provide some insight as to the format of the data for OSC commands.

## Example OSC Communication (Conceptual)

Here's an example of setting the gain of input channel 1 to 6 dB:

1. **MATLAB Client**: Creates an OSC message:

~~~matlab
    % MATLAB code
    osc_ip = '127.0.0.1';
    osc_port_send = 8222; % Default oscmix send port
    osc_address = osc_new_address(osc_ip, osc_port_send);
    gain_db = 6.0;
    osc_message = struct('path', '/input/1/gain', 'data', {{gain_db}});
    osc_send(osc_address, osc_message);
