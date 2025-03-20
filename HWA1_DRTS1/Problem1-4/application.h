#ifndef APP_H
#define APP_H

#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/////////////////////////
// Macros
/////////////////////////
#define FREQ                1000
#define PERIOD              USEC((1000000)/(2*FREQ))
#define START_VOLUME        15        
#define MAX_VOLUME          20
#define DAC_ADDR            (0x4000741C)
#define DAC_WRITE(val)      ((*(volatile uint8_t *)DAC_ADDR) = (val))
#define START_TEMPO         120     // bpm
#define START_LENGTH        MSEC((60*1000)/START_TEMPO)
#define START_KEY           0
#define MIN_KEY_VALUE       -10    
#define MIN_TO_TIME         6000000  
#define NUMBER_OF_TONES     32
#define KEY_OFFSET          5
#define TEMPO_PRINT         SEC(2)
#define MUTE_PRINT          SEC(2)
#define HEARTBEAT_PERIOD    MSEC(200)
#define REQ_CONDUCT_TIME    SEC(1)

#define PROBLEM4_ACTIVE     true

#define NODE_FAILED         1
#define NODE_UNKNOWN        2         
#define NODE_CONNECTED      3         
#define NODE_SELF           4      
#define NODE_INTI           5

#define NETWORK_SIZE_MAX    5
#define NETWORK_ARRAY_INIT  {NODE_INTI, NODE_INTI, NODE_INTI, NODE_INTI, NODE_INTI}

// Deadline Priorites //
#define HIGH_PRIORITY       1
#define MEDIUM_PRIORITY     10
#define LOW_PRIORITY        100


/////////////////////////
// CAN
/////////////////////////
#define CAN_MSG_HEARTBEAT       1  // buff = null
#define CAN_MSG_PLAY            2  // buff = null
#define CAN_MSG_UPDATE          3  // buff = null


/////////////////////////
// Class Definitions
/////////////////////////
typedef struct {
    Object super;
    int index;
    Time toneLength;
    int key;
    bool onSwitch;
    bool musicianMode;
    bool musicianWhen2Nodes;
} MusicPlayer;

typedef struct {
    Object super;
    Time period;
    int volume;
    bool active;
    bool mute;
    int deadline;
    bool stop;
} ToneObj;

typedef struct {
    Object super;
    char buffer[128]; // Serial input buffer
    int buffIndex;
    int nodeID;
    int networkNodes[NETWORK_SIZE_MAX];
    int networkSize;
    int conductorNode;
    int leaderNum;
    bool reqConduct;
    bool printTempo;
    bool printMute;
} TxRxObj;

typedef struct {
    Object super;
    int failMode; // 0, 1, 2
    bool recovering;
} FailureHandler;



/////////////////////////
// Object Init Macros
/////////////////////////
#define initMusicPlayer() { initObject(), 0, START_LENGTH, START_KEY, false, true, true}
#define initToneObj() { initObject(), PERIOD, START_VOLUME, false, false, MEDIUM_PRIORITY, false}
#define initTxRxObj() { initObject(), {0}, 0, 0, NETWORK_ARRAY_INIT, 1, 0, 0, 0, 0}
#define initFailureHandler() { initObject(), 0, 0}
extern int periods[];
extern int tones[];
extern char toneLengths[];



/////////////////////////
// Object Initializations
/////////////////////////
extern MusicPlayer musicPlayer;
extern ToneObj toneObj;
extern TxRxObj txrxObj;
extern FailureHandler failurehandler;
extern Serial sci0;
extern Can can0;
extern Timer timerFault;
extern Timer timerNote;



/////////////////////////
// Method Prototypes
/////////////////////////
void play(MusicPlayer*, int);
void musicianPlay(MusicPlayer*, int);
void paus(MusicPlayer*, int);
void changeTempo(MusicPlayer*, int);
void changeKey(MusicPlayer*, int);
void startStopMusic(MusicPlayer*, int);
void setMusicianMode(MusicPlayer*, int);
void setConductorMode(MusicPlayer*, int);
int getMode(MusicPlayer*, int);
int getTempo(MusicPlayer*, int);
int getKey(MusicPlayer*, int);
int getToneIndex(MusicPlayer*, int);
int getPlayStatus(MusicPlayer*, int);
void setIndex(MusicPlayer*, int);
void setOnSwitch(MusicPlayer*, int);
Time getNoteLenght(MusicPlayer*, int);
int getMusicianWhen2Node(MusicPlayer*, int);
void setMusicianWhen2Node(MusicPlayer*, int);
void empty_receiver(MusicPlayer*, int);
void toneGenerator(ToneObj*, int);
void setVolume(ToneObj*, int);
void muteVolume(ToneObj*, int);
void changePeriod(ToneObj*, int);
void changeStop(ToneObj*, int);
int getVolume(ToneObj*, int);
int getMute(ToneObj*, int);
void reader(TxRxObj*, int);
void receiver(TxRxObj*, int);
void TxHeartbeat(TxRxObj*, int);
void RxHeartbeat(TxRxObj*, int);
void nodeDetection(TxRxObj*, int);
void claimConduct(TxRxObj*, int);
void checkNodesAfterFail(TxRxObj*, int);
int getNodeID(TxRxObj*, int);
int getNetworkSize(TxRxObj*, int);
int getReceivingID(TxRxObj*, int);
int getLeaderNum(TxRxObj*, int);
void setConductorNode(TxRxObj*, int);
void printTempo(TxRxObj*, int);
void printMute(TxRxObj*, int);
void leave_failure_mode(FailureHandler*, int);
void toggle_failure1(FailureHandler*, int);
void enter_failure_f1(FailureHandler*, int);
void enter_failure_f2(FailureHandler*, int);
void set_failure_f3(FailureHandler*, int);
void enter_failure_mode(FailureHandler*, int);
int getFailMode(FailureHandler*, int);
int getRecoverState(FailureHandler*, int);
void clearRecoverState(FailureHandler*, int);
void simulate_can_failure(Can*, int);
void simulate_can_restore(Can*, int);
void print(char*, int);



#endif // END APP_H