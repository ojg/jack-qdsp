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

#if defined(__SSE3__)
#include <immintrin.h>
#endif

#if (defined(__AVX__))
static inline void dotp8_2(float * sumy, float * sumz, float * y, float * z, float * yc, float * zc, size_t len)
{
    __m256 yc8, zc8, y8, z8;
    __m256 sum8y = _mm256_setzero_ps();
    __m256 sum8z = _mm256_setzero_ps();
    size_t n;

    for (n = 0; n < len; n+=8) {
        yc8 = _mm256_loadu_ps(&yc[n]);
        zc8 = _mm256_loadu_ps(&zc[n]);
        y8 = _mm256_loadu_ps(&y[n]);
        z8 = _mm256_loadu_ps(&z[n]);
#if (defined(__FMA__))
        sum8y = _mm256_fmadd_ps(yc8, y8, sum8y);
        sum8z = _mm256_fmadd_ps(zc8, z8, sum8z);
#else
        sum8y = _mm256_add_ps(sum8y, _mm256_mul_ps(yc8, y8));
        sum8z = _mm256_add_ps(sum8z, _mm256_mul_ps(zc8, z8));
#endif
    }
    sum8y = _mm256_hadd_ps(sum8y, sum8y);
    sum8y = _mm256_hadd_ps(sum8y, sum8y);
    _mm_store_ss(sumy, _mm_add_ps(_mm256_extractf128_ps(sum8y, 0), _mm256_extractf128_ps(sum8y, 1)));

    sum8z = _mm256_hadd_ps(sum8z, sum8z);
    sum8z = _mm256_hadd_ps(sum8z, sum8z);
    _mm_store_ss(sumz, _mm_add_ps(_mm256_extractf128_ps(sum8z, 0), _mm256_extractf128_ps(sum8z, 1)));
}

#elif (defined(__SSE3__))
static inline void dotp4_2(float * sumy, float * sumz, float * y, float * z, float * yc, float * zc, size_t len)
{
    __m128 yc4, zc4, y4, z4;
    __m128 sum4y = _mm_setzero_ps();
    __m128 sum4z = _mm_setzero_ps();
    for (size_t n = 0; n < len; n+=4) {
        yc4 = _mm_loadu_ps(&yc[n]);
        zc4 = _mm_loadu_ps(&zc[n]);
        y4 = _mm_loadu_ps(&y[n]);
        z4 = _mm_loadu_ps(&z[n]);
        sum4y = _mm_add_ps(_mm_mul_ps(yc4, y4), sum4y);
        sum4z = _mm_add_ps(_mm_mul_ps(zc4, z4), sum4z);
    }
    sum4y = _mm_hadd_ps(sum4y, sum4y);
    sum4y = _mm_hadd_ps(sum4y, sum4y);
    _mm_store_ss(sumy, sum4y);
    sum4z = _mm_hadd_ps(sum4z, sum4z);
    sum4z = _mm_hadd_ps(sum4z, sum4z);
    _mm_store_ss(sumz, sum4z);
}
#endif

static inline float dotp4(const float * x, const float * y, size_t len)
{
    float sum = 0.0f;

#if (defined(__ARM_NEON__))
    __asm__ volatile (
                  "vmov.f32 q8, #0.0          \n\t" // zero out q8 register
                  "1:                         \n\t"
                  "subs %3, %3, #4            \n\t" // we load 4 floats into q0, and q2 register
                  "vld1.f32 {d0,d1}, [%1, :32]!    \n\t" // loads q0, update pointer *x
                  "vld1.f32 {d4,d5}, [%2, :32]!    \n\t" // loads q2, update pointer *y
                  "vmla.f32 q8, q0, q2        \n\t" // store four partial sums in q8
                  "bgt 1b                     \n\t" // loops to label 1 until len==0
                  "vpadd.f32 d0, d16, d17     \n\t" // pairwise add 4 partial sums in q8, store in d0
                  "vadd.f32 %0, s0, s1        \n\t" // add 2 partial sums in d0, store result in return variable
                  : "=w"(sum)                 // output
                  : "r"(x), "r"(y), "r"(len)    // input
                  : "q0", "q2", "q8");        // clobber list

#else
    for (size_t n = 0; n < len; n++) {
        sum += x[n] * y[n];
    }
#endif
    return sum;
}

