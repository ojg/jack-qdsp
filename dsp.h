#ifndef DSP_H
#define DSP_H

#include <stdio.h>
#include <stdbool.h>

#define NCHANNELS_MAX 8

struct qdsp_t {
    struct qdsp_t *next;
    const float * restrict inbufs[NCHANNELS_MAX];
    float * restrict outbufs[NCHANNELS_MAX];
    const float * restrict zerobuf;
    unsigned int fs;
    int nchannels;
    int nframes;
    unsigned int sequencecount;
    void *state;
    void (*process)(struct qdsp_t *);
    void (*init)(struct qdsp_t *);
    void (*destroy)(struct qdsp_t *);
};

struct dspfuncs_t {
        void (*helpfunc)(void);
        int (*createfunc)(struct qdsp_t *, char **);
};

void create_dsp(struct qdsp_t * dsp, char * subopts);
void init_dsp(struct qdsp_t * dsphead);
void destroy_dsp(struct qdsp_t * dsphead);
void endprogram(char * str);
void debugprint(int level, const char * fmt, ...);

#ifndef DEBUGLEVEL
#define DEBUGLEVEL 0
#endif
#if DEBUGLEVEL >= 0
#define DEBUG0(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG0(...)
#endif
#if DEBUGLEVEL >= 1
#define DEBUG1(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG1(...)
#endif
#if DEBUGLEVEL >= 2
#define DEBUG2(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG2(...)
#endif
#if DEBUGLEVEL >= 3
#define DEBUG3(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG3(...)
#endif


#endif
