#ifndef DEVICE_H
#define DEVICE_H

#include <stdbool.h>

/* Device capability flags */
#define DEVICE_DUREC 0x00000001	 /* Direct USB Recording */
#define DEVICE_ROOMEQ 0x00000002 /* Room EQ */

/* Input flags */
#define INPUT_GAIN 0x00000001	  /* Has gain control */
#define INPUT_48V 0x00000002	  /* Has phantom power */
#define INPUT_REFLEVEL 0x00000004 /* Has reference level selection */
#define INPUT_HIZ 0x00000008	  /* Has Hi-Z impedance option */
#define INPUT_PAD 0x00000010	  /* Has pad */
#define INPUT_AEB 0x00000020	  /* Has AEB (Auto-EQ-Balance) */
#define INPUT_LOCUT 0x00000040	  /* Has low cut filter */
#define INPUT_MS 0x00000080		  /* Has M/S processing */
#define INPUT_AUTOSET 0x00000100  /* Has auto-set function */

/* Output flags */
#define OUTPUT_VOLUME 0x00000001   /* Has volume control */
#define OUTPUT_MUTE 0x00000002	   /* Has mute */
#define OUTPUT_REFLEVEL 0x00000004 /* Has reference level selection */
#define OUTPUT_DITHER 0x00000008   /* Has dither */
#define OUTPUT_PHASE 0x00000010	   /* Has phase inversion */
#define OUTPUT_MONO 0x00000020	   /* Has mono mode */
#define OUTPUT_LOOPBACK 0x00000040 /* Has loopback */

/* Register map definitions */
/* System settings */
#define REG_SAMPLE_RATE 0x8000
#define REG_CLOCK_SOURCE 0x8002
#define REG_BUFFER_SIZE 0x8004
#define REG_PHANTOM_POWER 0x8006
#define REG_MASTER_VOLUME 0x8008
#define REG_MASTER_MUTE 0x800A
#define REG_DIGITAL_GAIN 0x800C

/* Hardware status */
#define REG_DSP_STATUS 0x3E04
#define REG_DSP_VERSION 0x3E05
#define REG_TEMPERATURE 0x3E06
#define REG_CLOCK_STATUS 0x3E08
#define REG_INPUT_SIGNAL 0x3E0A
#define REG_OUTPUT_SIGNAL 0x3E0C

/* DURec status and control */
#define REG_DUREC_STATUS 0x3E90
#define REG_DUREC_POSITION 0x3E91
#define REG_DUREC_TIME 0x3E92
#define REG_DUREC_USB_STATUS 0x3E93
#define REG_DUREC_TOTAL_SPACE 0x3E94
#define REG_DUREC_FREE_SPACE 0x3E95
#define REG_DUREC_FILE 0x3E96
#define REG_DUREC_NEXT 0x3E97
#define REG_DUREC_FILES_LEN 0x3E98
#define REG_DUREC_RECORD_TIME 0x3E99
/* DURec commands */
#define REG_DUREC_RECORD 0x3EA0
#define REG_DUREC_STOP 0x3EA1
#define REG_DUREC_PLAY 0x3EA2
#define REG_DUREC_PAUSE 0x3EA3
#define REG_DUREC_PREV 0x3EA4
#define REG_DUREC_PLAYMODE 0x3EA5

/* Input channel registers */
#define REG_INPUT_GAIN 0x0100
#define REG_INPUT_PHANTOM 0x0102
#define REG_INPUT_PAD 0x0104
#define REG_INPUT_REFLEVEL 0x0106
#define REG_INPUT_MUTE 0x0108
#define REG_INPUT_HIZ 0x010A
#define REG_INPUT_AEB 0x010C
#define REG_INPUT_LOCUT 0x010E
#define REG_INPUT_MS 0x0110
#define REG_INPUT_AUTOSET 0x0112

/* Output channel registers */
#define REG_OUTPUT_VOLUME 0x0200
#define REG_OUTPUT_MUTE 0x0202
#define REG_OUTPUT_REFLEVEL 0x0204
#define REG_OUTPUT_DITHER 0x0206
#define REG_OUTPUT_PHASE 0x0208
#define REG_OUTPUT_MONO 0x020A
#define REG_OUTPUT_LOOPBACK 0x020C

/* Playback channel registers */
#define REG_PLAYBACK_VOLUME 0x0300
#define REG_PLAYBACK_MUTE 0x0302
#define REG_PLAYBACK_PHASE 0x0304

/* Mixer matrix registers */
#define REG_MIXER_VOLUME 0x0400
#define REG_MIXER_PAN 0x0402
#define REG_MIXER_MUTE 0x0404
#define REG_MIXER_SOLO 0x0406
#define REG_MIXER_PHASE 0x0408

/* TotalMix registers */
#define REG_TOTALMIX_LOAD 0x7000
#define REG_TOTALMIX_SAVE 0x7002
#define REG_TOTALMIX_CLEARALL 0x7004

/* Log and error control */
#define REG_LOG_DEBUG 0x9000
#define REG_LOG_LEVEL 0x9001
#define REG_LOG_CLEAR 0x9002
#define REG_ERROR_CLEAR 0x9012

/* Enumeration names - declared here, defined in device.c */
extern const char *const CLOCK_SOURCE_NAMES[];
extern const char *const BUFFER_SIZE_NAMES[];
extern const char *const DUREC_STATUS_NAMES[];
extern const char *const DUREC_PLAYMODE_NAMES[];
extern const char *const INPUT_REF_LEVEL_NAMES[];
extern const char *const OUTPUT_REF_LEVEL_NAMES[];
extern const char *const DITHER_NAMES[];
extern const char *const LOG_LEVEL_NAMES[];

/* Input channel state structure */
struct input_state
{
	float gain;
	bool phantom;
	bool hiz;
	bool pad;
	bool mute;
	bool stereo;
	int reflevel;
};

/* Output channel state structure */
struct output_state
{
	float volume;
	bool mute;
	bool stereo;
	int reflevel;
};

/* Input channel descriptor */
struct inputinfo
{
	const char *name;
	unsigned int flags;
};

/* Output channel descriptor */
struct outputinfo
{
	const char *name;
	unsigned int flags;
};

/* Device descriptor structure */
struct device
{
	const char *name;
	int inputslen;
	int outputslen;
	int playbacklen;
	int mixerlen;
	unsigned int flags;
	const struct inputinfo *inputs;
	const struct outputinfo *outputs;
};

/* Global device reference */
extern const struct device *cur_device;

/* Device initialization function */
int device_init(const char *name);

/* Device get/set functions */
int get_dsp_state(int *version, int *load);
int get_durec_status(int *status, int *position);
int get_sample_rate(int *rate);

/* Input/output parameter get functions */
int get_input_params(int channel, float *gain, bool *phantom, bool *hiz, bool *mute);
int get_output_params(int channel, float *volume, bool *mute);
int get_mixer_state(int input, int output, float *volume, float *pan);

/* State retrieval functions - return full struct */
struct input_state *get_input_state_struct(int index);
struct output_state *get_output_state_struct(int index);

/* Update functions */
int update_sample_rate(int val);
int update_dsp_status(int val);
int update_durec_status(int val);
int update_input_parameter(int channel, int param, int val);
int set_durec_state(int status, int position, int time, int usb_errors, int usb_load,
					float total_space, float free_space, int file, int record_time,
					int prev, int next, int playmode);
int set_durec_files_length(int length);
int refreshing_state(int new_state);

/* Device accessor */
const struct device *getDevice(void);

#endif /* DEVICE_H */
