#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "application.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>


/////////////////////////
// Global Variables
/////////////////////////
//Period values for frequency index -10 to 14
int periods[] = { 2025, 1911, 1804, 1703, 1607, 1517, 1432, 1351,
                         1276, 1204, 1136, 1073, 1012, 956, 902, 851,
                         804, 758, 716, 676, 638, 602, 568, 536, 506 };

int tones[] = { 0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9,
                         7, 5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0 };

char toneLengths[] = { 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
                                'b', 'a', 'a', 'b', 'c', 'c', 'c', 'c', 'a', 'a', 'c',
                                'c', 'c', 'c', 'a', 'a', 'a', 'a', 'b', 'a', 'a', 'b' };


/////////////////////////
// Object Initializations
/////////////////////////
MusicPlayer musicPlayer = initMusicPlayer();
Timer timerFault = initTimer();
Timer timerNote = initTimer();


/////////////////////////
// Musicplayer Methods
/////////////////////////
void play(MusicPlayer* self, int unused) {
    if (self->onSwitch == false || SYNC(&failurehandler, getFailMode, 0) > 0 || self->musicianMode) {
        ASYNC(&toneObj, changeStop, 1);
        self->index = 0;
        self->key = START_KEY;
        return;
        print("returned inside play\n", 0);
    }


    int tone = tones[self->index] + self->key;
    Time period = USEC(periods[tone - MIN_KEY_VALUE]);

    ASYNC(&toneObj, changePeriod, period);

    Time length;
    switch (toneLengths[self->index])
    {
    case 'a':
        length = self->toneLength;
        break;
    case 'b':
        length = self->toneLength << 1;
        break;
    case 'c':
        length = self->toneLength >> 1;
        break;
    default:
        break;
    }
    Time paus_length = length / 6;


    int playingNode = SYNC(&txrxObj, getReceivingID, self->index);
    if (playingNode == -1)
        print("PLAYING NODE NEGATIVE IN PLAY FUNC.\n", 0);

    int nodeID = SYNC(&txrxObj, getNodeID, 0);
    if (nodeID == -1)
        print("NODE ID NEGATIVE IN PLAY FUNC.\n", 0);


    CANMsg msg;
    msg.nodeId = nodeID & (0xF);
    msg.length = 6 & (0x7F);
    msg.msgId = CAN_MSG_PLAY & (0x7F);
    msg.buff[0] = playingNode & 0xFF;
    msg.buff[1] = self->index & 0xFF;
    msg.buff[2] = (MIN_TO_TIME / self->toneLength) & 0xFF;
    msg.buff[3] = (self->key + KEY_OFFSET) & 0xFF;
    msg.buff[4] = SYNC(&toneObj, getVolume, 0) & 0xFF;
    msg.buff[5] = SYNC(&txrxObj, getLeaderNum, 0) & 0xFF;
    CAN_SEND(&can0, &msg);

    if (playingNode == nodeID) {
        ASYNC(&toneObj, changeStop, 0);
        SEND(length - paus_length, 0, self, paus, paus_length);
    }
    else {
        SEND(length, 0, self, play, 0);
    }

    self->index++;
    self->index %= NUMBER_OF_TONES;
}

void paus(MusicPlayer* self, int pauseLength) {
    ASYNC(&toneObj, changeStop, 1);
    SEND(pauseLength, LOW_PRIORITY, self, play, 0);
}

void musicianPlay(MusicPlayer* self, int toneInd) {
    int tone = tones[toneInd] + self->key;
    Time period = USEC(periods[tone - MIN_KEY_VALUE]);
    BEFORE(MEDIUM_PRIORITY, &toneObj, changePeriod, period);

    Time length;
    switch (toneLengths[toneInd])
    {
    case 'a':
        length = self->toneLength;
        break;
    case 'b':
        length = self->toneLength << 1;
        break;
    case 'c':
        length = self->toneLength >> 1;
        break;
    default:
        break;
    }

    Time paus_length = length / 6;

    BEFORE(HIGH_PRIORITY, &toneObj, changeStop, 0); // Enable tone generator
    SEND(length - paus_length, 0, &toneObj, changeStop, 1); // Disable the tonegenerator
}

