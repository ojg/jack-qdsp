
#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sndfile.h>
#include <stdbool.h>
#include <time.h>
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
//            fprintf(stderr,"%s: processing %p, next=%p, nframes=%d, seq=%d\n", __func__, dsp, dsp->next, nframes, dsp->sequencecount);
        }

        dsp->nframes = nframes;
        dsp->sequencecount++;
        dsp->process((void*)dsp);
        dsp = dsp->next;
    }

    return 0;
}

void (*dsphelpfunc[])(void) = {
		help_gain,
		help_gate,
		help_iir,
		NULL,
};

void print_help()
{
	int i=0;
    fprintf(stderr,"file-qdsp -i inputfile -o outputfile [general-options] -p dsp-name <dsp-options> [-p ...]\n\n");
    fprintf(stderr,"General options\n");
    fprintf(stderr," -i input filename, all types supported by libsndfile, - for stdin\n");
    fprintf(stderr," -o output filename, all types supported by libsndfile, - for stdout\n");
    fprintf(stderr," -n framesize in samples, default=1024, must be a power-of-two\n");
    fprintf(stderr," -r raw file options:\n");
    fprintf(stderr,"    c=channels\n    r=samplerate in Hz\n    f=format 1=S8,2=S16,3=S24,4=S32,5=U8,6=F32\n");
    fprintf(stderr,"\nDSP options\n");

    while (dsphelpfunc[i]) {
        dsphelpfunc[i++]();
        fprintf(stderr, "\n");
    }

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

struct timespec timespecsub(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

struct timespec timespecadd(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec+start.tv_nsec)>=1000000000) {
        temp.tv_sec = end.tv_sec+start.tv_sec+1;
        temp.tv_nsec = end.tv_nsec+start.tv_nsec-1000000000;
    } else {
        temp.tv_sec = end.tv_sec+start.tv_sec;
        temp.tv_nsec = end.tv_nsec+start.tv_nsec;
    }
    return temp;
}

void create_dsp(struct qdsp_t * dsp, char * subopts)
{
    enum {
        GAIN_OPT = 0,
        GATE_OPT,
        IIR_OPT,
    };
    char *const token[] = {
        [GAIN_OPT]   = "gain",
        [GATE_OPT]   = "gate",
        [IIR_OPT]    = "iir",
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

    if (errfnd) endprogram("Could not create dsp\n");
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

bool get_rawfileopts(SF_INFO * input_sfinfo, char * subopts)
{
    enum {
        CH_OPT = 0,
        FS_OPT,
        FORMAT_OPT,
    };
    char *const token[] = {
        [CH_OPT]   = "c",
        [FS_OPT]   = "r",
        [FORMAT_OPT] = "f",
        NULL
    };
    char *value;
    char *name = NULL;
    int errfnd = 0;

    memset(input_sfinfo, 0, sizeof(*input_sfinfo));

    while (*subopts != '\0' && !errfnd) {
        switch (getsubopt(&subopts, token, &value)) {
        case FS_OPT:
            if (value == NULL) {
                fprintf(stderr, "Missing value for suboption '%s'\n", token[FS_OPT]);
                errfnd = 1;
                continue;
            }
            input_sfinfo->samplerate = atoi(value);
            fprintf(stderr,"raw_fs=%d\n",atoi(value));
            break;
        case CH_OPT:
            if (value == NULL) {
                fprintf(stderr, "Missing value for suboption '%s'\n", token[CH_OPT]);
                errfnd = 1;
                continue;
            }
            input_sfinfo->channels = atoi(value);
            fprintf(stderr,"raw_ch=%d\n",atoi(value));
            break;
        case FORMAT_OPT:
            if (value == NULL) {
                fprintf(stderr, "Missing value for suboption '%s'\n", token[FORMAT_OPT]);
                errfnd = 1;
                continue;
            }
            input_sfinfo->format = atoi(value);
            fprintf(stderr,"raw_format=%d\n",atoi(value));
            break;
        default:
            fprintf(stderr, "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
    }
    return !errfnd;
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
    unsigned int nframes=0, totframes=0;
    bool israwinput = false;
    struct timespec t,t2,ttot,res;
    int i,c;

    if (signal(SIGINT, sig_handler) == SIG_ERR)
        fprintf(stderr,"\ncan't catch SIGINT\n");

    /* Get command line options */
    while ((c = getopt (argc, argv, "r:n:i:o:p:h?")) != -1) {
        switch (c) {
        case 'r':
            // for raw file support
            israwinput = get_rawfileopts(&input_sfinfo, optarg);
            break;
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
    if (israwinput)
        input_sfinfo.format |= SF_FORMAT_RAW;
    else
        memset(&input_sfinfo, 0, sizeof(input_sfinfo));
    if (!(input_file = sf_open(input_filename, SFM_READ, &input_sfinfo))) {
        fprintf(stderr,"Could not open file %s for reading.\n", input_filename);
        endprogram("");
    }

    memcpy(&output_sfinfo, &input_sfinfo, sizeof(input_sfinfo));
    if (!(output_file = sf_open(output_filename, SFM_WRITE, &output_sfinfo))) {
        fprintf(stderr,"Could not open file %s for writing.\n", output_filename);
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
    ttot.tv_sec=0;
    ttot.tv_nsec=0;
    while (nframes == sf_readf_float(input_file, readbuf, nframes)) {
        totframes += nframes;
        deinterleave(tempbuf[0], readbuf, channels, nframes);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
        process(nframes, dsphead);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t2); t = timespecsub(t,t2); ttot = timespecadd(t,ttot);
        interleave(writebuf, tempbuf[0], channels, nframes);
        if (nframes != sf_writef_float(output_file, writebuf, nframes))
            break;
    }
    clock_getres(CLOCK_THREAD_CPUTIME_ID, &res);
    /* wrap up */
    fprintf(stderr, "Done! Processed %d samples in %lld.%.9ld sec, res=%ld nsec\n", totframes, (long long)ttot.tv_sec, ttot.tv_nsec, res.tv_nsec);

    if (sf_close(input_file)!=0) fprintf(stderr, "Failed closing %s: %s\n", input_filename, sf_strerror(input_file));
    if (sf_close(output_file)!=0) fprintf(stderr, "Failed closing %s: %s\n", output_filename, sf_strerror(output_file));

    return 0;
}

