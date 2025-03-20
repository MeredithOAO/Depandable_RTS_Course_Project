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
TxRxObj txrxObj = initTxRxObj();
Serial sci0 = initSerial(SCI_PORT0, &txrxObj, reader);
Can can0 = initCan(CAN_PORT0, &txrxObj, receiver);


/////////////////////////
// TxRxObj Methods
/////////////////////////
void reader(TxRxObj* self, int c) {
   bool conductorMode = !SYNC(&musicPlayer, getMode, 0);
   bool sendUpdate = true;

   if (SYNC(&failurehandler, getFailMode, 0))
      sendUpdate = false;

   if (c == '\n') return;
   int bufferValue;

   if (conductorMode) {
      switch (c) {
      case 'e': // into silent failure state
         print("%c\n", c);
         self->buffer[self->buffIndex] = '\0';
         self->buffIndex = 0;
         bufferValue = atoi(self->buffer);
         ASYNC(&failurehandler, enter_failure_mode, bufferValue); // 0 out failure   1:f1  2:f2  f3: use cable in real world
         break;

      case 's': // Start/Stop playing music
         print("%c\n", c);
         SYNC(&musicPlayer, startStopMusic, 0);
         break;

      case 't': // Change tempo
         print("%c\n", c);
         self->buffer[self->buffIndex] = '\0';
         self->buffIndex = 0;
         bufferValue = atoi(self->buffer);
         SYNC(&musicPlayer, changeTempo, bufferValue);
         break;

      case 'k': // Change key
         print("%c\n", c);
         self->buffer[self->buffIndex] = '\0';
         self->buffIndex = 0;
         bufferValue = atoi(self->buffer);
         SYNC(&musicPlayer, changeKey, bufferValue);
         break;

      case 'r': // Reset all 
         print("%c\n", c);
         self->buffer[self->buffIndex] = '\0';
         self->buffIndex = 0;
         SYNC(&musicPlayer, changeTempo, START_TEMPO);
         SYNC(&musicPlayer, changeKey, START_KEY);
         break;

      case 'p': // Start printing tempo
         print("%c\n", c);
         self->buffer[self->buffIndex] = '\0';
         self->buffIndex = 0;
         self->printTempo = !self->printTempo;
         ASYNC(self, printTempo, 0);
         break;

      case 30: // Increase volume
         int vol = SYNC(&toneObj, getVolume, 0);
         SYNC(&toneObj, setVolume, vol + 1);
         break;

      case 31: // Decrease volume
         vol = SYNC(&toneObj, getVolume, 0);
         SYNC(&toneObj, setVolume, vol - 1);
         break;

      case 'm': // Display members
         print("\n--------- MEMBERSHIP ---------\n", 0);
         for (int i = 0; i < NETWORK_SIZE_MAX; i++) {
            if (self->networkNodes[i] == NODE_UNKNOWN ||
               self->networkNodes[i] == NODE_CONNECTED) {
               print("NODE ID: %d, Active Musician.\n", i);
            }
            else if (self->networkNodes[i] == NODE_SELF) {
               print("NODE ID: %d, Active Conductor.\n", i);
            }
            else if (self->networkNodes[i] == NODE_FAILED) {
               print("NODE ID: %d, Failed.\n", i);
            }

         }
         print("------------------------------\n", 0);
         sendUpdate = false;
         break;

      default: //Store charachter in internal buffer
         if ((c >= '0' && c <= '9') ||
            (c == 45))
         {
            print("%c", c);
            self->buffer[self->buffIndex++] = c;
         }
         else {
            print("Value %d is invalid\n", c);
         }
         sendUpdate = false;
         break;
      }

      if (sendUpdate) {
         CANMsg msg;
         msg.nodeId = self->nodeID & (0xF);
         msg.length = 6 & (0x7F);
         msg.msgId = CAN_MSG_UPDATE & (0x7F);
         msg.buff[0] = SYNC(&musicPlayer, getTempo, 0) & 0xFF;
         msg.buff[1] = (SYNC(&musicPlayer, getKey, 0) + KEY_OFFSET) & 0xFF;
         msg.buff[2] = SYNC(&toneObj, getVolume, 0) & 0xFF;
         msg.buff[3] = SYNC(&musicPlayer, getToneIndex, 0) & 0xFF;
         msg.buff[4] = SYNC(&musicPlayer, getPlayStatus, 0) & 0xFF;
         msg.buff[5] = self->leaderNum & 0xFF;
         CAN_SEND(&can0, &msg);
      }

   }
   else {
      switch (c) {
      case 'e': // into silent failure state
         print("%c\n", c);
         self->buffer[self->buffIndex] = '\0';
         self->buffIndex = 0;
         bufferValue = atoi(self->buffer);
         ASYNC(&failurehandler, enter_failure_mode, bufferValue); // 0 out failure   1:f1  2:f2  f3: use cable in real world
         break;

      case 'i': // Set ID
         print("%c\n", c);
         self->buffer[self->buffIndex] = '\0';
         self->buffIndex = 0;
         bufferValue = atoi(self->buffer);
         self->nodeID = bufferValue;
         self->networkNodes[bufferValue] = NODE_SELF;
         print("Node ID set to: %d\n", self->nodeID);
         ASYNC(&txrxObj, TxHeartbeat, 0);
         AFTER(HEARTBEAT_PERIOD * 2, &txrxObj, nodeDetection, 0);

         break;

      case 'c': // Request conductorship
         print("%c\n", c);
         self->reqConduct = true;
         print("Requesting conductorship...\n", 0);
         AFTER(REQ_CONDUCT_TIME, self, claimConduct, 0);
         break;

      case 't': //Mute
         print("%c\n", c);
         ASYNC(&toneObj, muteVolume, 0);
         ASYNC(self, printMute, 0);
         break;

      case 'p': // Start printing mute state
         print("%c\n", c);
         self->buffer[self->buffIndex] = '\0';
         self->buffIndex = 0;
         self->printMute = !self->printMute;
         ASYNC(self, printMute, 0);
         break;

      default: //Store charachter in internal buffer
         if ((c >= '0' && c <= '9') ||
            (c == 45))
         {
            print("%c", c);
            self->buffer[self->buffIndex++] = c;
         }
         else {
            print("Value %d is invalid\n", c);
         }
         break;
      }
   }
}

