
#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sndfile.h>
#include "dsp.h"

unsigned int channels;
float **tempbuf;
float *zerobuf;


int process (unsigned int nframes, void *arg)
{
    struct qdsp_t * dsp = (struct qdsp_t *)arg;
    struct qdsp_t * dsphead = dsp;

    while (dsp)
    {
        if (dsp->sequencecount == 0) {
            fprintf(stderr,"%s: processing %p, next=%p, nframes=%d, seq=%d\n", __func__, dsp, dsp->next, nframes, dsp->sequencecount);
        }

        dsp->nframes = nframes;
        dsp->sequencecount++;
        dsp->process((void*)dsp);
        dsp = dsp->next;
    }

    return 0;
}


void print_help()
{
    fprintf(stderr,"jack-dsp helptext goes here\n");
    exit(EXIT_SUCCESS);
}

void sig_handler(int signo)
{
    fprintf(stderr,"sig_handler\n");
    if (signo == SIGINT) {
        fprintf(stderr,"received SIGINT\n");
        exit(0);
    }
}

void endprogram(char * str)
{
    fprintf(stderr,"%s",str);
    exit(EXIT_FAILURE);
}

void create_dsp(struct qdsp_t * dsp, char * subopts)
{
    enum {
        GAIN_OPT = 0,
        GATE_OPT
    };
    char *const token[] = {
        [GAIN_OPT]   = "gain",
        [GATE_OPT]   = "gate",
        NULL
    };
    char *value;
    char *name = NULL;
    int errfnd = 0;
    fprintf(stderr,"create_dsp subopts: %s\n", subopts);

    while (*subopts != '\0' && !errfnd) {
        switch (getsubopt(&subopts, token, &value)) {
        case GATE_OPT:
            create_gate(dsp, &subopts);
            break;
        case GAIN_OPT:
            create_gain(dsp, &subopts);
            break;
        default:
            fprintf(stderr, "create_dsp: No match found "
                    "for token: /%s/\n", value);
            errfnd = 1;
            break;
        }
    }
    //fprintf(stderr,"%s: channels=%d\n", __func__, channels);
    //dsp->nchannels = channels;
    dsp->sequencecount = 0;
    dsp->next = NULL;

    if (errfnd) exit(1);
}

void deinterleave(float * restrict dst, float * restrict src, int nch, int nfr)
{
    int c,n;
    for (c=0; c<nch; c++) {
        for (n=0; n<nfr; n++) {
            dst[c*nfr+n] = src[n*nch+c];
        }
    }
}

void interleave(float * restrict dst, float * restrict src, int nch, int nfr)
{
    int c,n;
    for (c=0; c<nch; c++) {
        for (n=0; n<nfr; n++) {
            dst[n*nch+c] = src[c*nfr+n];
        }
    }
}

