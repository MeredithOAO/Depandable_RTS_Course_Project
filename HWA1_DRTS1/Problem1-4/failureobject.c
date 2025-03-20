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
FailureHandler failurehandler = initFailureHandler();


/////////////////////////
// Failure Handler Methods
/////////////////////////
void leave_failure_mode(FailureHandler* self, int unused) {
   SYNC(&can0, simulate_can_restore, 0);
   self->recovering = true;
   self->failMode = 0;
   AFTER(MSEC(500), &txrxObj, checkNodesAfterFail, 0);
   AFTER(MSEC(600), self, clearRecoverState, 0);

   if (PROBLEM4_ACTIVE) {
      print("I now join back as a musician!\n", 0);
   }
   else {
      print("Leaving silent failure.\n", 0);
   }

   SYNC(&txrxObj, TxHeartbeat, 0);
}

void toggle_failure1(FailureHandler* self, int unused)
{
   if (self->failMode == 1) {
      leave_failure_mode(self, 0);
   }
   else {
      enter_failure_f1(self, 0);
   }
}

void enter_failure_f1(FailureHandler* self, int unused) {
   self->failMode = 1;
   ASYNC(&can0, simulate_can_failure, 0);
   print("Silent failure mode F1 \n", 0);
   SYNC(&musicPlayer, setMusicianMode, 2);

}

void enter_failure_f2(FailureHandler* self, int unused) {
   self->failMode = 2;
   int delta = T_SAMPLE(&timerFault);
   int delay = 5 + (delta % 6);
   ASYNC(&can0, simulate_can_failure, 0);
   print("Silent failure Mode F2, restore automatcially in %d sec\n", delay);
   AFTER(SEC(delay), self, leave_failure_mode, 0);
   SYNC(&musicPlayer, setMusicianMode, 3);
}

void set_failure_f3(FailureHandler* self, int fail) {

   if (fail) {
      self->failMode = 3;
      print("Failure mode F3 \n", 0);
   }
   else {
      ASYNC(self, leave_failure_mode, 0);
   }
}

void enter_failure_mode(FailureHandler* self, int failure_mode) {
   switch (failure_mode)
   {
   case 0:
      if (self->failMode == 1)
         ASYNC(self, leave_failure_mode, 0);
      break;
   case 1:
      if (self->failMode == 0)
         ASYNC(self, enter_failure_f1, 0);
      break;
   case 2:
      if (self->failMode == 0)
         ASYNC(self, enter_failure_f2, 0);
      break;
   default:
      print("unknow failure mode: %d \n", failure_mode);
      break;
   }
}

int getFailMode(FailureHandler* self, int unused) {
   return self->failMode;
}

int getRecoverState(FailureHandler* self, int unused) {
   return self->recovering;
}

void clearRecoverState(FailureHandler* self, int unused) {
   self->recovering = false;
}