void changeTempo(MusicPlayer* self, int tempo) {
    if (tempo >= 20 && tempo <= 240) {
        int toneLeng = MIN_TO_TIME / tempo;

        if (self->toneLength != toneLeng) {
            self->toneLength = toneLeng;
            print("Tempo set to: %d\n", tempo);
        }
    }
    else {
        print("Tempo %d not in the intervall of 20-240 bbpm\n", tempo);
    }
}

void changeKey(MusicPlayer* self, int key) {
    if (key >= -5 && key <= 5) {

        if (key != self->key) {
            self->key = key;
            print("Key set to: %d\n", key);
        }
    }
    else {
        print("Key %d is not in the range of [(-5) - 5]\n", key);
    }
}

void startStopMusic(MusicPlayer* self, int unused) {
    self->onSwitch = !self->onSwitch;

    if (self->onSwitch == true)
    {
        ASYNC(self, play, 0);
        print("Music started!\n", 0);
    }
    else {
        print("Music stoped!\n", 0);
    }
}

void setMusicianMode(MusicPlayer* self, int cause) {

    if (self->musicianMode == false) {
        switch (cause)
        {
        case 0:
            print("Conductorship void (higher leader detected).\n", 0);
            break;
        case 1:
            print("Conductorship void (other node claimed conductor).\n", 0);
            break;
        case 2:
            print("Conductorship void due to F1.\n", 0);
            break;
        case 3:
            print("Conductorship void due to F2.\n", 0);
            break;
        case 4:
            print("Conductorship void due to F3.\n", 0);
            break;

        default:
            break;
        }
    }

    self->musicianMode = true;
}

void setConductorMode(MusicPlayer* self, int cause) {

    if (self->musicianMode == true) {
        switch (cause)
        {
        case 0:
            print("I am the new conductor!\n", 0);
            break;

        default:
            break;
        }
    }

    int nodeID = SYNC(&txrxObj, getNodeID, 0);
    SYNC(&txrxObj, setConductorNode, nodeID);
    self->musicianMode = false;
}

int getMode(MusicPlayer* self, int unused) {
    return self->musicianMode;
}

int getTempo(MusicPlayer* self, int unused) {
    return MIN_TO_TIME / self->toneLength;
}

int getKey(MusicPlayer* self, int unused) {
    return self->key;
}

int getToneIndex(MusicPlayer* self, int unused) {
    return self->index;
}

int getPlayStatus(MusicPlayer* self, int unused) {
    return (int)self->onSwitch;
}

void setIndex(MusicPlayer* self, int indx) {
    self->index = indx;
}

void setOnSwitch(MusicPlayer* self, int onSwitch) {
    self->onSwitch = onSwitch;
}

Time getNoteLenght(MusicPlayer* self, int unused) {

    Time length;
    switch (toneLengths[self->index])
    {
    case 'a':
        length = self->toneLength;
        break;
    case 'b':
        length = self->toneLength << 1;
        break;
    case 'c':
        length = self->toneLength >> 1;
        break;
    default:
        break;
    }

    return length;
}

int getMusicianWhen2Node(MusicPlayer* self, int unused) {
    return self->musicianWhen2Nodes;
}

void setMusicianWhen2Node(MusicPlayer* self, int mode) {
    self->musicianWhen2Nodes = mode;
}

void empty_receiver(MusicPlayer* self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
}


/////////////////////////
// Helper functions
/////////////////////////
void print(char* format, int i) {
    char buffer[128];
    snprintf(buffer, 128, format, i); // Convert integer to string
    SCI_WRITE(&sci0, buffer);
}


/////////////////////////
// Start & Main
/////////////////////////
void startApp(MusicPlayer* self, int arg) {
    SCI_INIT(&sci0);
    CAN_INIT(&can0);
    T_RESET(&timerFault);
    T_RESET(&timerNote);

    print("\nPlaying Brother John!\n", 0);
    print("\nTempo: %d, Key: 0\n", START_TEMPO);
    print("Request conductorship with 'c'. \n", 0);
    print("Start/Stop the music with 's'.\n", 0);
    print("Increase/decrease volume with up/down arrows.\n", 0);
    print("Mute a muscician by pressing 't'.\n", 0);
    print("Change tempo by writing an integer between 60 to 240 and then press 't'.\n", 0);
    print("Change key by writing an integer between -5 to 5 and then press 'k'.\n", 0);
    print("Enable printing with 'p'.\n", 0);
    print("\nType the Node ID and then press 'i'\nNode ID:", 0);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&musicPlayer, startApp, 0);
    return 0;
}
