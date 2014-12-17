#include <math.h>
#include "handmade.hpp"

static void renderWeirdGradient(OffScreenBuffer *buf, int blueOffset, int greenOffset) {
    Pixel *pixels = buf->pixels;

    for (uint32_t y = 0; y < buf->height; y++) {

        Pixel *currPixel = pixels; 

        for (uint32_t x = 0; x < buf->width; x++) {
            currPixel->b = x + blueOffset;
            currPixel->g = y + greenOffset;

            currPixel++;
        }

        pixels += buf->width;
    }
}


static void outputSound(SoundBuffer* sb) {

    real32_t period = SOUND_FREQ / sb->tone;



    for (uint32_t i = 0; i < sb->numSamples; i++) {

        sb->t += 2 * pi32 / period;
        real32_t sineVal = sinf(sb->t);

        int16_t sample = sineVal * sb->volume;

        sb->samples[i].leftChannel = sample;  //left channel 
        sb->samples[i].rightChannel = sample; //right channel

    }

}

void gameUpdateAndRender(OffScreenBuffer *buf, int blueOffset, int greenOffset, SoundBuffer* sb) {
    outputSound(sb);
    renderWeirdGradient(buf, blueOffset, greenOffset);
}
