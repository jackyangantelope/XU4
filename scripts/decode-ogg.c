#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int stb_vorbis_decode_filename(const char *filename, int *channels, int *rate, short **output);

static int word(FILE *stream, uint32_t value)
{
    unsigned char bytes[4] = {value, value >> 8, value >> 16, value >> 24};
    return fwrite(bytes, 1, sizeof bytes, stream) == sizeof bytes;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    int channels = 0, rate = 0;
    short *samples = NULL;
    int frames = stb_vorbis_decode_filename(argv[1], &channels, &rate, &samples);
    if (frames <= 0 || frames > 4000000 || channels < 1 || channels > 2 ||
        rate < 8000 || rate > 96000) {
        free(samples);
        return 3;
    }
    uint32_t count = (uint32_t)frames * channels;
    int ok = word(stdout, 0x4d503458) && word(stdout, 1) && word(stdout, rate) &&
        word(stdout, channels) && word(stdout, frames) && word(stdout, count * 2);
    for (uint32_t i = 0; ok && i < count; ++i) {
        uint16_t sample = (uint16_t)samples[i];
        ok = putchar(sample & 255) != EOF && putchar(sample >> 8) != EOF;
    }
    free(samples);
    return ok && fflush(stdout) == 0 ? 0 : 4;
}
