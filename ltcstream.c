#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <ltc.h>

#define SAMPLE_RATE 48000
#define BUFFER_SIZE 256

int main() {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    int dir;
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_uframes_t frames = BUFFER_SIZE;
    short buffer[BUFFER_SIZE];

    // Open PCM device for capture (recording)
    if (snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_CAPTURE, 0) < 0) {
        fprintf(stderr, "Unable to open PCM device\n");
        return 1;
    }

    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm_handle, params);
    snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
    snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, &dir);
    snd_pcm_hw_params_set_period_size_near(pcm_handle, params, &frames, &dir);
    snd_pcm_hw_params(pcm_handle, params);
    snd_pcm_hw_params_free(params);
    snd_pcm_prepare(pcm_handle);

    // ✅ Corrected decoder type
    LTCDecoder *decoder = ltc_decoder_create(SAMPLE_RATE, 80);
    LTCFrameExt frame;
    SMPTETimecode stime;

    printf("🔊 Listening for LTC...\n");

    while (1) {
        int err = snd_pcm_readi(pcm_handle, buffer, BUFFER_SIZE);
        if (err == -EPIPE) {
            snd_pcm_prepare(pcm_handle);
            continue;
        } else if (err < 0) {
            fprintf(stderr, "Read error: %s\n", snd_strerror(err));
            continue;
        }

        for (int i = 0; i < BUFFER_SIZE; i++) {
            ltc_decoder_write_short(decoder, buffer[i]);
        }

        while (ltc_decoder_read(decoder, &frame)) {
            ltc_frame_to_timecode(&stime, &frame.ltc, 25);  // ⚠️ Adjust FPS here if needed

            int hh = stime.hours;
            int mm = stime.mins;
            int ss = stime.secs;
            int ff = stime.frame;

            if (hh >= 0 && hh < 24 &&
                mm >= 0 && mm < 60 &&
                ss >= 0 && ss < 60 &&
                ff >= 0 && ff < 30) {
                printf("🕒 %02d:%02d:%02d:%02d\n", hh, mm, ss, ff);
            } else {
                fprintf(stderr, "⚠️ Rejected invalid LTC frame: %02d:%02d:%02d:%02d\n", hh, mm, ss, ff);
            }
        }
    }

    ltc_decoder_free(decoder);
    snd_pcm_close(pcm_handle);

    return 0;
}
