
#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sndfile.h>
#include <stdbool.h>
#include <time.h>
#include <fenv.h>
#include "dsp.h"

int debuglevel;
int get_debuglevel(void)
{
    return debuglevel;    
}

struct qdsp_t * process (unsigned int nframes, void *arg)
{
    struct qdsp_t * dsphead = (struct qdsp_t *)arg;
    struct qdsp_t * dsp = dsphead;
    struct qdsp_t * lastdsp = dsp;

    while (dsp)
    {
        if (dsp->sequencecount == 0) {
//            debugprint(0, "%s: processing %p, next=%p, nframes=%d, seq=%d\n", __func__, dsp, dsp->next, nframes, dsp->sequencecount);
        }

        dsp->nframes = nframes;
        dsp->sequencecount++;
        dsp->process((void*)dsp);
        lastdsp = dsp;
        dsp = dsp->next;
    }

    return lastdsp;
}

void print_help()
{
    int i=0;
    debugprint(0, "file-qdsp -i inputfile -o outputfile [general-options] -p dsp-name <dsp-options> [-p ...]\n\n");
    debugprint(0, "Version: %s\n", VERSION);
    debugprint(0, "General options\n");
    debugprint(0, " -i input filename, all types supported by libsndfile, - for stdin\n");
    debugprint(0, " -o output filename, all types supported by libsndfile, - for stdout\n");
    debugprint(0, " -n framesize in samples, default=1024, must be a power-of-two\n");
    debugprint(0, " -r raw file options:\n");
    debugprint(0, "    c=channels\n    r=samplerate in Hz\n    f=format 1=S8,2=S16,3=S24,4=S32,5=U8,6=F32\n");
    debugprint(0, "\nDSP options\n");

    struct dspfuncs_t * dspfuncs = get_dspfuncs();
    while (dspfuncs[i].helpfunc) {
        dspfuncs[i++].helpfunc();
        debugprint(0,  "\n");
    }

    exit(EXIT_SUCCESS);
}