void receiver(TxRxObj* self, int unused) {
   CANMsg msg;
   CAN_RECEIVE(&can0, &msg);
   int leadNum = 0;

   // Read leaderNum
   switch (msg.msgId) {
   case CAN_MSG_HEARTBEAT:
      leadNum = msg.buff[1];
      break;
   case CAN_MSG_PLAY:
      leadNum = msg.buff[5];
      break;
   case CAN_MSG_UPDATE:
      leadNum = msg.buff[5];
      break;
   }

   // Check for valid leader number
   if (leadNum < self->leaderNum || msg.nodeId == self->nodeID) {
      return; // Disregard messages from old conductor or from own board
   }
   else if (leadNum > self->leaderNum) {
      self->leaderNum = leadNum;
      // Switch to musician mode if a new conductor starts transmitting
      SYNC(&musicPlayer, setMusicianMode, 0);
   }

   // Read content of message
   switch (msg.msgId) {
   case CAN_MSG_HEARTBEAT:
      ASYNC(self, RxHeartbeat, msg.nodeId);

      // Other node transmitts as conductor 
      if (msg.buff[0] == 0) {
         SYNC(&musicPlayer, setMusicianMode, 1);
         self->conductorNode = msg.nodeId;
      }

      // Handle similtanious conductorship requests
      if (msg.buff[2] && self->reqConduct && (self->nodeID > msg.nodeId)) {
         self->reqConduct = false; // Give up request if other node has lower ID
         print("Canceling conductorship request due to request from higher ranked node.\n", 0);
      }
      break;
   case CAN_MSG_PLAY:
      T_RESET(&timerNote);
      ASYNC(&musicPlayer, setIndex, msg.buff[1]);
      ASYNC(&musicPlayer, changeTempo, msg.buff[2]);
      ASYNC(&musicPlayer, changeKey, (msg.buff[3] - KEY_OFFSET));
      ASYNC(&toneObj, setVolume, msg.buff[4]);
      if (self->nodeID == msg.buff[0]) {
         ASYNC(&musicPlayer, musicianPlay, msg.buff[1]);
      }
      break;
   case CAN_MSG_UPDATE:
      ASYNC(&musicPlayer, changeTempo, msg.buff[0]);
      ASYNC(&musicPlayer, changeKey, (msg.buff[1] - KEY_OFFSET));
      ASYNC(&toneObj, setVolume, msg.buff[2]);
      ASYNC(&musicPlayer, setOnSwitch, msg.buff[4]);
      break;
   }
}

