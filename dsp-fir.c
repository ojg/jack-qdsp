#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dsp.h"

#if defined(_OPENMP)
#include <omp.h>
#endif

#if (defined(__AVX__))
#include <immintrin.h>
static inline float dotp(float * x, float * y, size_t len)
{
    __m256 x4, y4;
    __m256 sum4 = _mm256_setzero_ps();
    float sum;
    size_t n;
    size_t len1 = len - (len % 8);

    for (n = 0; n < len1; n+=8) {
        x4 = _mm256_loadu_ps(&x[n]);
        y4 = _mm256_loadu_ps(&y[n]);
#if (defined(__FMA__))
        sum4 = _mm256_fmadd_ps(x4, y4, sum4);
#else
        prod4 = _mm256_mul_ps(x4, y4);
        sum4 = _mm256_add_ps(prod4, sum4);
#endif
    }
    sum4 = _mm256_hadd_ps(sum4, sum4);
    sum4 = _mm256_hadd_ps(sum4, sum4);
    _mm_store_ss(&sum, _mm_add_ps(_mm256_extractf128_ps(sum4, 0), _mm256_extractf128_ps(sum4, 1)));

    for (; n < len; n++)
        sum += x[n] * y[n];

    return sum;
}
#elif (defined(__SSE3__))
#include <immintrin.h>
static inline float dotp(float * x, float * y, size_t len)
{
    __m128 x4, y4;
    __m128 sum4 = _mm_setzero_ps();
    __m128 prod4;
    float sum;
    size_t n;
    size_t len1 = len - (len % 4);

    for (n = 0; n < len1; n+=4) {
        x4 = _mm_loadu_ps(&x[n]);
        y4 = _mm_loadu_ps(&y[n]);
        prod4 = _mm_mul_ps(x4, y4);
        sum4 = _mm_add_ps(prod4, sum4);
    }
    sum4 = _mm_hadd_ps(sum4, sum4);
    sum4 = _mm_hadd_ps(sum4, sum4);
    _mm_store_ss(&sum, sum4);

    for (; n < len; n++)
        sum += x[n] * y[n];

    return sum;
}
#elif (1)
static inline float dotp(float * x, float * y, size_t len)
{
    float sum[4] = { 0.0f };
    size_t k,n;

    for (k = 0, n = 0; k < len / 4; n+=4, k++) {
         sum[0] += x[n] * y[n];
         sum[1] += x[n+1] * y[n+1];
         sum[2] += x[n+2] * y[n+2];
         sum[3] += x[n+3] * y[n+3];
    }

    for (; n < len; n++)
        sum[0] += x[n] * y[n];

    return sum[0] + sum[1] + sum[2] + sum[3];
}
#else
static inline float dotp(float * x, float * y, size_t len)
{
    float sum = 0;
    for (size_t n = 0; n < len; n++) {
        sum += x[n] * y[n];
    }
    return sum;
}
#endif

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
    size_t offset = state->offset;

#if defined(_OPENMP)
    #pragma omp parallel for firstprivate(offset) lastprivate(offset)
#endif
    for (size_t c = 0; c < (size_t)dsp->nchannels; c++) {
        offset = state->offset;
        float * delayline = &state->delayline[state->hlen * c];
        for (int s = 0; s < dsp->nframes; s++) {
            float * coeffs = &state->coeffs[state->hlen - 1 - offset];
            delayline[offset] = dsp->inbufs[c][s];
            dsp->outbufs[c][s] = dotp(coeffs, delayline, state->hlen);
            if (++offset == state->hlen)
                offset = 0;
        }
    }
    state->offset = offset;
}

void fir_init(struct qdsp_t * dsp)
{
    struct qdsp_fir_state_t * state = (struct qdsp_fir_state_t *)dsp->state;
    state->delayline = realloc(state->delayline, dsp->nchannels * state->hlen * sizeof(float));
    memset(state->delayline, 0, dsp->nchannels * state->hlen * sizeof(float));
    state->offset = 0;

#if defined(_OPENMP)
    if (dsp->nchannels > 1 && state->hlen * dsp->nframes > 10000)
        omp_set_num_threads(dsp->nchannels);
    else
        omp_set_num_threads(1);
#endif
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
    state->hlen = 256;
    state->coeffs = malloc(state->hlen * sizeof(float)); //initial size of coeffs
    while (!feof(fid)) {
        if (fscanf(fid, "%f\n", &state->coeffs[i]) != 1) {
            debugprint(0, "%s: Read error in file: %s\n", __func__, state->coeff_filename);
            fclose(fid);
            errfnd = 1;
            break;
        }
        if (++i == state->hlen) {
            state->hlen = i * 2;
            state->coeffs = realloc(state->coeffs, state->hlen * sizeof(float)); //realloc if size grows
        }
    }
    fclose(fid);
    state->hlen = i;
    debugprint(2, "%s: state->hlen=%d\n", __func__, state->hlen);
    debugprint(2, "%s: state->coeff[1]=%e\n", __func__, state->coeffs[1]);
    state->coeffs = realloc(state->coeffs, state->hlen * 2 * sizeof(float)); //realloc to final size * 2
    for (i = 0; i < state->hlen; i++)
        state->coeffs[state->hlen * 2 - i - 1] = state->coeffs[i]; //reverse coeffs for second half
    memcpy(state->coeffs, &state->coeffs[state->hlen], state->hlen * sizeof(float)); //duplicate reversed coeffs

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