static inline void dotp_2(float * sumy, float * sumz, float * y, float * z, float * yc, float * zc, size_t len)
{
    float suma = 0, sumb = 0;
#if (defined(__AVX__))
    dotp8_2(&suma, &sumb, y, z, yc, zc, len);
#elif (defined(__SSE3__))
    dotp4_2(&suma, &sumb, y, z, yc, zc, len);
#elif (defined(__ARM_NEON__))
    suma = dotp4(y, yc, len);
    sumb = dotp4(z, zc, len);
#else
    for (size_t n = 0; n < len; n++) {
        suma += y[n] * yc[n];
        sumb += z[n] * zc[n];
    }
#endif

    *sumy = suma;
    *sumz = sumb;
}

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

    if (dsp->nchannels == 2) {
        float * delayline0 = &state->delayline[0];
        float * delayline1 = &state->delayline[state->hlen];
        for (int s = 0; s < dsp->nframes; s++) {
            float * coeffs = &state->coeffs[state->hlen - 1 - offset];
            delayline0[offset] = dsp->inbufs[0][s];
            delayline1[offset] = dsp->inbufs[1][s];
            dotp_2(&dsp->outbufs[0][s], &dsp->outbufs[1][s], delayline0, delayline1, coeffs, coeffs, state->hlen);
            if (++offset == state->hlen)
                offset = 0;
        }
        state->offset = offset;
        return;
    }

#if defined(_OPENMP)
    #pragma omp parallel for firstprivate(offset) lastprivate(offset)
#endif
    for (size_t c = 0; c < (size_t)dsp->nchannels; c++) {
        offset = state->offset;
        float * delayline = &state->delayline[state->hlen * c];
        for (int s = 0; s < dsp->nframes; s++) {
            float * coeffs = &state->coeffs[state->hlen - 1 - offset];
            float suma, sumb = 0;
            delayline[offset] = dsp->inbufs[c][s];
            dotp_2(&suma, &sumb, delayline, delayline+state->hlen/2, coeffs, coeffs+state->hlen/2, state->hlen/2);
            dsp->outbufs[c][s] = suma + sumb;
            if (++offset == state->hlen)
                offset = 0;
        }
    }
    state->offset = offset;
}

void fir_init(struct qdsp_t * dsp)
{
    struct qdsp_fir_state_t * state = (struct qdsp_fir_state_t *)dsp->state;
    free(state->delayline);
    state->delayline = valloc(dsp->nchannels * state->hlen * sizeof(float));
    memset(state->delayline, 0, dsp->nchannels * state->hlen * sizeof(float));
    state->offset = 0;

#if defined(_OPENMP)
    if (dsp->nchannels > 1 && state->hlen * dsp->nframes > 10000) {
        omp_set_num_threads(dsp->nchannels);
        debugprint(0, "fir_init: Use OpenMP\n");
    }
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
    float * tempcoeffs = malloc(state->hlen * sizeof(float)); //initial size of coeffs
    while (!feof(fid)) {
        if (fscanf(fid, "%f\n", &tempcoeffs[i]) != 1) {
            debugprint(0, "%s: Read error in file: %s\n", __func__, state->coeff_filename);
            fclose(fid);
            errfnd = 1;
            break;
        }
        if (++i == state->hlen) {
            state->hlen = i * 2;
            tempcoeffs = realloc(tempcoeffs, state->hlen * sizeof(float)); //realloc if size grows
        }
    }
    fclose(fid);
    state->hlen = i;
#if (defined(__AVX__))
    size_t exphlen = (i & ~15) + 16;
#else
    size_t exphlen = (i & ~7) + 8;
#endif
    debugprint(2, "%s: state->hlen=%d\n", __func__, state->hlen);
    debugprint(2, "%s: state->coeff[1]=%e\n", __func__, tempcoeffs[1]);
    state->coeffs = valloc(exphlen * 2 * sizeof(float)); //realloc to final size * 2
    memset(state->coeffs, 0, exphlen * 2 * sizeof(float));
    for (i = 0; i < state->hlen; i++)
        state->coeffs[exphlen * 2 - i - 1] = tempcoeffs[i]; //reverse coeffs for second half
    memcpy(state->coeffs, &state->coeffs[exphlen], exphlen * sizeof(float)); //duplicate reversed coeffs
    free(tempcoeffs);
    state->hlen = exphlen;

#if (defined(__AVX__))
    debugprint(0, "fir: Use AVX\n");
#elif (defined(__SSE3__))
    debugprint(0, "fir: Use SSE3\n");
#elif (defined(__ARM_NEON__))
    debugprint(0, "fir: Use ARM NEON\n");
#endif

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
