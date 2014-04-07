#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "dsp.h"


struct qdsp_gain_state_t {
    float gain;
    float delay_seconds;
    unsigned int delay_samples;
    float * delayline;
};

void gain_process(struct qdsp_t * dsp)
{
    struct qdsp_gain_state_t * state = (struct qdsp_gain_state_t *)dsp->state;
    unsigned int i, k=0, n=0;

    if (state->delay_samples > dsp->nframes) {
        endprogram("gain_process: Error: Delay is longer than framesize\n");
    }

    for (i=0; i<dsp->nchannels; i++) {
        float * restrict delayline = &state->delayline[state->delay_samples * i];
        for (n=0, k=dsp->nframes - state->delay_samples; n<state->delay_samples; n++, k++) {
            dsp->outbufs[i][n] = state->gain * delayline[n];
            delayline[n] = dsp->inbufs[i][k];
        }
        debugprint(3, "i=%p, o=%p, n=%d\t", dsp->inbufs[i], dsp->outbufs[i], n);
        for (k=0; n<dsp->nframes; n++, k++) {
            dsp->outbufs[i][n] = state->gain * dsp->inbufs[i][k];
        }
        debugprint(3, "k=%d\n", k);
    }
}

void gain_init(struct qdsp_t * dsp)
{
    struct qdsp_gain_state_t * state = (struct qdsp_gain_state_t *)dsp->state;
    state->delay_samples = state->delay_seconds * dsp->fs;
    debugprint(2, "%s: delay_samples=%d\n", __func__, state->delay_samples);
    state->delayline = (float*)calloc(state->delay_samples * dsp->nchannels, sizeof(float));
}

int create_gain(struct qdsp_t * dsp, char ** subopts)
{
    enum {
        GAIN_OPT = 0,
        DELAY_OPT,
    };
    char *const token[] = {
        [GAIN_OPT]   = "g",
        [DELAY_OPT]  = "d",
        NULL
    };
    char *value;
    char *name = NULL;
    int errfnd = 0;
    struct qdsp_gain_state_t * state = malloc(sizeof(struct qdsp_gain_state_t));
    dsp->state = (void*)state;

    // default values
    state->delay_seconds = 0;
    state->gain = 1.0f;

    debugprint(1, "%s subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        switch (getsubopt(subopts, token, &value)) {
        case GAIN_OPT:
            if (value == NULL) {
                debugprint(0, "%s: Missing value for suboption '%s'\n", __func__, token[GAIN_OPT]);
                errfnd = 1;
                continue;
            }
            state->gain = atof(value);
            debugprint(1, "%s: gain=%f\n", __func__, atof(value));
            break;
        case DELAY_OPT:
            if (value == NULL) {
                debugprint(0, "%s: Missing value for suboption '%s'\n", __func__, token[DELAY_OPT]);
                errfnd = 1;
                continue;
            }
            state->delay_seconds = atof(value);
            debugprint(1, "%s: delay_seconds=%f\n", __func__, atof(value));
            break;
        default:
            debugprint(0, "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
    dsp->process = gain_process;
    dsp->init = gain_init;

    return errfnd;
}

void help_gain(void)
{
    debugprint(0, "  Gain options\n");
    debugprint(0, "    Name: gain\n    g=gain value (linear)\n");
    debugprint(0, "    Name: gain\n    d=delay value (seconds)\n");
    debugprint(0, "    Example: -p gain,g=0.5,d=0.002\n");
}
