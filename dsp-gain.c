#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dsp.h"


struct qdsp_gain_state_t {
    float gain;
    float delay_seconds;
    int delay_samples;
    float * delayline;
    int offset;
    float clip_threshold;
};

static inline float gain_and_clip_sample(float sample, float gain, float clip_threshold)
{
    return fmaxf(fminf(gain * sample, clip_threshold), -clip_threshold);
}

void gain_process(struct qdsp_t * dsp)
{
    struct qdsp_gain_state_t * state = (struct qdsp_gain_state_t *)dsp->state;
    int i, k=0, n=0;

    if (state->delay_samples > dsp->nframes) {
        for (i=0; i<dsp->nchannels; i++) {
            float * restrict delayline = &state->delayline[state->delay_samples * i];
            k = state->offset;
            for (n=0; n<dsp->nframes; n++) {
                dsp->outbufs[i][n] = gain_and_clip_sample(delayline[k], state->gain, state->clip_threshold);
                delayline[k] = dsp->inbufs[i][n];
                if (++k == state->delay_samples) k=0;
            }
            DEBUG3("i=%p:%.2f, o=%p:%.2f, n=%d\n", dsp->inbufs[i], dsp->inbufs[i][n], dsp->outbufs[i], dsp->outbufs[i][n], n);
        }
        state->offset = k;
    }
    else {
        for (i=0; i<dsp->nchannels; i++) {
            float * restrict delayline = &state->delayline[state->delay_samples * i];
            for (n=0, k=dsp->nframes - state->delay_samples; n<state->delay_samples; n++, k++) {
                dsp->outbufs[i][n] = gain_and_clip_sample(delayline[n], state->gain, state->clip_threshold);
                delayline[n] = dsp->inbufs[i][k];
            }
            DEBUG3("i=%p:%.2f, o=%p:%.2f, n=%d\t", dsp->inbufs[i], dsp->inbufs[i][n], dsp->outbufs[i], dsp->outbufs[i][n], n);
            for (k=0; n<dsp->nframes; n++, k++) {
                dsp->outbufs[i][n] = gain_and_clip_sample(dsp->inbufs[i][k], state->gain, state->clip_threshold);
            }
            DEBUG3("k=%d\n", k);
        }
    }
}

void gain_init(struct qdsp_t * dsp)
{
    struct qdsp_gain_state_t * state = (struct qdsp_gain_state_t *)dsp->state;
    state->delay_samples = state->delay_seconds * dsp->fs;
    debugprint(2, "%s: delay_samples=%d\n", __func__, state->delay_samples);
    state->delayline = (float*)realloc(state->delayline, state->delay_samples * dsp->nchannels * sizeof(float));
    memset(state->delayline, 0, state->delay_samples * dsp->nchannels * sizeof(float));
}

void destroy_gain(struct qdsp_t * dsp)
{
    struct qdsp_gain_state_t * state = (struct qdsp_gain_state_t *)dsp->state;
    free(state->delayline);
    free(dsp->state);
}

int create_gain(struct qdsp_t * dsp, char ** subopts)
{
    enum {
        GAIN_OPT = 0,
        GAIN_LIN_OPT,
        DELAY_OPT,
        THRESHOLD_OPT,
    };
    char *const token[] = {
        [GAIN_OPT]   = "g",
        [GAIN_LIN_OPT]   = "gl",
        [DELAY_OPT]  = "d",
        [THRESHOLD_OPT]  = "t",
        NULL
    };
    char *value;
    int errfnd = 0;
    struct qdsp_gain_state_t * state = malloc(sizeof(struct qdsp_gain_state_t));
    dsp->state = (void*)state;

    // default values
    state->delayline = NULL;
    state->delay_seconds = 0;
    state->gain = 1.0f;
    state->offset = 0;
    state->clip_threshold = 1.0f;

    debugprint(1, "%s subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        switch (getsubopt(subopts, token, &value)) {
        case GAIN_OPT:
            if (value == NULL) {
                debugprint(0, "%s: Missing value for suboption '%s'\n", __func__, token[GAIN_OPT]);
                errfnd = 1;
                continue;
            }
            state->gain = powf(10.0f, atof(value) / 20.0f);
            debugprint(1, "%s: gain=%f\n", __func__, atof(value));
            break;
        case GAIN_LIN_OPT:
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
        case THRESHOLD_OPT:
            if (value == NULL) {
                debugprint(0, "%s: Missing value for suboption '%s'\n", __func__, token[THRESHOLD_OPT]);
                errfnd = 1;
                continue;
            }
            state->clip_threshold = powf(10.0f, atof(value) / 20.0f);
            debugprint(1, "%s: gain=%f\n", __func__, atof(value));
            break;
        default:
            debugprint(0, "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
    dsp->process = gain_process;
    dsp->init = gain_init;
    dsp->destroy = destroy_gain;

    return errfnd;
}

void help_gain(void)
{
    debugprint(0, "  Gain options\n");
    debugprint(0, "    Name: gain\n");
    debugprint(0, "        g = gain value (dB)\n");
    debugprint(0, "        gl = gain value (linear)\n");
    debugprint(0, "        d = delay value (seconds)\n");
    debugprint(0, "        t = clip threshold (dBFS)\n");
    debugprint(0, "    Example: -p gain,g=-3,d=0.002,t=-6\n");
    debugprint(0, "    Note: Gain is applied before clipping\n");
}
