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
