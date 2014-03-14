#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dsp.h"

typedef double iirfp;

enum iir_type {
    IIR_TYPE_DIRECT = 0,
};

char *const typestr[] = {
    [IIR_TYPE_DIRECT]   = "direct",
    NULL
};

struct coeffs_t {
    iirfp a1;
    iirfp a2;
    iirfp b0;
    iirfp b1;
    iirfp b2;
};

struct qdsp_iir_state_t {
    enum iir_type type;
    struct coeffs_t coeffs __attribute__ ((aligned (16)));
    iirfp s[2*NCHANNELS_MAX] __attribute__ ((aligned (16)));
};

typedef double v2df __attribute__ ((vector_size (16)));

/* iir */
void iir_process(void * arg)
{
    struct qdsp_t * dsp = (struct qdsp_t *)arg;
    struct qdsp_iir_state_t * state = (struct qdsp_iir_state_t *)dsp->state;
    unsigned int nchannels = dsp->nchannels;
    unsigned int nframes = dsp->nframes;
    int c,n;

#if 0
    switch (nchannels) {
    case 2:
    {
        v2df x,y,s1,s2,b0,b1,b2,a1,a2 __attribute__ ((aligned (16)));
        a1[0] = a1[1] = state->coeffs.a1;
        a2[0] = a2[1] = state->coeffs.a2;
        b0[0] = b0[1] = state->coeffs.b0;
        b1[0] = b1[1] = state->coeffs.b1;
        b2[0] = b2[1] = state->coeffs.b2;
        s1[0] = state->s[0];
        s2[0] = state->s[1];
        s1[1] = state->s[2];
        s2[1] = state->s[3];
        for (n<0; n<nframes; n++) {
            x[0] = (iirfp)dsp->inbufs[0][n];
            x[1] = (iirfp)dsp->inbufs[1][n];
            y  = s1 + b0 * x;
            s1 = s2 + b1 * x - a1 * y;
            s2 =      b2 * x - a2 * y;
            dsp->outbufs[0][n] = (float)y[0];
            dsp->outbufs[1][n] = (float)y[1];
        }
        state->s[0] = s1[0];
        state->s[1] = s2[0];
        state->s[2] = s1[1];
        state->s[3] = s2[1];
        break;
    }
    default:
#endif
    {
        float *inbuf;
        float *outbuf;
        iirfp x,y,s1,s2;
        iirfp a1 = state->coeffs.a1;
        iirfp a2 = state->coeffs.a2;
        iirfp b0 = state->coeffs.b0;
        iirfp b1 = state->coeffs.b1;
        iirfp b2 = state->coeffs.b2;
        for (c=0; c<nchannels; c++) {
            inbuf = dsp->inbufs[c];
            outbuf = dsp->outbufs[c];
            s1 = state->s[c*2];
            s2 = state->s[c*2+1];
            for (n=0; n<nframes; n++) {
                x = (iirfp)inbuf[n];
                y  = s1 + b0 * x;
                s1 = s2 + b1 * x - a1 * y;
                s2 =      b2 * x - a2 * y;
                outbuf[n] = (float)y;
            }
            state->s[c*2] = s1;
            state->s[c*2+1] = s2;
        }
    }
//    }
}

int create_iir(struct qdsp_t * dsp, char ** subopts)
{
    enum {
        DIRECT_OPT = 0,
        A1_OPT,
        A2_OPT,
        B0_OPT,
        B1_OPT,
        B2_OPT
    };
    char *const token[] = {
        [DIRECT_OPT]   = "direct",
        [A1_OPT]   = "a1",
        [A2_OPT]   = "a2",
        [B0_OPT]   = "b0",
        [B1_OPT]   = "b1",
        [B2_OPT]   = "b2",
        NULL
    };
    char *value;
    char *name = NULL;
    int errfnd = 0;
    struct qdsp_iir_state_t * state = malloc(sizeof(struct qdsp_iir_state_t));
    dsp->state = (void*)state;
    dsp->process = iir_process;

    fprintf(stderr,"%s subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        fprintf(stderr,"%s checking subopts: %s\n", __func__, *subopts);
        switch (getsubopt(subopts, token, &value)) {
        case DIRECT_OPT:
            state->type = IIR_TYPE_DIRECT;
            fprintf(stderr,"%s iir type is direct\n", __func__);
            break;
        case A1_OPT:
            if (value == NULL) {fprintf(stderr, "Missing value for suboption '%s'\n", token[A1_OPT]); errfnd = 1; continue;}
            state->coeffs.a1 = strtod(value, NULL);
            break;
        case A2_OPT:
            if (value == NULL) {fprintf(stderr, "Missing value for suboption '%s'\n", token[A2_OPT]); errfnd = 1; continue;}
            state->coeffs.a2 = strtod(value, NULL);
            break;
        case B0_OPT:
            if (value == NULL) {fprintf(stderr, "Missing value for suboption '%s'\n", token[B0_OPT]); errfnd = 1; continue;}
            state->coeffs.b0 = strtod(value, NULL);
            break;
        case B1_OPT:
            if (value == NULL) {fprintf(stderr, "Missing value for suboption '%s'\n", token[B1_OPT]); errfnd = 1; continue;}
            state->coeffs.b1 = strtod(value, NULL);
            break;
        case B2_OPT:
            if (value == NULL) {fprintf(stderr, "Missing value for suboption '%s'\n", token[B2_OPT]); errfnd = 1; continue;}
            state->coeffs.b2 = strtod(value, NULL);
            break;
        default:
            fprintf(stderr, "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
/*    state->s1 = calloc(2 * nchannels, sizeof(float));
    state->s2 = state->s1 + nchannels;*/

    return errfnd;
}

void help_iir(void)
{
	fprintf(stderr,"  IIR options\n");
	fprintf(stderr,"    Name: iir\n");
	fprintf(stderr,"    type: direct\n");
	fprintf(stderr,"        a1=,a2=,b0=,b1=,b2=coefficient\n");
	fprintf(stderr,"    Example: -p iir,direct,a1=1,a2=0,b0=1,b1=2,b2=1\n");
}
