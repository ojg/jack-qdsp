#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "dsp.h"

typedef double iirfp;

enum iir_type {
    DIRECT_OPT = 0,
    LP2_OPT,
    HP2_OPT,
    LS2_OPT,
    HS2_OPT,
    LP1_OPT,
    HP1_OPT,
    LS1_OPT,
    HS1_OPT,
    PEQ_OPT,
    LWT_OPT,
    AP2_OPT,
    AP1_OPT,
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
    double f0,f1,q0,q1,gain;
    struct coeffs_t coeffs __attribute__ ((aligned (16)));
    iirfp s[2*NCHANNELS_MAX] __attribute__ ((aligned (16)));
};

typedef double v2df __attribute__ ((vector_size (16)));


int calc_coeffs(struct qdsp_iir_state_t * state, double fs)
{
    /* Based on RBJ Cookbook Formulae */
    double A = sqrt(pow(10.0,state->gain/20.0));
    double w0 = 2.0*M_PI*state->f0/fs;
    double sinw0 = sin(w0);
    double cosw0 = cos(w0);
    double ln2 = log(2.0);
    double alpha = sinw0/(2.0*state->q0);
    double b0,b1,b2,a0,a1,a2;

    switch (state->type) {
    case LP2_OPT:
        b0 =  (1 - cosw0)/2;
        b1 =   1 - cosw0;
        b2 =  (1 - cosw0)/2;
        a0 =   1 + alpha;
        a1 =  -2*cosw0;
        a2 =   1 - alpha;
        break;
    case HP2_OPT:
        b0 =  (1 + cosw0)/2;
        b1 = -(1 + cosw0);
        b2 =  (1 + cosw0)/2;
        a0 =   1 + alpha;
        a1 =  -2*cosw0;
        a2 =   1 - alpha;
        break;
    case LS2_OPT:
        b0 =    A*( (A+1) - (A-1)*cosw0 + 2*sqrt(A)*alpha );
        b1 =  2*A*( (A-1) - (A+1)*cosw0                   );
        b2 =    A*( (A+1) - (A-1)*cosw0 - 2*sqrt(A)*alpha );
        a0 =        (A+1) + (A-1)*cosw0 + 2*sqrt(A)*alpha;
        a1 =   -2*( (A-1) + (A+1)*cosw0                   );
        a2 =        (A+1) + (A-1)*cosw0 - 2*sqrt(A)*alpha;
        break;
    case HS2_OPT:
        b0 =    A*( (A+1) + (A-1)*cosw0 + 2*sqrt(A)*alpha );
        b1 = -2*A*( (A-1) + (A+1)*cosw0                   );
        b2 =    A*( (A+1) + (A-1)*cosw0 - 2*sqrt(A)*alpha );
        a0 =        (A+1) - (A-1)*cosw0 + 2*sqrt(A)*alpha;
        a1 =    2*( (A-1) - (A+1)*cosw0                   );
        a2 =        (A+1) - (A-1)*cosw0 - 2*sqrt(A)*alpha;
        break;
    case PEQ_OPT:
        b0 =   1 + alpha*A;
        b1 =  -2*cosw0;
        b2 =   1 - alpha*A;
        a0 =   1 + alpha/A;
        a1 =  -2*cosw0;
        a2 =   1 - alpha/A;
        break;
    case AP2_OPT:
        b0 =   1 - alpha;
        b1 =  -2*cosw0;
        b2 =   1 + alpha;
        a0 =   1 + alpha;
        a1 =  -2*cosw0;
        a2 =   1 - alpha;
        break;
    default:
        return 1;
    }

    state->coeffs.b0 = b0 / a0;
    state->coeffs.b1 = b1 / a0;
    state->coeffs.b2 = b2 / a0;
    state->coeffs.a1 = a1 / a0;
    state->coeffs.a2 = a2 / a0;

    return 0;
}

void init_iir(struct qdsp_t * dsp)
{
    struct qdsp_iir_state_t * state = (struct qdsp_iir_state_t *)dsp->state;
    if (state->type != DIRECT_OPT) {
        calc_coeffs(state, dsp->fs);
    }
}

void iir_process(struct qdsp_t * dsp)
{
    struct qdsp_iir_state_t * state = (struct qdsp_iir_state_t *)dsp->state;
    unsigned int nchannels = dsp->nchannels;
    unsigned int nframes = dsp->nframes;
    int c,n;

    switch (nchannels) {
    case 2:
#if 0
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
            x[0] = (double)dsp->inbufs[0][n];
            x[1] = (double)dsp->inbufs[1][n];
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
#endif
    default:
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
    }
}

