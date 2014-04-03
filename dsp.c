#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "dsp.h"

void endprogram(char * str)
{
    fprintf(stderr,"%s",str);
    exit(EXIT_FAILURE);
}

void create_dsp(struct qdsp_t * dsp, char * subopts)
{
    enum {
        GAIN_OPT = 0,
        GATE_OPT,
        IIR_OPT,
        CLIP_OPT,
    };
    char *const token[] = {
        [GAIN_OPT]   = "gain",
        [GATE_OPT]   = "gate",
        [IIR_OPT]    = "iir",
        [CLIP_OPT]   = "clip",
        NULL
    };
    char *value;
    char *name = NULL;
    int errfnd = 0;
    fprintf(stderr,"create_dsp subopts: %s\n", subopts);

    while (*subopts != '\0' && !errfnd) {
        switch (getsubopt(&subopts, token, &value)) {
        case GATE_OPT:
            errfnd = create_gate(dsp, &subopts);
            break;
        case GAIN_OPT:
            errfnd = create_gain(dsp, &subopts);
            break;
        case IIR_OPT:
            errfnd = create_iir(dsp, &subopts);
            break;
        case CLIP_OPT:
            errfnd = create_clip(dsp, &subopts);
            break;
        default:
            fprintf(stderr, "create_dsp: No match found "
                    "for token: /%s/\n", value);
            errfnd = 1;
            break;
        }
    }

    dsp->sequencecount = 0;
    dsp->next = NULL;

    if (errfnd) endprogram("Could not create dsp\n");
}