void sig_handler(int signo)
{
    debugprint(0, "sig_handler\n");
    if (signo == SIGINT) {
        debugprint(0, "received SIGINT\n");
        exit(0);
    }
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
    int errfnd = 0;
    int curtoken;
    int curparammask = 0;

    while (*subopts != '\0' && !errfnd) {
        curtoken = getsubopt(&subopts, token, &value);
        switch (curtoken) {
        case FS_OPT:
            if (value == NULL) {
                debugprint(0,  "Missing value for suboption '%s'\n", token[FS_OPT]);
                errfnd = 1;
                continue;
            }
            input_sfinfo->samplerate = atoi(value);
            debugprint(1, "raw_fs=%d\n",atoi(value));
            break;
        case CH_OPT:
            if (value == NULL) {
                debugprint(0,  "Missing value for suboption '%s'\n", token[CH_OPT]);
                errfnd = 1;
                continue;
            }
            input_sfinfo->channels = atoi(value);
            debugprint(1, "raw_ch=%d\n",atoi(value));
            break;
        case FORMAT_OPT:
            if (value == NULL) {
                debugprint(0,  "Missing value for suboption '%s'\n", token[FORMAT_OPT]);
                errfnd = 1;
                continue;
            }
            input_sfinfo->format = atoi(value);
            debugprint(1, "raw_format=%d\n",atoi(value));
            break;
        default:
            debugprint(0,  "%s: No match found for token: /%s/\n", __func__, value);
            errfnd = 1;
            break;
        }
        curparammask |= 1 << curtoken;

    }

    if (curparammask != 7) {
        debugprint(0, "%s: Not enough suboptions for -r\n", __func__);
        errfnd = 1;
    }

    input_sfinfo->format |= SF_FORMAT_RAW;

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
    struct qdsp_t *dsp = NULL;
    float *readbuf, *writebuf;
    unsigned int nframes=1024, totframes=0, nframesread=0;
    struct timespec t,t2,ttot,res;
    int i,c,itmp;
    debuglevel = 0;
    unsigned int channels;

    if (signal(SIGINT, sig_handler) == SIG_ERR)
        debugprint(0, "\ncan't catch SIGINT\n");

    if (argc == 1) {
        print_help();
        exit(1);
    }

    memset(&input_sfinfo, 0, sizeof(input_sfinfo));

    /* Get command line options */
    while ((c = getopt (argc, argv, "r:n:i:o:p:v::h?")) != -1) {
        switch (c) {
        case 'r':
            // for raw file support
            if (!get_rawfileopts(&input_sfinfo, optarg))
                endprogram("Wrong options for -r\n");
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
                dsphead = malloc(sizeof(struct qdsp_t));
                if (!dsphead) endprogram("Could not allocate memory for dsp.\n");
                dsp = dsphead;
            }
            else {
                dsp->next = malloc(sizeof(struct qdsp_t));
                if (!dsp->next) endprogram("Could not allocate memory for dsp.\n");
                dsp = dsp->next;
            }
            debugprint(2, "%s: dsp=%p\n",__func__, dsp);
            create_dsp(dsp, optarg);
            debugprint(2, "%s: dsp->next=%p\n",__func__, dsp);
            break;
        case 'v':
            if (optarg) {
                itmp = atoi(optarg);
                debugprint(0, "%s: verbosity=%d\n",__func__, itmp);
                if (itmp<0)
                    debuglevel = 0;
                else
                    debuglevel = itmp;
            }
            else
                debuglevel = 0;
            break;
        case 'h':
        case '?':
            print_help();
            exit(0);
        }
    }

    if (optind != argc) {
        for (i = optind; i < argc; i++)
            debugprint(0,  "Non-option argument %s\n\n", argv[i]);
        print_help();
    }

    if ((nframes == 0) || (nframes & (nframes - 1))) endprogram("Framesize must be a power of two.\n");

    /* open files */
    if (!input_filename)
        endprogram("Must specify input file\n");

    if (!output_filename)
        endprogram("Must specify output file\n");

    if (!(input_file = sf_open(input_filename, SFM_READ, &input_sfinfo))) {
        debugprint(0, "Could not open file %s for reading.\n", input_filename);
        endprogram("");
    }

    memcpy(&output_sfinfo, &input_sfinfo, sizeof(input_sfinfo));
    if (!(output_file = sf_open(output_filename, SFM_WRITE, &output_sfinfo))) {
        debugprint(0, "Could not open file %s for writing.\n", output_filename);
        endprogram("");
    }

    /* get the current samplerate. */
    debugprint(0,  "input file samplerate: %d\n", input_sfinfo.samplerate);
    debugprint(0,  "input file channels: %d\n", input_sfinfo.channels);
    channels = input_sfinfo.channels;
    if (channels < 1 || channels > NCHANNELS_MAX) endprogram("Invalid number of channels specified\n");

    if (!dsphead) endprogram("No processing specified\n");
    dsphead->fs = input_sfinfo.samplerate;
    dsphead->nchannels = channels;
    dsphead->nframes = nframes;
    init_dsp(dsphead);

    readbuf = malloc(nframes*channels*sizeof(float));
    writebuf = malloc(nframes*channels*sizeof(float));

    /* Run processing until EOF */
    ttot.tv_sec=0;
    ttot.tv_nsec=0;
    while ((nframesread = sf_readf_float(input_file, readbuf, nframes))) {
        int raised;
        if (nframesread < nframes) {
            memset(readbuf + (nframesread * channels), 0, (nframes-nframesread) * channels * sizeof(float));
        }
        totframes += nframes;
        debugprint(3, "inbufs=%p\n", dsp->inbufs[0]);

        deinterleave((float*)dsphead->inbufs[0], readbuf, channels, nframes);

        feclearexcept(FE_ALL_EXCEPT);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);

        dsp = process(nframes, dsphead);

        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t2); t = timespecsub(t,t2); ttot = timespecadd(t,ttot);
        raised = fetestexcept(FE_INEXACT | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW | FE_INVALID);
        if (raised) debugprint(3, "FE exception raised: 0x%02X\n", raised);

        debugprint(3, "outbufs=%p\n", dsp->outbufs[0]);

        interleave(writebuf, dsp->outbufs[0], channels, nframes);

        if (nframesread != sf_writef_float(output_file, writebuf, nframesread))
            break;
    }
    clock_getres(CLOCK_THREAD_CPUTIME_ID, &res);
    /* wrap up */
    debugprint(0,  "Done! Processed %d samples in %lld.%.9ld sec, res=%ld nsec\n", totframes, (long long)ttot.tv_sec, ttot.tv_nsec, res.tv_nsec);

    if (sf_close(input_file)!=0) debugprint(0,  "Failed closing %s: %s\n", input_filename, sf_strerror(input_file));
    if (sf_close(output_file)!=0) debugprint(0,  "Failed closing %s: %s\n", output_filename, sf_strerror(output_file));

    free(writebuf);
    free(readbuf);
    destroy_dsp(dsphead);

    return 0;
}

