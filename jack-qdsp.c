
#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <jack/jack.h>
#include "dsp.h"

jack_port_t *input_port[NCHANNELS_MAX];
jack_port_t *output_port[NCHANNELS_MAX];
jack_client_t *client;
float *tempbuf[NCHANNELS_MAX];
float *zerobuf;

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 */
int process (jack_nframes_t nframes, void *arg)
{
    struct qdsp_t * dsp = (struct qdsp_t *)arg;
    struct qdsp_t * dsphead = dsp;

    while (dsp)
    {
        if (dsp->sequencecount==0) {
            fprintf(stderr,"%s: processing %p, next=%p, nframes=%d\n", __func__, dsp, dsp->next, nframes);
        }

        if (dsp==dsphead) {
            for (int i=0; i<dsp->nchannels; i++)
                dsp->inbufs[i] = jack_port_get_buffer (input_port[i], nframes);
        }
        else {
            for (int i=0; i<dsp->nchannels; i++)
                dsp->inbufs[i] = tempbuf[i];
        }

        if (!dsp->next) {
            for (int i=0; i<dsp->nchannels; i++)
                dsp->outbufs[i] = jack_port_get_buffer (output_port[i], nframes);
        }
        else {
            for (int i=0; i<dsp->nchannels; i++)
                dsp->outbufs[i] = tempbuf[i];
        }

        dsp->nframes = nframes;
        dsp->zerobuf = zerobuf;
        dsp->sequencecount++;
        dsp->process((void*)dsp);
        dsp = dsp->next;
    }

    return 0;
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown (void *arg)
{
    exit(EXIT_FAILURE);
}

void (*dsphelpfunc[])(void) = {
        help_gain,
        help_gate,
        help_iir,
        help_clip,
        NULL,
};

void print_help()
{
    int i=0;
    fprintf(stderr,"jack-qdsp -c channels [general-options] -p dsp-name <dsp-options> [-p ...]\n\n");
    fprintf(stderr,"General options\n");
    fprintf(stderr," -c channels\n");
    fprintf(stderr," -s server name\n");
    fprintf(stderr," -n client name\n");
    fprintf(stderr," -i input ports\n");
    fprintf(stderr," -o output ports\n");
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
        jack_client_close (client);
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
        GATE_OPT,
        IIR_OPT,
        CLIP_OPT
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
            create_gate(dsp, &subopts);
            break;
        case GAIN_OPT:
            create_gain(dsp, &subopts);
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

    if (errfnd) exit(1);
}


int main (int argc, char *argv[])
{
    const char *client_name = "jack-qdsp";
    char *server_name = NULL;
    char *input_ports = NULL;
    char *output_ports = NULL;
    jack_options_t options = JackNullOption;
    jack_status_t status;
    struct qdsp_t *dsphead = NULL;
    struct qdsp_t *dsp;
    unsigned int fs;
    unsigned int channels;
    int i,c;

    if (signal(SIGINT, sig_handler) == SIG_ERR)
        fprintf(stderr,"\ncan't catch SIGINT\n");

    if (argc == 1) {
        print_help();
        exit(1);
    }

    /* Get command line options */
    while ((c = getopt (argc, argv, "c:n:s:i:o:p:h?")) != -1) {
        switch (c) {
        case 'c':
            channels = atoi(optarg);
            if (channels < 1 || channels > NCHANNELS_MAX) endprogram("Invalid number of channels specified\n");
            break;
        case 'n':
            client_name = optarg;
            break;
        case 's':
            server_name = optarg;
            break;
        case 'i':
            input_ports = optarg;
            break;
        case 'o':
            output_ports = optarg;
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
        printf ("Non-option argument %s\n", argv[i]);

    if (channels == 0) endprogram("Must specify -c\n");

    /* open a client connection to the JACK server */
    fprintf(stderr,"Connecting to jack server: %s\n", server_name ? server_name : "default");
    client = jack_client_open (client_name, options, &status, server_name);
    if (client == NULL) {
        fprintf(stderr, "jack_client_open() failed, "
             "status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            fprintf(stderr, "Unable to connect to JACK server\n");
        }
        exit (1);
    }
    if (status & JackServerStarted) {
        fprintf(stderr, "JACK server started\n");
    }
    if (status & JackNameNotUnique) {
        client_name = jack_get_client_name(client);
        fprintf(stderr, "unique name `%s' assigned\n", client_name);
    }

    /* tell the JACK server to call `process()' whenever
       there is work to be done.
    */

    jack_set_process_callback (client, process, dsphead);

    /* tell the JACK server to call `jack_shutdown()' if
       it ever shuts down, either entirely, or if it
       just decides to stop calling us.
    */

    jack_on_shutdown (client, jack_shutdown, 0);

    /* get the current sample rate. */
    fs = jack_get_sample_rate (client);
    fprintf(stderr, "Samplerate: %d\n", fs);
    fprintf(stderr, "Channels: %d\n", channels);

    dsp = dsphead;
    while (dsp) {
        dsp->fs = fs;
        dsp->nchannels = channels;
        dsp->init(dsp);
        dsp = dsp->next;
    }

    /* create ports */
    for (i=0; i<channels; i++) {
        char name[20];
        sprintf(name, "in_%d", i+1);
        input_port[i] = jack_port_register (client, name,
                                     JACK_DEFAULT_AUDIO_TYPE,
                                     JackPortIsInput, 0);

        sprintf(name, "out_%d", i+1);
        output_port[i] = jack_port_register (client, name,
                                      JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsOutput, 0);
    }

    /* allocate tempbuf as one large buffer */
    tempbuf[0] = (float*)malloc(channels*NFRAMES_MAX*sizeof(float));
    if (!tempbuf[0]) endprogram("Could not allocate memory for temporary buffer.\n");
    for (i=1; i<channels; i++)
        tempbuf[i] = tempbuf[0] + i*NFRAMES_MAX;

    /* allocate a common zerobuf */
    zerobuf = (float*)calloc(NFRAMES_MAX, sizeof(float));

    /* Tell the JACK server that we are ready to roll.  Our
     * process() callback will start running now. */

    if (jack_activate (client)) {
        fprintf (stderr, "cannot activate client");
        exit (1);
    }

    /* Connect the ports.  You can't do this before the client is
     * activated, because we can't make connections to clients
     * that aren't running.  Note the confusing (but necessary)
     * orientation of the driver backend ports: playback ports are
     * "input" to the backend, and capture ports are "output" from
     * it.
     */
    if (input_ports) {
        char * token = strtok(input_ports,",");
        i=0;
        while (token) {
            fprintf(stderr,"Connecting to input %s\n", token);
            if (jack_connect (client, token, jack_port_name (input_port[i++]))) {
                fprintf (stderr, "cannot connect input ports\n");
            }
            token = strtok(NULL, ",");
        }
    }

    if (output_ports) {
        char * token = strtok(output_ports,",");
        i=0;
        while (token) {
            fprintf(stderr,"Connecting to output %s\n", token);
            if (jack_connect (client, jack_port_name (output_port[i++]), token)) {
                fprintf (stderr, "cannot connect output ports\n");
            }
            token = strtok(NULL, ",");
        }
    }

    /* keep running until stopped by the user */

    sleep (-1);

    /* this is never reached but if the program
       had some other way to exit besides being killed,
       they would be important to call.
    */

    jack_client_close (client);
    exit (0);
}