int main (int argc, char *argv[])
{
    SNDFILE *input_file = NULL;
    SNDFILE *output_file = NULL;
    SF_INFO input_sfinfo;
    SF_INFO output_sfinfo;
    char *input_filename = NULL;
    char *output_filename = NULL;
    struct qdsp_t *dsphead = NULL;
    struct qdsp_t *dsp;
    float *readbuf, *writebuf;
    unsigned int nframes=0;
    int i,c;

    if (signal(SIGINT, sig_handler) == SIG_ERR)
        fprintf(stderr,"\ncan't catch SIGINT\n");

    /* Get command line options */
    while ((c = getopt (argc, argv, "n:i:o:p:h?")) != -1) {
        switch (c) {
        /*case 'c':
            channels = atoi(optarg);
            if (channels < 1 || channels > NCHANNELS_MAX) endprogram("Invalid number of channels specified\n");
            break;
        case 'r':
            fs = atoi(optarg);
            break;*/
        case 'n':
            nframes = atoi(optarg);
            break;
        case 'i':
            input_filename = optarg;
            break;
        case 'o':
            output_filename = optarg;
            break;
        case 'p':
            if (!dsphead) {
                dsphead = (struct qdsp_t*)malloc(sizeof(struct qdsp_t));
                if (!dsphead) endprogram("Could not allocate memory for dsp.\n");
                dsp = dsphead;
            }
            else {
                dsp->next = (struct qdsp_t*)malloc(sizeof(struct qdsp_t));
                if (!dsp->next) endprogram("Could not allocate memory for dsp.\n");
                dsp = dsp->next;
            }
            fprintf(stderr,"%s: dsp=%p\n",__func__, dsp);
            create_dsp(dsp, optarg);
            fprintf(stderr,"%s: dsp->next=%p\n",__func__, dsp);
            break;
        case 'h':
        case '?':
            print_help();
            break;
        }
    }

    for (i = optind; i < argc; i++)
        fprintf(stderr, "Non-option argument %s\n", argv[i]);

    if ((nframes == 0) || (nframes & (nframes - 1))) endprogram("Framesize must be a power of two.\n");

    /* open files */
    memset(&input_sfinfo, 0, sizeof(input_sfinfo));
    if (!(input_file = sf_open(input_filename, SFM_READ, &input_sfinfo))) {
        fprintf(stderr,"Could not file %s for reading.\n", input_filename);
        endprogram("");
    }

    memcpy(&output_sfinfo, &input_sfinfo, sizeof(input_sfinfo));
    if (!(output_file = sf_open(output_filename, SFM_WRITE, &output_sfinfo))) {
        fprintf(stderr,"Could not file %s for reading.\n", output_filename);
        endprogram("");
    }

    /* get the current samplerate. */
    fprintf(stderr, "input file samplerate: %d\n", input_sfinfo.samplerate);
    fprintf(stderr, "input file channels: %d\n", input_sfinfo.channels);
    channels = input_sfinfo.channels;
    if (channels < 1 || channels > NCHANNELS_MAX) endprogram("Invalid number of channels specified\n");

    /* allocate tempbuf as one large buffer */
    tempbuf = (float**)malloc(channels*sizeof(float*));
    tempbuf[0] = (float*)malloc(channels*nframes*sizeof(float));
    if (!tempbuf[0]) endprogram("Could not allocate memory for temporary buffer.\n");
    for (i=1; i<channels; i++)
        tempbuf[i] = tempbuf[0] + i*nframes;

    /* allocate a common zerobuf */
    zerobuf = (float*)calloc(nframes, sizeof(float));

    readbuf = (float*)malloc(nframes*channels*sizeof(float));
    writebuf = (float*)malloc(nframes*channels*sizeof(float));

    /* setup all static dsp list info */
    dsp = dsphead;
    while (dsp) {
        dsp->fs = input_sfinfo.samplerate;
        dsp->nchannels = input_sfinfo.channels;
        dsp->zerobuf = zerobuf;

        for (int i=0; i<dsp->nchannels; i++)
            dsp->inbufs[i] = tempbuf[i];

        for (int i=0; i<dsp->nchannels; i++)
            dsp->outbufs[i] = tempbuf[i];

        dsp = dsp->next;
    }

    /* Run processing until EOF */
    while (nframes == sf_readf_float(input_file, readbuf, nframes)) {
        deinterleave(tempbuf[0], readbuf, channels, nframes);
        process(nframes, dsphead);
        interleave(writebuf, tempbuf[0], channels, nframes);
        if (nframes != sf_writef_float(output_file, writebuf, nframes))
            break;
    }

    /* wrap up */
    fprintf(stderr, "Done!\n");
    if (sf_close(input_file)!=0) fprintf(stderr, "Failed closing %s: %s\n", input_filename, sf_strerror(input_file));
    if (sf_close(output_file)!=0) fprintf(stderr, "Failed closing %s: %s\n", output_filename, sf_strerror(output_file));

    return 0;
}

