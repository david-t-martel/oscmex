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

/* Output flags */
#define OUTPUT_REFLEVEL 0x00000001 /* Has reference level selection */

/* Register map definitions */
#define REG_SAMPLE_RATE 0x8000
#define REG_DSP_STATUS 0x3e04
#define REG_DUREC_STATUS 0x3e90
#define REG_DUREC_TIME 0x3e91
#define REG_DUREC_USB_STATUS 0x3e92
#define REG_DUREC_TOTAL_SPACE 0x3e93
#define REG_DUREC_FREE_SPACE 0x3e94
#define REG_DUREC_FILE 0x3e95
#define REG_DUREC_NEXT 0x3e96
#define REG_DUREC_FILES_LEN 0x3e97
#define REG_DUREC_RECORD_TIME 0x3e98

/* Macro to get input register */
#define REG_INPUT(ch, param) ((ch << 6) | (param))
#define REG_INPUT_VOLUME 0
#define REG_INPUT_PAN 1
#define REG_INPUT_STEREO 2
#define REG_INPUT_MUTE 3
#define REG_INPUT_48V_HIZ 4

/* Similar macros for output and mixer registers */

/* Common constants and enumerations */

/* Sample rate definitions */
#define SAMPLE_RATE_COUNT 10
extern const long SAMPLE_RATES[SAMPLE_RATE_COUNT];

/* DURec status definitions */
enum durec_status
{
	DUREC_NO_MEDIA = 0,
	DUREC_FILESYSTEM_ERROR = 1,
	DUREC_INITIALIZING = 2,
	DUREC_REINITIALIZING = 3,
	DUREC_STOPPED = 5,
	DUREC_RECORDING = 6,
	DUREC_PLAYING = 10,
	DUREC_PAUSED = 11
};
extern const char *const DUREC_STATUS_NAMES[];

/* Playback mode definitions */
enum durec_playmode
{
	DUREC_PLAYMODE_SINGLE = 0,
	DUREC_PLAYMODE_REPEAT = 1,
	DUREC_PLAYMODE_SEQUENCE = 2,
	DUREC_PLAYMODE_RANDOM = 3
};
extern const char *const DUREC_PLAYMODE_NAMES[];

/* Input/Output reference level definitions */
enum ref_level
{
	REF_LEVEL_LO_GAIN = 0,
	REF_LEVEL_PLUS_4DBU = 1,
	REF_LEVEL_MINUS_10DBV = 2,
	REF_LEVEL_HI_GAIN = 3
};
extern const char *const INPUT_REF_LEVEL_NAMES[];
extern const char *const OUTPUT_REF_LEVEL_NAMES[];

/**
 * @brief Input channel information structure
 */
struct inputinfo
{
	char name[32]; /* Input channel name */
	int flags;	   /* Input capabilities */
};

/**
 * @brief Output channel information structure
 */
struct outputinfo
{
	const char *name; /* Output channel name */
	int flags;		  /* Output capabilities */
};

/**
 * @brief Audio device structure
 */
struct device
{
	const char *id;	  /* Device identifier */
	const char *name; /* Device name */
	int version;	  /* Device version */
	int flags;		  /* Device capabilities */

	const struct inputinfo *inputs; /* Array of input channels */
	int inputslen;					/* Number of input channels */

	const struct outputinfo *outputs; /* Array of output channels */
	int outputslen;					  /* Number of output channels */
};

/**
 * @brief Initialize the device subsystem
 *
 * @return 0 on success, non-zero on failure
 */
int device_init(void);

/**
 * @brief Clean up the device subsystem
 */
void device_cleanup(void);

/**
 * @brief Get a pointer to the current device
 *
 * @return Pointer to the current device structure
 */
const struct device *get_current_device(void);

#endif /* DEVICE_H */
