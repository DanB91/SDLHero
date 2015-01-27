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


static void outputSound(GameSoundOutput* sb, uint32_t tone) {

    real32_t period = SOUND_FREQ / tone;



    for (uint32_t i = 0; i < sb->numSamples; i++) {

        sb->t += 2 * pi32 / period;
        real32_t sineVal = sinf(sb->t);

#if 0 
        int16_t sample = sineVal * sb->volume;
#else
        int16_t sample = 0;
#endif
        sb->samples[i].leftChannel = sample;  //left channel 
        sb->samples[i].rightChannel = sample; //right channel

    }
    
}

#ifndef NDEBUG 
extern "C"
#endif
void gameUpdateAndRender(GameMemory* memory, OffScreenBuffer *buf, GameSoundOutput* sb, const InputContext* inputContext, real32_t secsSinceLastFrame) {
    GameState* state = (GameState*)memory->permanentStorage;

    if(!state->isInited) {
        state->tone = 512;
        state->isInited = true;
    }

    for(uint32_t i = 0; i < ARRAY_SIZE(inputContext->controllers); i++) {
        const ControllerInput* ci = &inputContext->controllers[i]; 

        if(ci->isAnalog) {
            state->tone = 512 + (int)(256.0f*ci->avgY);
            state->blueOffset += ci->avgX * 4 * secsSinceLastFrame;
            state->greenOffset += ci->avgY * 4 * secsSinceLastFrame ;
        }
        else {
            real32_t velocity = 500 * secsSinceLastFrame; 
            if(ci->directionLeft.isEndedDown) {
                state->blueOffset -= velocity;
            }
            else if(ci->directionRight.isEndedDown) {
                state->blueOffset += velocity;
            }

            if(ci->directionUp.isEndedDown) {
                state->greenOffset -= velocity;
            }
            else if(ci->directionDown.isEndedDown) {
                state->greenOffset += velocity;
            }
        }


    }


    outputSound(sb, state->tone);
    renderWeirdGradient(buf, state->blueOffset, state->greenOffset);
}
