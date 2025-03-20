#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "application.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


/////////////////////////
// Object Initializations
/////////////////////////
ToneObj toneObj = initToneObj();



/////////////////////////
// ToneObj Methods
/////////////////////////
void toneGenerator(ToneObj* self, int unused) {
   self->active = !self->active;

   if (self->active && !self->mute && !self->stop) {
      DAC_WRITE(self->volume);
   }
   else {
      DAC_WRITE(0);
   }

   if (!self->stop)
      SEND(self->period, USEC(self->deadline), self, toneGenerator, 0);
}

void setVolume(ToneObj* self, int vol) {

   if (vol >= MAX_VOLUME) {
      if (vol != self->volume) {
         self->volume = MAX_VOLUME;
         print("Volume at max!\n", 0);
      }
   }
   else if (vol <= 0) {
      if (vol != self->volume) {
         print("Volume: 0\n", 0);
         self->volume = 0;
      }
   }
   else {
      // Only change volume if not muted and inside allowed volume range
      if (vol != self->volume) {
         self->volume = vol;
         print("Volume: %d\n", self->volume);
      }
   }
}

void muteVolume(ToneObj* self, int unused) {
   self->mute = !self->mute;

   if (self->mute)
   {
      print("Volume muted!\n", 0);
   }
   else
   {
      print("Volume: %d\n", self->volume);
   }
}

void changePeriod(ToneObj* self, int period) {
   self->period = period;
}

void changeStop(ToneObj* self, int stop) {
   if (stop > 0) {
      self->stop = true;
   }
   else {
      self->stop = false;
      SEND(self->period, USEC(self->deadline), self, toneGenerator, 0);
   }
}

int getVolume(ToneObj* self, int unused) {
   return self->volume;
}

int getMute(ToneObj* self, int unused) {
   return self->mute;
}
