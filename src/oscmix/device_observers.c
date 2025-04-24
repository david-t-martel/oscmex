#include "device_observers.h"
#include <stdlib.h>
#include <string.h>

#define MAX_OBSERVERS 10

/* Observer lists */
typedef struct
{
    dsp_state_changed_cb callback;
    void *user_data;
} dsp_observer;

static dsp_observer dsp_observers[MAX_OBSERVERS];
static int dsp_observer_count = 0;

typedef struct
{
    durec_status_changed_cb callback;
    void *user_data;
} durec_observer;

static durec_observer durec_observers[MAX_OBSERVERS];
static int durec_observer_count = 0;

typedef struct
{
    samplerate_changed_cb callback;
    void *user_data;
} samplerate_observer;

static samplerate_observer samplerate_observers[MAX_OBSERVERS];
static int samplerate_observer_count = 0;

typedef struct
{
    input_changed_cb callback;
    void *user_data;
} input_observer;

static input_observer input_observers[MAX_OBSERVERS];
static int input_observer_count = 0;

typedef struct
{
    output_changed_cb callback;
    void *user_data;
} output_observer;

static output_observer output_observers[MAX_OBSERVERS];
static int output_observer_count = 0;

typedef struct
{
    mixer_changed_cb callback;
    void *user_data;
} mixer_observer;

static mixer_observer mixer_observers[MAX_OBSERVERS];
static int mixer_observer_count = 0;

/* Registration functions */
int register_dsp_observer(dsp_state_changed_cb callback, void *user_data)
{
    if (dsp_observer_count >= MAX_OBSERVERS)
        return -1;

    dsp_observers[dsp_observer_count].callback = callback;
    dsp_observers[dsp_observer_count].user_data = user_data;
    dsp_observer_count++;

    return 0;
}

int register_durec_observer(durec_status_changed_cb callback, void *user_data)
{
    if (durec_observer_count >= MAX_OBSERVERS)
        return -1;

    durec_observers[durec_observer_count].callback = callback;
    durec_observers[durec_observer_count].user_data = user_data;
    durec_observer_count++;

    return 0;
}

int register_samplerate_observer(samplerate_changed_cb callback, void *user_data)
{
    if (samplerate_observer_count >= MAX_OBSERVERS)
        return -1;

    samplerate_observers[samplerate_observer_count].callback = callback;
    samplerate_observers[samplerate_observer_count].user_data = user_data;
    samplerate_observer_count++;

    return 0;
}

int register_input_observer(input_changed_cb callback, void *user_data)
{
    if (input_observer_count >= MAX_OBSERVERS)
        return -1;

    input_observers[input_observer_count].callback = callback;
    input_observers[input_observer_count].user_data = user_data;
    input_observer_count++;

    return 0;
}

int register_output_observer(output_changed_cb callback, void *user_data)
{
    if (output_observer_count >= MAX_OBSERVERS)
        return -1;

    output_observers[output_observer_count].callback = callback;
    output_observers[output_observer_count].user_data = user_data;
    output_observer_count++;

    return 0;
}

int register_mixer_observer(mixer_changed_cb callback, void *user_data)
{
    if (mixer_observer_count >= MAX_OBSERVERS)
        return -1;

    mixer_observers[mixer_observer_count].callback = callback;
    mixer_observers[mixer_observer_count].user_data = user_data;
    mixer_observer_count++;

    return 0;
}

/* Unregistration functions */
int unregister_dsp_observer(dsp_state_changed_cb callback, void *user_data)
{
    int i, j;

    for (i = 0; i < dsp_observer_count; i++)
    {
        if (dsp_observers[i].callback == callback &&
            dsp_observers[i].user_data == user_data)
        {

            /* Compact the array by shifting remaining elements */
            for (j = i; j < dsp_observer_count - 1; j++)
            {
                dsp_observers[j] = dsp_observers[j + 1];
            }

            dsp_observer_count--;
            return 0;
        }
    }

    return -1; /* Observer not found */
}

int unregister_durec_observer(durec_status_changed_cb callback, void *user_data)
{
    int i, j;

    for (i = 0; i < durec_observer_count; i++)
    {
        if (durec_observers[i].callback == callback &&
            durec_observers[i].user_data == user_data)
        {

            /* Compact the array by shifting remaining elements */
            for (j = i; j < durec_observer_count - 1; j++)
            {
                durec_observers[j] = durec_observers[j + 1];
            }

            durec_observer_count--;
            return 0;
        }
    }

    return -1; /* Observer not found */
}

