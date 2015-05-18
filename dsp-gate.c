#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dsp.h"

enum gate_status {gate_open, gate_closed, gate_release, gate_attack};

struct qdsp_gate_state_t {
    enum gate_status status[NCHANNELS_MAX];
    float threshold;
    float hold;
    unsigned int holdcount[NCHANNELS_MAX];
};

void gate_process(struct qdsp_t * dsp)
{
    struct qdsp_gate_state_t * state = (struct qdsp_gate_state_t *)dsp->state;
    unsigned int holdthresh = (state->hold * dsp->fs) / dsp->nframes;
    int i,n;
    float gain, gainstep;

    for (i=0; i<dsp->nchannels; i++) {
        const float * inbuf = dsp->inbufs[i];
        float * outbuf = dsp->outbufs[i];

        switch (state->status[i]) {
            default:
            case gate_open:
                for (n=0; n<dsp->nframes; n++) {
                    if (fabsf(inbuf[n]) > state->threshold) {
                        state->holdcount[i] = 0;
                        state->status[i] = gate_open;
                        break;
                    }
                }
                if (n == dsp->nframes) {
                    state->holdcount[i]++;
                    if (state->holdcount[i] >= holdthresh) {
                        state->status[i] = gate_attack;
                    }
                }
                memcpy(outbuf, inbuf, dsp->nframes*sizeof(float));
                DEBUG3("%s: open, holdcount=%i, holdthresh=%i\n", __func__, state->holdcount[i], holdthresh);
                break;

            case gate_closed:
                for (n=0; n<dsp->nframes; n++) {
                    if (fabsf(inbuf[n]) > state->threshold) {
                        state->holdcount[i] = 0;
                        state->status[i] = gate_release;
                        break;
                    }
                }
                memcpy(outbuf, dsp->zerobuf, dsp->nframes*sizeof(float));
                DEBUG3("%s: closed\n", __func__);
                break;

            case gate_attack:
                gainstep = 1.0f / (dsp->nframes-1);
                gain = 1.0f;
                for (n=0; n<dsp->nframes; n++) {
                    outbuf[n] = gain * inbuf[n];
                    gain -= gainstep;
                }
                state->status[i] = gate_closed;
                DEBUG3("%s: attack\n", __func__);
                break;

            case gate_release:
                gainstep = 1.0f / (dsp->nframes-1);
                gain = 0;
                for (n=0; n<dsp->nframes; n++) {
                    outbuf[n] = gain * inbuf[n];
                    gain += gainstep;
                }
                state->status[i] = gate_open;
                DEBUG3("%s: release\n", __func__);
                break;
        }
    }
}

void gate_init(struct qdsp_t * dsp)
{
	struct qdsp_gate_state_t * state = (struct qdsp_gate_state_t *)dsp->state;

    for (int i=0; i<NCHANNELS_MAX; i++) {
        state->holdcount[i] = 0;
        state->status[i] = gate_open;
    }
}

void destroy_gate(struct qdsp_t * dsp)
{
    free(dsp->state);
}

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
    state->threshold=0;
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
            state->threshold = powf(10.0f, atof(value) / 20.0f);
            debugprint(1, "%s: threshold=%f\n", __func__, atof(value));
            break;
        case HOLDTIME_OPT:
            if (value == NULL) {
                debugprint(0, "Missing value for suboption '%s'\n", token[HOLDTIME_OPT]);
                errfnd = 1;
                continue;
            }
            state->hold = atof(value);
            debugprint(1, "%s: holdtime=%d\n", __func__, atoi(value));
            break;
        default:
            debugprint(0, "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
    if (state->threshold == 0) {
        debugprint(0, "%s: Missing value for threshold.\n", __func__);
        errfnd = 1;
    }

    dsp->process = gate_process;
    dsp->init = gate_init;
    dsp->destroy = destroy_gate;

    return errfnd;
}

void help_gate(void)
{
    debugprint(0, "  Gate options\n");
    debugprint(0, "    Name: gate\n    t=threshold (dBFS)\n    h=holdtime (sec)\n");
    debugprint(0, "    Example: -p gate,t=-80,h=30\n");
}
