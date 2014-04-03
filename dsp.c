#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "dsp.h"

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

int (*createfunc[])(struct qdsp_t * dsp, char ** subopts) = {
        [GAIN_OPT]   = create_gain,
        [GATE_OPT]   = create_gate,
        [IIR_OPT]    = create_iir,
        [CLIP_OPT]   = create_clip,
        NULL
};


void endprogram(char * str)
{
    fprintf(stderr,"%s",str);
    exit(EXIT_FAILURE);
}

void create_dsp(struct qdsp_t * dsp, char * subopts)
{
    char *value;
    char *name = NULL;
    int errfnd = 0;
    int curtoken;

    fprintf(stderr,"create_dsp subopts: %s\n", subopts);

    while (*subopts != '\0' && !errfnd) {
        curtoken = getsubopt(&subopts, token, &value);
        if (curtoken >= 0 && curtoken < END_OPT) {
            errfnd = createfunc[curtoken](dsp, &subopts);
        }
        else {
            fprintf(stderr, "create_dsp: No match found for token: /%s/\n", value);
            errfnd = 1;
            break;
        }
    }

    dsp->sequencecount = 0;
    dsp->next = NULL;

    if (errfnd) endprogram("Could not create dsp\n");
}