void TxHeartbeat(TxRxObj* self, int unused) {
   int failMode = SYNC(&failurehandler, getFailMode, 0);
   if (failMode == 1 || failMode == 2)
      return;

   CANMsg msg;
   msg.nodeId = self->nodeID & (0xF);
   msg.length = 3 & (0x7F);
   msg.msgId = CAN_MSG_HEARTBEAT & (0x7F);
   msg.buff[0] = (SYNC(&musicPlayer, getMode, 0)) & 0xFF;
   msg.buff[1] = self->leaderNum & 0xFF;
   msg.buff[2] = self->reqConduct & 0xFF;
   CAN_SEND(&can0, &msg);
   AFTER(HEARTBEAT_PERIOD, self, TxHeartbeat, 0);
}

void RxHeartbeat(TxRxObj* self, int nodeId) {
   self->networkNodes[nodeId] = NODE_CONNECTED;
}

void nodeDetection(TxRxObj* self, int unused) {
   int newSize = 1;
   int rank = 0;
   bool conductornodePresent = false;
   int lastSize = self->networkSize;

   bool musicianWhen2Nodes = SYNC(&musicPlayer, getMusicianWhen2Node, 0);
   bool failState = SYNC(&failurehandler, getFailMode, 0);
   bool recoverState = SYNC(&failurehandler, getRecoverState, 0);
   bool musicianModeActive = SYNC(&musicPlayer, getMode, 0);

   // Check if conductor is still in network
   if (self->networkNodes[self->conductorNode] == NODE_CONNECTED ||
      !musicianModeActive) {
      conductornodePresent = true;
   }

   for (int i = 0; i < NETWORK_SIZE_MAX; i++) {

      if (self->networkNodes[i] <= NODE_UNKNOWN) {
         self->networkNodes[i] = NODE_FAILED;
      }
      else if (self->networkNodes[i] == NODE_CONNECTED) {
         newSize++;
         self->networkNodes[i] = NODE_UNKNOWN;

         if (i < self->nodeID)
            rank++;
      }
   }

   self->networkSize = newSize;

   if (self->networkSize >= 2) {
      SYNC(&musicPlayer, setMusicianWhen2Node, musicianModeActive);
   }

   // Set failure F3
   if (self->networkSize == 1 && ((!musicianWhen2Nodes && PROBLEM4_ACTIVE) || lastSize > 2 ||
      (!PROBLEM4_ACTIVE && musicianModeActive))
      && !failState && !recoverState) {

      ASYNC(&failurehandler, set_failure_f3, 1);
      SYNC(&musicPlayer, setMusicianMode, 4);

   }
   // Claim conductorship
   else if (!conductornodePresent && rank == 0 && !recoverState
      && !failState && self->networkSize >= 1 && PROBLEM4_ACTIVE) {
      ASYNC(&txrxObj, claimConduct, 1);
   }


   // Clear failure F3
   if (self->networkSize > 1 && SYNC(&failurehandler, getFailMode, 0) == 3)
      ASYNC(&failurehandler, set_failure_f3, 0);


   AFTER(HEARTBEAT_PERIOD * 2, &txrxObj, nodeDetection, 0);
}

