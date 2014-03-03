#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "dsp.h"


struct qdsp_gain_state_t {
    float gain;
};

void gain_process(void * arg)
{
    struct qdsp_t * dsp = (struct qdsp_t *)arg;
    struct qdsp_gain_state_t * state = (struct qdsp_gain_state_t *)dsp->state;

    for (int i=0; i<dsp->nchannels; i++) {
        for (int n=0; n<dsp->nframes; n++) {
            dsp->outbufs[i][n] = state->gain * dsp->inbufs[i][n];
        }
    }
}


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


    fprintf(stderr,"%s subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        switch (getsubopt(subopts, token, &value)) {
        case GAIN_OPT:
            if (value == NULL) {
                fprintf(stderr, "Missing value for "
                        "suboption '%s'\n", token[GAIN_OPT]);
                errfnd = 1;
                continue;
            }
            state->gain = atof(value);
            fprintf(stderr,"gain=%f\n",atof(value));
            break;
        default:
            fprintf(stderr, "%s: No match found "
                    "for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
    dsp->process = gain_process;

    return errfnd;
}
