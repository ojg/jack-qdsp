#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include "dsp.h"

/*****************************************************************/
/* Add a line to each of these blocks when adding a new dsp type */
enum {
    GAIN_OPT = 0,
    GATE_OPT,
    IIR_OPT,
    FIR_OPT,
    END_OPT
};

char *const token[] = {
    [GAIN_OPT]   = "gain",
    [GATE_OPT]   = "gate",
    [IIR_OPT]    = "iir",
    [FIR_OPT]    = "fir",
    NULL
};

extern int create_gate(struct qdsp_t * dsp, char ** subopts);
extern int create_gain(struct qdsp_t * dsp, char ** subopts);
extern int create_iir(struct qdsp_t * dsp, char ** subopts);
extern int create_fir(struct qdsp_t * dsp, char ** subopts);

extern void help_gain(void);
extern void help_gate(void);
extern void help_iir(void);
extern void help_fir(void);

struct dspfuncs_t dspfuncs[] = {
        [GAIN_OPT] = {.helpfunc = help_gain, .createfunc = create_gain },
        [GATE_OPT] = {.helpfunc = help_gate, .createfunc = create_gate },
        [IIR_OPT] = {.helpfunc = help_iir, .createfunc = create_iir },
        [FIR_OPT] = {.helpfunc = help_fir, .createfunc = create_fir },
        [END_OPT] = {.helpfunc = NULL, .createfunc = NULL },
};
/******************************************************************/

extern int debuglevel;

float * pingbuf;

void create_dsp(struct qdsp_t * dsp, char * subopts)
{
    char *value;
    int errfnd = 0;
    int curtoken;

    debugprint(1, "create_dsp subopts: %s\n", subopts);

    while (*subopts != '\0' && !errfnd) {
        curtoken = getsubopt(&subopts, token, &value);
        if (curtoken >= 0 && curtoken < END_OPT) {
            errfnd = dspfuncs[curtoken].createfunc(dsp, &subopts);
        }
        else {
            debugprint(0, "create_dsp: No match found for token: /%s/\n", value);
            errfnd = 1;
            break;
        }
    }

    dsp->sequencecount = 0;
    dsp->next = NULL;

    if (errfnd) endprogram("Could not create dsp\n");
}


void init_dsp(struct qdsp_t * dsphead)
{
    float *zerobuf;
    bool ping = false;
    struct qdsp_t * dsp;
    int i;
    float * pongbuf;
    int nframes = dsphead->nframes;
    int nchannels = dsphead->nchannels;

    /* allocate tempbuf as one large buffer */
    pingbuf = realloc(pingbuf, (2 * nchannels + 1) * nframes * sizeof(float));
    if (!pingbuf) endprogram("Could not allocate memory for temporary buffer.\n");
    /* Todo: Does realloc return NULL on fail? */

    /* allocate a common zerobuf */
    pongbuf = pingbuf + nchannels*nframes;
    zerobuf = pingbuf + 2*nchannels*nframes;
    for (i=0; i<nframes; i++)
        zerobuf[i] = FLT_EPSILON;

    /* setup all static dsp list info */
    dsp = dsphead;
    while (dsp) {
        dsp->fs = dsphead->fs;
        dsp->nchannels = dsphead->nchannels;
        dsp->nframes = dsphead->nframes;
        dsp->zerobuf = zerobuf;

        for (i=0; i<dsp->nchannels; i++) {
            dsp->inbufs[i] = ping ? pingbuf + i*nframes : pongbuf + i*nframes;
            dsp->outbufs[i] = ping ? pongbuf + i*nframes : pingbuf + i*nframes;
        }

        dsp->init(dsp);

        dsp = dsp->next;
        ping = !ping;
    }
}

void destroy_dsp(struct qdsp_t * dsphead)
{
    struct qdsp_t * dsp;
    while (dsphead) {
        dsp = dsphead;
        dsphead = dsp->next;
        dsp->destroy(dsp);
        free(dsp);
    }
    free(pingbuf);
}


void endprogram(char * str)
{
    debugprint(0, "%s",str);
    exit(EXIT_FAILURE);
}

void debugprint(int level, const char * fmt, ...)
{
    if (level <= debuglevel) {
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt ,ap);
        va_end(ap);
    }
}
