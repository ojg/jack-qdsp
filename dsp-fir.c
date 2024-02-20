#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dsp.h"

struct qdsp_fir_state_t {
    char * coeff_filename;
    float * delayline;
    float * coeffs;
    unsigned hlen;
    unsigned offset;
};

void fir_process(struct qdsp_t * dsp)
{
    struct qdsp_fir_state_t * state = (struct qdsp_fir_state_t *)dsp->state;

    for (int s = 0; s < dsp->nframes; s++) {
        float * coeffs = &state->coeffs[state->offset];
        for (int c = 0; c < dsp->nchannels; c++) {
            float * delayline  = &state->delayline[state->hlen * c];
            delayline[state->offset] = dsp->inbufs[c][s];
            float sum = 0;
            for (unsigned n = 0; n < state->hlen; n++) {
                sum += coeffs[n] * delayline[n];
            }
            dsp->outbufs[c][s] = sum;
        }
        if (++state->offset == state->hlen)
            state->offset = 0;
    }

}

void fir_init(struct qdsp_t * dsp)
{
    struct qdsp_fir_state_t * state = (struct qdsp_fir_state_t *)dsp->state;
    state->delayline = realloc(state->delayline, dsp->nchannels * state->hlen * sizeof(float));
    memset(state->delayline, 0, dsp->nchannels * state->hlen * sizeof(float));
    state->offset = 0;
}

void destroy_fir(struct qdsp_t * dsp)
{
    struct qdsp_fir_state_t * state = (struct qdsp_fir_state_t *)dsp->state;
    free(state->coeffs);
    free(state->delayline);
    free(state);
}

int create_fir(struct qdsp_t * dsp, char ** subopts)
{
    enum {
        COEFF_OPT = 0,
    };
    char *const token[] = {
        [COEFF_OPT]   = "h",
        NULL
    };
    char *value;
    int errfnd = 0;
    struct qdsp_fir_state_t * state = malloc(sizeof(struct qdsp_fir_state_t));
    dsp->state = (void*)state;

    // default values
    state->coeff_filename = NULL;
    state->delayline = NULL;
    state->coeffs = NULL;
    state->offset = 0;
    state->hlen = 0;

    debugprint(1, "%s subopts: %s\n", __func__, *subopts);
    while (**subopts != '\0' && !errfnd) {
        switch (getsubopt(subopts, token, &value)) {
        case COEFF_OPT:
            if (value == NULL) {
                debugprint(0, "%s: Missing value for suboption '%s'\n", __func__, token[COEFF_OPT]);
                errfnd = 1;
                continue;
            }
            state->coeff_filename = value;
            debugprint(1, "%s: coeff_filename=%s\n", __func__, value);
            break;
        default:
            debugprint(0, "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
    dsp->process = fir_process;
    dsp->init = fir_init;
    dsp->destroy = destroy_fir;

    if (errfnd || !state->coeff_filename)
        return 1;

    FILE * fid = fopen(state->coeff_filename, "r");
    if (!fid) {
        debugprint(0, "%s: Unable to open file: %s\n", __func__, state->coeff_filename);
        return 1;
    }

    size_t i = 0;
    state->coeffs = malloc(256 * sizeof(float)); //initial size of coeffs
    while (!feof(fid)) {
        if (fscanf(fid, "%f\n", &state->coeffs[i]) != 1) {
            debugprint(0, "%s: Read error in file: %s\n", __func__, state->coeff_filename);
            fclose(fid);
            errfnd = 1;
            break;
        }
        if (++i == 256)
            state->coeffs = realloc(state->coeffs, i * 2 * sizeof(float)); //realloc if size grows
    }
    fclose(fid);
    state->hlen = i;
    debugprint(2, "%s: state->hlen=%d\n", __func__, state->hlen);
    debugprint(2, "%s: state->coeff[1]=%e\n", __func__, state->coeffs[1]);
    state->coeffs = realloc(state->coeffs, state->hlen * 2 * sizeof(float)); //realloc to final size * 2
    memcpy(&state->coeffs[state->hlen], state->coeffs, state->hlen * sizeof(float)); //duplicate coeffs
    state->delayline = malloc(state->hlen * sizeof(float));

    return errfnd;
}

void help_fir(void)
{
    debugprint(0, "  FIR filter options\n");
    debugprint(0, "    Name: fir\n");
    debugprint(0, "        h = coefficient filename\n");
    debugprint(0, "    Example: -p fir,h=coeffs.txt\n");
    debugprint(0, "    Note: Coefficient file should contain one coefficient per line\n");
}