#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include "dsp.h"

extern int debuglevel;

enum {
    GAIN_OPT = 0,
    GATE_OPT,
    IIR_OPT,
    CLIP_OPT,
    END_OPT
};

char *const token[] = {
    [GAIN_OPT]   = "gain",
    [GATE_OPT]   = "gate",
    [IIR_OPT]    = "iir",
    [CLIP_OPT]   = "clip",
    NULL
};

extern int create_gate(struct qdsp_t * dsp, char ** subopts);
extern int create_gain(struct qdsp_t * dsp, char ** subopts);
extern int create_iir(struct qdsp_t * dsp, char ** subopts);
extern int create_clip(struct qdsp_t * dsp, char ** subopts);

extern void help_gain(void);
extern void help_gate(void);
extern void help_iir(void);
extern void help_clip(void);

struct dspfuncs_t dspfuncs[] = {
        [GAIN_OPT].helpfunc     = help_gain,
        [GAIN_OPT].createfunc   = create_gain,

        [GATE_OPT].helpfunc     = help_gate,
        [GATE_OPT].createfunc   = create_gate,

        [IIR_OPT].helpfunc      = help_iir,
        [IIR_OPT].createfunc    = create_iir,

        [CLIP_OPT].helpfunc     = help_clip,
        [CLIP_OPT].createfunc   = create_clip,

        [END_OPT].helpfunc      = NULL,
        [END_OPT].createfunc    = NULL,
};


void create_dsp(struct qdsp_t * dsp, char * subopts)
{
    char *value;
    char *name = NULL;
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
