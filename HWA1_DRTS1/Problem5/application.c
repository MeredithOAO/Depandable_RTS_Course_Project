#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#define BUFF_LENGTH 	9

typedef struct {
    Object super;
    int count;
    char c;
	char msgid;
	char buffer[50];
	bool burst_transmit_en;
	bool print_id_en;
} App;

typedef struct {
	Object super;
	uchar RxMsg[10];
	uchar inter_arrival_time;
	uchar head_index;
	uchar tail_index;
}Regulator;

App app = { initObject(), 0, 'X', 0, {0},false, true};
Regulator reg ={ initObject(), {0}, 0,0, 0};

void reader(App*, int);
void receiver(App*, int);

Serial sci0 = initSerial(SCI_PORT0, &app, reader);

Can can0 = initCan(CAN_PORT0, &app, receiver);
Timer timer0 = initTimer();
Timer timer1 = initTimer();

void print(char* format, int i) {
    char buffer[128];
    snprintf(buffer, 128, format, i); // Convert integer to string
    SCI_WRITE(&sci0, buffer);
}



void transmitCanMsg(App* self) {
	CANMsg msg;

	self->msgid++;
	self->msgid %= 128;
	msg.nodeId = 1;
	msg.length = 2 & (0x7F);
	msg.msgId = self->msgid;

	CAN_SEND(&can0, &msg);
	if(self->print_id_en == true) {
		print("\n sent message sequence: %d \n", self->msgid);
	}
	return;

}

void burst_transmit(App* self)
{
	if(self->burst_transmit_en)
	{
		ASYNC(self, transmitCanMsg, 0);
		AFTER(MSEC(500), self, burst_transmit,0); 
		return;
	}
	else{
		return;
	}
}

void pull_buffer(Regulator *self, int unused)
{
	int next;		//circular buffer
	if(self->head_index == self->tail_index) {
		print("buff is empty \n",0);
		return; 
		}
	
	next = self->tail_index +1;
	next %= (BUFF_LENGTH-1);
	
	if(T_SAMPLE(&timer1) >= SEC(self->inter_arrival_time)) {
	
	print("Received message with ID : %d ", self->RxMsg[self->tail_index]);
	print(" at time : %d sec \n", (int)((T_SAMPLE(&timer0))/100000));
	self->tail_index = next;
	T_RESET(&timer1);
	
	if(self->head_index != self->tail_index) {
		AFTER(SEC(self->inter_arrival_time), self, pull_buffer, 0);
		}
	
	}
	else {
		Time diff = SEC(self->inter_arrival_time) - T_SAMPLE(&timer1);
		AFTER(diff, self, pull_buffer, 0);
	}

	return;

}

void push_buffer(Regulator *self, int canmsgID) {
	
	int next;
	next = self->head_index+1;
	next %=  (BUFF_LENGTH-1);
		
	if(next == self->tail_index) {
		print("Buffer full, message lost: %d\n", canmsgID);
		return;
	}
	else if (self->head_index == self->tail_index) {
		ASYNC(self, pull_buffer, 0);
	}
		
	self->RxMsg[self->head_index] = canmsgID;
	self->head_index = next;
	return;
}

void receiver(App *self, int unused) {
	CANMsg canmsg;
	CAN_RECEIVE(&can0, &canmsg);
	ASYNC(&reg, push_buffer, canmsg.msgId);
	return;
}



void set_inter_arrival(Regulator *self, int inter_arrival) {
	self->inter_arrival_time = inter_arrival;
	return;
}

void reader(App *self, int c) {
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
	switch (c) {
		case '0'...'9':
		case '-':
			// Add valid value into buffer
			
			self->buffer[self->count++] = c;
			break;
			
		case 'r':  //sets the regulator speed with time in ms

			self->buffer[self->count] = '\0'; //delimiter
			self->count = 0;
			int inter_arrival =0;
			inter_arrival = atoi(self->buffer);
			ASYNC(&reg, set_inter_arrival, inter_arrival);
			print("Regulator speed set to : %d s\n", inter_arrival);
			break;
		
	  case 'o': // send one message
		 self->count = 0;
		 ASYNC(self, transmitCanMsg, 0);
		 
		 break;



	  case 'b': // send burst of message
		 self->count = 0;
		 self->burst_transmit_en =true; /////
		 T_RESET(&timer0);
		 ASYNC(self, burst_transmit, 0);///////
		 
		 break;

	  case 'x': // stop sending burst of msg
		 self->count = 0;
		 self->burst_transmit_en = false;
		 print("stoped CAN burst\n",0);
		 break;
		 
	  case 'i': 	//view the id of Tx message
		 self->count = 0;
		 if(self->print_id_en == true) {
			 self->print_id_en = false;
		 }
		 else{
			 self->print_id_en= true;
		 }
		 
		 break;
	  case 'd':		// for debug
		self->count = 0;
		 print("Timer : %ld \n", T_SAMPLE(&timer0));
		 break;


	  default: //Store charachter in internal buffer
		self->count = 0;
		 print("\nInvalid command\n",0);
  }
}


void startApp(App *self, int arg) {
    //CANMsg msg;
	T_RESET(&timer0);

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");
	
	print("\nCAN message regulator\n", 0);    
    print("Send single CAN message with 'o'. \n", 0);
    print("Start sending burst CAN message with 'b'.\n", 0);
    print("Stop sending burst CAN message with 'x'\n", 0);
    print("Enable and disable Tx message id with 'i'.\n", 0);
	print("Enter regulator speed in milliseconds followed by 'r'.\n", 0);
}

int main() {
    INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
	INSTALL(&can0, can_interrupt, CAN_IRQ0);
    TINYTIMBER(&app, startApp, 0);
    return 0;
}
