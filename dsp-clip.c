#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "dsp.h"


struct qdsp_clip_state_t {
    float threshold;
};

void clip_process(struct qdsp_t * dsp)
{
    struct qdsp_clip_state_t * state = (struct qdsp_clip_state_t *)dsp->state;

    for (int i=0; i<dsp->nchannels; i++) {
        for (int n=0; n<dsp->nframes; n++) {
            if (dsp->inbufs[i][n] < -state->threshold)
                dsp->outbufs[i][n] = -state->threshold;
            else if (dsp->inbufs[i][n] > state->threshold)
                dsp->outbufs[i][n] = state->threshold;
            else
                dsp->outbufs[i][n] = dsp->inbufs[i][n];
        }
    }
}

void clip_init(struct qdsp_t * dsp)
{}

int create_clip(struct qdsp_t * dsp, char ** subopts)
{
    enum {
        THRESHOLD_OPT = 0,
    };
    char *const token[] = {
        [THRESHOLD_OPT]   = "t",
        NULL
    };
    char *value;
    char *name = NULL;
    int errfnd = 0;
    struct qdsp_clip_state_t * state = malloc(sizeof(struct qdsp_clip_state_t));
    dsp->state = (void*)state;


    fprintf(stderr,"%s subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        switch (getsubopt(subopts, token, &value)) {
        case THRESHOLD_OPT:
            if (value == NULL) {
                fprintf(stderr, "Missing value for suboption '%s'\n", token[THRESHOLD_OPT]);
                errfnd = 1;
                continue;
            }
            state->threshold = atof(value);
            fprintf(stderr,"threshold=%f\n",atof(value));
            break;
        default:
            fprintf(stderr, "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
    dsp->process = clip_process;
    dsp->init = clip_init;

    return errfnd;
}

void help_clip(void)
{
    fprintf(stderr,"  Clip options\n");
    fprintf(stderr,"    Name: clip\n    t=clip threshold (linear)\n");
    fprintf(stderr,"    Example: -p clip,t=1.0\n");
}
