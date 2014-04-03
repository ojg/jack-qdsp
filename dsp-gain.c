#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include "dsp.h"


struct qdsp_gain_state_t {
    float gain;
};

void gain_process(struct qdsp_t * dsp)
{
    struct qdsp_gain_state_t * state = (struct qdsp_gain_state_t *)dsp->state;

    for (int i=0; i<dsp->nchannels; i++) {
        for (int n=0; n<dsp->nframes; n++) {
            dsp->outbufs[i][n] = state->gain * dsp->inbufs[i][n];
        }
    }
}

void gain_init(struct qdsp_t * dsp)
{}

int create_gain(struct qdsp_t * dsp, char ** subopts)
{
    enum {
        GAIN_OPT = 0,
    };
    char *const token[] = {
        [GAIN_OPT]   = "g",
        NULL
    };
    char *value;
    char *name = NULL;
    int errfnd = 0;
    struct qdsp_gain_state_t * state = malloc(sizeof(struct qdsp_gain_state_t));
    dsp->state = (void*)state;


    debugprint(1, "%s subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        switch (getsubopt(subopts, token, &value)) {
        case GAIN_OPT:
            if (value == NULL) {
                debugprint(0, "Missing value for suboption '%s'\n", token[GAIN_OPT]);
                errfnd = 1;
                continue;
            }
            state->gain = atof(value);
            debugprint(0, "%s: gain=%f\n", __func__, atof(value));
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
    debugprint(0, "    Example: -p gain,g=0.5\n");
}