int unregister_samplerate_observer(samplerate_changed_cb callback, void *user_data)
{
    int i, j;

    for (i = 0; i < samplerate_observer_count; i++)
    {
        if (samplerate_observers[i].callback == callback &&
            samplerate_observers[i].user_data == user_data)
        {

            /* Compact the array by shifting remaining elements */
            for (j = i; j < samplerate_observer_count - 1; j++)
            {
                samplerate_observers[j] = samplerate_observers[j + 1];
            }

            samplerate_observer_count--;
            return 0;
        }
    }

    return -1; /* Observer not found */
}

int unregister_input_observer(input_changed_cb callback, void *user_data)
{
    int i, j;

    for (i = 0; i < input_observer_count; i++)
    {
        if (input_observers[i].callback == callback &&
            input_observers[i].user_data == user_data)
        {

            /* Compact the array by shifting remaining elements */
            for (j = i; j < input_observer_count - 1; j++)
            {
                input_observers[j] = input_observers[j + 1];
            }

            input_observer_count--;
            return 0;
        }
    }

    return -1; /* Observer not found */
}

int unregister_output_observer(output_changed_cb callback, void *user_data)
{
    int i, j;

    for (i = 0; i < output_observer_count; i++)
    {
        if (output_observers[i].callback == callback &&
            output_observers[i].user_data == user_data)
        {

            /* Compact the array by shifting remaining elements */
            for (j = i; j < output_observer_count - 1; j++)
            {
                output_observers[j] = output_observers[j + 1];
            }

            output_observer_count--;
            return 0;
        }
    }

    return -1; /* Observer not found */
}

int unregister_mixer_observer(mixer_changed_cb callback, void *user_data)
{
    int i, j;

    for (i = 0; i < mixer_observer_count; i++)
    {
        if (mixer_observers[i].callback == callback &&
            mixer_observers[i].user_data == user_data)
        {

            /* Compact the array by shifting remaining elements */
            for (j = i; j < mixer_observer_count - 1; j++)
            {
                mixer_observers[j] = mixer_observers[j + 1];
            }

            mixer_observer_count--;
            return 0;
        }
    }

    return -1; /* Observer not found */
}

/* Notification functions */
void notify_dsp_state_changed(int version, int load)
{
    for (int i = 0; i < dsp_observer_count; i++)
    {
        if (dsp_observers[i].callback)
        {
            dsp_observers[i].callback(version, load, dsp_observers[i].user_data);
        }
    }
}

void notify_durec_status_changed(int status, int position)
{
    for (int i = 0; i < durec_observer_count; i++)
    {
        if (durec_observers[i].callback)
        {
            durec_observers[i].callback(status, position, durec_observers[i].user_data);
        }
    }
}

void notify_samplerate_changed(int samplerate)
{
    for (int i = 0; i < samplerate_observer_count; i++)
    {
        if (samplerate_observers[i].callback)
        {
            samplerate_observers[i].callback(samplerate, samplerate_observers[i].user_data);
        }
    }
}

void notify_input_changed(int index, float gain, bool phantom, bool hiz, bool mute)
{
    for (int i = 0; i < input_observer_count; i++)
    {
        if (input_observers[i].callback)
        {
            input_observers[i].callback(index, gain, phantom, hiz, mute,
                                        input_observers[i].user_data);
        }
    }
}

void notify_output_changed(int index, float volume, bool mute)
{
    for (int i = 0; i < output_observer_count; i++)
    {
        if (output_observers[i].callback)
        {
            output_observers[i].callback(index, volume, mute,
                                         output_observers[i].user_data);
        }
    }
}

void notify_mixer_changed(int input, int output, float volume, float pan)
{
    for (int i = 0; i < mixer_observer_count; i++)
    {
        if (mixer_observers[i].callback)
        {
            mixer_observers[i].callback(input, output, volume, pan,
                                        mixer_observers[i].user_data);
        }
    }
}

// Add this implementation

int get_observer_status(bool *dsp_active, bool *durec_active, bool *samplerate_active,
                        bool *input_active, bool *output_active, bool *mixer_active)
{
    int count = 0;

    if (dsp_active)
    {
        *dsp_active = (dsp_observer_count > 0);
        if (*dsp_active)
            count++;
    }

    if (durec_active)
    {
        *durec_active = (durec_observer_count > 0);
        if (*durec_active)
            count++;
    }

    if (samplerate_active)
    {
        *samplerate_active = (samplerate_observer_count > 0);
        if (*samplerate_active)
            count++;
    }

    if (input_active)
    {
        *input_active = (input_observer_count > 0);
        if (*input_active)
            count++;
    }

    if (output_active)
    {
        *output_active = (output_observer_count > 0);
        if (*output_active)
            count++;
    }

    if (mixer_active)
    {
        *mixer_active = (mixer_observer_count > 0);
        if (*mixer_active)
            count++;
    }

    return count;
}
