#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <ltc.h>

#define BUFFER_SIZE 1024
#define LTC_QUEUE_LENGTH 16

int main(int argc, char** argv) {
    snd_pcm_t* pcm_handle;
    snd_pcm_hw_params_t* params;
    unsigned int sample_rate = 48000;
    int dir;
    snd_pcm_uframes_t frames = BUFFER_SIZE;
    int channels = 1;

    LTCDecoder* decoder;
    LTCFrameExt frame;
    ltcsnd_sample_t buffer[BUFFER_SIZE];
    int16_t raw_buffer[BUFFER_SIZE];

    int rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "🔥 Error opening PCM device: %s\n", snd_strerror(rc));
        return 1;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &sample_rate, &dir);
    snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &frames, &dir);
    snd_pcm_hw_params(pcm_handle, params);
    snd_pcm_hw_params_free(params);
    snd_pcm_prepare(pcm_handle);

    decoder = ltc_decoder_create(sample_rate, LTC_QUEUE_LENGTH);
    if (!decoder) {
        fprintf(stderr, "💥 Failed to create LTC decoder!\n");
        return 1;
    }

    printf("🎙️  Listening for LTC on ALSA device 'default' at %d Hz...\n", sample_rate);

    while (1) {
        rc = snd_pcm_readi(pcm_handle, raw_buffer, BUFFER_SIZE);
        if (rc == -EPIPE) {
            fprintf(stderr, "🔄 Overrun occurred!\n");
            snd_pcm_prepare(pcm_handle);
            continue;
        }
        else if (rc < 0) {
            fprintf(stderr, "❌ Error reading from PCM device: %s\n", snd_strerror(rc));
            continue;
        }

        for (int i = 0; i < rc; i++) {
            buffer[i] = 128 + (raw_buffer[i] / 256); // scale to 8-bit unsigned
        }

        ltc_decoder_write(decoder, buffer, rc, 0);

        while (ltc_decoder_read(decoder, &frame)) {
            SMPTETimecode stime;
            ltc_frame_to_time(&stime, &frame.ltc, 0);
            printf("🕒 %02d:%02d:%02d%c%02d\n",
                stime.hours,
                stime.mins,
                stime.secs,
                frame.ltc.dfbit ? '.' : ':',
                stime.frame);
            fflush(stdout);
        }
    }

    ltc_decoder_free(decoder);
    snd_pcm_close(pcm_handle);
    return 0;
}
