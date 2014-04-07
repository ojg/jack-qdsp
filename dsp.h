#ifndef DSP_H
#define DSP_H

#include <stdio.h>
#include <stdbool.h>

#define NFRAMES_MAX 8192
#define NCHANNELS_MAX 8

struct qdsp_t {
    struct qdsp_t *next;
    float * restrict inbufs[NCHANNELS_MAX];
    float * restrict outbufs[NCHANNELS_MAX];
    const float * restrict zerobuf;
    unsigned int fs;
    unsigned int nchannels;
    unsigned int nframes;
    unsigned int sequencecount;
    void *state;
    void (*process)(struct qdsp_t *);
    void (*init)(struct qdsp_t *);
};

struct dspfuncs_t {
        void (*helpfunc)(void);
        int (*createfunc)(struct qdsp_t * dsp, char ** subopts);
};

void create_dsp(struct qdsp_t * dsp, char * subopts);
void init_dsp(struct qdsp_t * dsphead, unsigned int fs, unsigned int nchannels, unsigned int nframes);
void endprogram(char * str);
void debugprint(int level, const char * fmt, ...);

#endif
