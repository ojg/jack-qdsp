#ifndef DSP_H
#define DSP_H

#define NFRAMES_MAX 8192
#define NCHANNELS_MAX 8

struct qdsp_t {
    struct qdsp_t *next;
    float *inbufs[NCHANNELS_MAX];
    float *outbufs[NCHANNELS_MAX];
    float *zerobuf;
    unsigned int fs;
    unsigned int nchannels;
    unsigned int nframes;
    unsigned int sequencecount;
    void *state;
    void (*process)(void *);
};

int create_gate(struct qdsp_t * dsp, char ** subopts);
int create_gain(struct qdsp_t * dsp, char ** subopts);
int create_iir(struct qdsp_t * dsp, char ** subopts);

#endif