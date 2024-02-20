#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dsp.h"


struct qdsp_gate_state_t {
    float threshold;
    unsigned int hold;
    unsigned int holdcount[NCHANNELS_MAX];
};

void gate_process(struct qdsp_t * dsp)
{
    struct qdsp_gate_state_t * state = (struct qdsp_gate_state_t *)dsp->state;
    unsigned int holdthresh = (state->hold * dsp->fs) / dsp->nframes;
    int i,n;

    for (i=0; i<dsp->nchannels; i++) {
        float max = 0;
        const float * inbuf = dsp->inbufs[i];
        for (n=0; n<dsp->nframes; n++) {
            if (max < fabsf(inbuf[n]))
                max = fabsf(inbuf[n]);
            if (max > state->threshold) break;
        }

        if (state->holdcount[i] < holdthresh) {
            // open
            if (dsp->inbufs[i] != dsp->outbufs[i]) {
                memcpy(dsp->outbufs[i], dsp->inbufs[i], dsp->nframes*sizeof(float));
            }
        }
        else if (state->holdcount[i] == holdthresh) {
            // attack
            float gainstep = 1.0f / dsp->nframes;
            const float * inbuf = dsp->inbufs[i];
            float * outbuf = dsp->outbufs[i];
            float gain = 1.0f;
            for (n=0; n<dsp->nframes; n++) {
                outbuf[n] = gain * inbuf[n];
                gain -= gainstep;
            }
            DEBUG3("%s: attack\n", __func__);
        }
        else if (state->holdcount[i] > holdthresh && max > state->threshold) {
            // release
            float gainstep = 1.0f / dsp->nframes;
            const float * inbuf = dsp->inbufs[i];
            float * outbuf = dsp->outbufs[i];
            float gain = 0;
            for (n=0; n<dsp->nframes; n++) {
                outbuf[n] = gain * inbuf[n];
                gain += gainstep;
            }
            DEBUG3("%s: release\n", __func__);
        }
        else { //dsp->holdcount > holdthresh && max < dsp->threshold
            // closed
            state->holdcount[i] = holdthresh;
            memcpy(dsp->outbufs[i], dsp->zerobuf, dsp->nframes*sizeof(float));
            //dsp->outbufs[i] = dsp->zerobuf;
        }

        if (dsp->sequencecount % (dsp->fs/dsp->nframes) == 0) {
            DEBUG3("%s: max: %.2f, holdthresh: %d, count: %d\n", __func__, 20*log10f(max), holdthresh, state->holdcount[i]);
        }

        if (max > state->threshold)
            state->holdcount[i] = 0;
        else
            state->holdcount[i]++;
    }
}

void gate_init(struct qdsp_t * dsp)
{}


int create_gate(struct qdsp_t * dsp, char ** subopts)
{
    enum {
        THRESHOLD_OPT = 0,
        HOLDTIME_OPT
    };
    char *const token[] = {
        [THRESHOLD_OPT]   = "t",
        [HOLDTIME_OPT]   = "h",
        NULL
    };
    char *value;
    int errfnd = 0;
    struct qdsp_gate_state_t * state = malloc(sizeof(struct qdsp_gate_state_t));
    dsp->state = (void*)state;
    state->hold=0;

    debugprint(1, "%s: subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        switch (getsubopt(subopts, token, &value)) {
        case THRESHOLD_OPT:
            if (value == NULL) {
                debugprint(0, "Missing value for suboption '%s'\n", token[THRESHOLD_OPT]);
                errfnd = 1;
                continue;
            }
            state->threshold = powf(10,atof(value)/20);
            debugprint(0, "%s: threshold=%f\n", __func__, atof(value));
            break;
        case HOLDTIME_OPT:
            if (value == NULL) {
                debugprint(0, "Missing value for suboption '%s'\n", token[HOLDTIME_OPT]);
                errfnd = 1;
                continue;
            }
            state->hold = atoi(value);
            debugprint(0, "%s: holdtime=%d\n", __func__, atoi(value));
            break;
        default:
            debugprint(0, "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
    for (int i=0; i<NCHANNELS_MAX; i++)
        state->holdcount[i] = 0;
    dsp->process = gate_process;
    dsp->init = gate_init;

    return errfnd;
}

void help_gate(void)
{
    debugprint(0, "  Gate options\n");
    debugprint(0, "    Name: gate\n    t=threshold (dBFS)\n    h=holdtime (sec)\n");
    debugprint(0, "    Example: -p gate,t=-80,h=30\n");
}