void claimConduct(TxRxObj* self, int directly) {
   if (self->reqConduct || directly) { // Check that no other similtaneus request has voided our request
      self->reqConduct = false;

      int toneInd = SYNC(&musicPlayer, getToneIndex, 0);
      if (toneInd == -1)
         print("ToneInd NEGATIVE IN CLAIM CONDUCT.\n", 0);

      self->leaderNum++;
      CANMsg msg;
      msg.nodeId = self->nodeID & (0xF);
      msg.length = 6 & (0x7F);
      msg.msgId = CAN_MSG_UPDATE & (0x7F);
      msg.buff[0] = SYNC(&musicPlayer, getTempo, 0) & 0xFF;
      msg.buff[1] = (SYNC(&musicPlayer, getKey, 0) + KEY_OFFSET) & 0xFF;
      msg.buff[2] = SYNC(&toneObj, getVolume, 0) & 0xFF;
      msg.buff[3] = (toneInd + 1) & 0xFF;
      msg.buff[4] = SYNC(&musicPlayer, getPlayStatus, 0) & 0xFF;
      msg.buff[5] = self->leaderNum & 0xFF;
      CAN_SEND(&can0, &msg);

      Time noteLenght = SYNC(&musicPlayer, getNoteLenght, 0);
      if (noteLenght == -1)
         print("NOTE LENGHT NEGATIVE IN CLAIM CONDUCT.\n", 0);

      Time startPlay = MSEC(10);
      Time playDiff = T_SAMPLE(&timerNote);

      if (noteLenght > playDiff) {
         startPlay = (noteLenght - playDiff);
      }

      SYNC(&musicPlayer, setIndex, msg.buff[3]);
      BEFORE(LOW_PRIORITY, &musicPlayer, setConductorMode, 0); // Start conducting
      SEND(startPlay, LOW_PRIORITY, &musicPlayer, play, 0);
   }
}

void checkNodesAfterFail(TxRxObj* self, int unused) {
   if (self->networkSize == 1 && PROBLEM4_ACTIVE) {
      SYNC(&musicPlayer, setMusicianWhen2Node, 1);
      ASYNC(self, claimConduct, 1);
   }
}

void printTempo(TxRxObj* self, int unused) {
   int tempo = SYNC(&musicPlayer, getTempo, 0);

   bool conductor = !SYNC(&musicPlayer, getMode, 0);
   if (conductor && self->printTempo) {
      print("TEMPO: %d\n", tempo);
      AFTER(TEMPO_PRINT, self, printTempo, 0);
   }
}

void printMute(TxRxObj* self, int unused) {
   bool musician = SYNC(&musicPlayer, getMode, 0);
   bool muted = SYNC(&toneObj, getMute, 0);

   if (musician && self->printMute && muted) {
      print("MUTED \n", 0);
      AFTER(TEMPO_PRINT, self, printMute, 0);
   }
}

int getNodeID(TxRxObj* self, int unused) {
   return self->nodeID;
}

int getNetworkSize(TxRxObj* self, int unused) {
   return self->networkSize;
}

int getReceivingID(TxRxObj* self, int toneIndex) {

   int mod = toneIndex % self->networkSize;
   int nodeFailed = 0;

   for (int i = 0; i < NETWORK_SIZE_MAX; i++)
   {
      while (self->networkNodes[i + nodeFailed] == NODE_FAILED ||
         self->networkNodes[i + nodeFailed] == NODE_INTI) {
         nodeFailed++;
      }

      if (i == mod) {
         return (i + nodeFailed);
      }
   }

   return 0;
}

int getLeaderNum(TxRxObj* self, int unused) {
   return self->leaderNum;
}

void setConductorNode(TxRxObj* self, int nodeID) {
   self->conductorNode = nodeID;
}

void simulate_can_failure(Can* obj, int unused) {
   obj->meth = (Method)empty_receiver;
}

void simulate_can_restore(Can* obj, int unused) {
   obj->meth = (Method)receiver;
}