int create_iir(struct qdsp_t * dsp, char ** subopts)
{
    enum {
        F0_OPT = AP1_OPT+1,
        Q0_OPT,
        GAIN_OPT,
        F1_OPT,
        Q1_OPT,
        A1_OPT,
        A2_OPT,
        B0_OPT,
        B1_OPT,
        B2_OPT,
    };
#define FIRST_VALUETOKEN F0_OPT

    char *const token[] = {
        [DIRECT_OPT] = "direct",
        [LP2_OPT]  = "lp2",
        [HP2_OPT]  = "hp2",
        [LS2_OPT]  = "ls2",
        [HS2_OPT]  = "hs2",
        [LP1_OPT]  = "lp1",
        [HP1_OPT]  = "hp1",
        [LS1_OPT]  = "ls1",
        [HS1_OPT]  = "hs1",
        [PEQ_OPT]  = "peq",
        [LWT_OPT]  = "lwt",
        [AP2_OPT]  = "ap2",
        [AP1_OPT]  = "ap1",

        [F0_OPT]   = "f",
        [Q0_OPT]   = "q",
        [GAIN_OPT] = "g",
        [F1_OPT]   = "f1",
        [Q1_OPT]   = "q1",
        [A1_OPT]   = "a1",
        [A2_OPT]   = "a2",
        [B0_OPT]   = "b0",
        [B1_OPT]   = "b1",
        [B2_OPT]   = "b2",
        NULL
    };

    long long validparammasks[] = {
            (1<<DIRECT_OPT) | (1<<A1_OPT) | (1<<A2_OPT) | (1<<B0_OPT) | (1<<B1_OPT) | (1<<B2_OPT),
            (1<<LP2_OPT) | (1<<F0_OPT) | (1<<Q0_OPT),
            (1<<HP2_OPT) | (1<<F0_OPT) | (1<<Q0_OPT),
            (1<<LP1_OPT) | (1<<F0_OPT),
            (1<<HP1_OPT) | (1<<F0_OPT),
            (1<<LS2_OPT) | (1<<F0_OPT) | (1<<Q0_OPT) | (1<<GAIN_OPT),
            (1<<HS2_OPT) | (1<<F0_OPT) | (1<<Q0_OPT) | (1<<GAIN_OPT),
            (1<<LS1_OPT) | (1<<F0_OPT),
            (1<<HS1_OPT) | (1<<F0_OPT),
            (1<<PEQ_OPT) | (1<<F0_OPT) | (1<<Q0_OPT) | (1<<GAIN_OPT),
            (1<<LWT_OPT) | (1<<F0_OPT) | (1<<F1_OPT) | (1<<Q0_OPT) | (1<<Q1_OPT),
            (1<<AP2_OPT) | (1<<F0_OPT) | (1<<Q0_OPT),
    };

    char *value;
    char *name = NULL;
    int errfnd = 0, i;
    struct qdsp_iir_state_t * state = malloc(sizeof(struct qdsp_iir_state_t));
    long long curparammask = 0;
    int curtoken;
    bool validparams = false;

    dsp->state = (void*)state;
    dsp->process = iir_process;

    fprintf(stderr,"%s subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        fprintf(stderr,"%s checking subopts: %s\n", __func__, *subopts);
        curtoken = getsubopt(subopts, token, &value);

        if (curtoken < 0) {
            fprintf(stderr, "%s: No match found for token '%s'\n", __func__, value);
            continue;
        }

        if (curtoken >= FIRST_VALUETOKEN && value == NULL) {
            fprintf(stderr, "Missing value for suboption '%s'\n", token[curtoken]);
            errfnd = 1;
            continue;
        }

        if (curtoken < FIRST_VALUETOKEN && value != NULL) {
            fprintf(stderr, "Ignoring value for suboption '%s'\n", token[curtoken]);
        }

        switch (curtoken) {
        case DIRECT_OPT:
        case LP2_OPT:
        case HP2_OPT:
        case LS2_OPT:
        case HS2_OPT:
        case LP1_OPT:
        case HP1_OPT:
        case LS1_OPT:
        case HS1_OPT:
        case PEQ_OPT:
        case LWT_OPT:
        case AP2_OPT:
        case AP1_OPT:
            state->type = curtoken;
            fprintf(stderr,"%s iir type is %s\n", __func__, token[curtoken]);
            break;
        case F0_OPT:
            state->f0 = strtod(value, NULL);
            break;
        case Q0_OPT:
            state->q0 = strtod(value, NULL);
            break;
        case F1_OPT:
            state->f1 = strtod(value, NULL);
            break;
        case Q1_OPT:
            state->q1 = strtod(value, NULL);
            break;
        case GAIN_OPT:
            state->gain = strtod(value, NULL);
            break;
        case A1_OPT:
            state->coeffs.a1 = strtod(value, NULL);
            break;
        case A2_OPT:
            state->coeffs.a2 = strtod(value, NULL);
            break;
        case B0_OPT:
            state->coeffs.b0 = strtod(value, NULL);
            break;
        case B1_OPT:
            state->coeffs.b1 = strtod(value, NULL);
            break;
        case B2_OPT:
            state->coeffs.b2 = strtod(value, NULL);
            break;
        default:
            fprintf(stderr, "%s: No match found for token '%s'\n", __func__, value);
            continue;
        }

        curparammask |= 1 << curtoken;

    }

    for (i=0; i<sizeof(validparammasks)/sizeof(validparammasks[0]); i++) {
        if (validparammasks[i] == curparammask)
            validparams = true;
    }
    if (!validparams) {
        fprintf(stderr, "%s: Missing parameters for iir type '%s'\n", __func__, token[state->type]);
        errfnd = 1;
    }

    dsp->init = init_iir;

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
