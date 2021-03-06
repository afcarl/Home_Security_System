/**
This sketch is for the receiver in the three node topology.
First, it broadcasts the "arm the system" message several times to wake up the system.
Then it listens to the other two nodes
and reacts accordingly (lights its red LED when intruder alert, green otherwise). It doesn't
send any response back though (at least not in this version).
*/
#include <SPI.h>
#include "RF24.h"

/**Pin declarations*/
const unsigned int RED = 8;
const unsigned int GREEN = 6;

/**Radio code*/
const int TIMEOUT = 200;//ms to wait for timeout
RF24 radio(9,10);  // Set up nRF24L01 radio on SPI bus plus pins 9 & 10 
byte talking_pipes[][6] = { "1Node", "2Node" };
byte broadcast_pipes[][6] = { "Node1", "Node2" };

bool pipe1_alert = false;
bool pipe2_alert = false;
bool have_not_broadcasted = true;

void setup(void)
{
  /**Pin sets*/
  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);
    
  /**Radio*/
  radio.begin();
  radio.setRetries(15, 15);
  radio.openReadingPipe(1, talking_pipes[0]);
  radio.openReadingPipe(2, talking_pipes[1]);
  radio.startListening();
}

void loop(void)
{
  if (have_not_broadcasted)
  {
    for (int i = 0; i < 15; i++)//broadcast the wake up signal to all nodes 15 times to make sure they all wake up
      broadcast();
    
    have_not_broadcasted = false;
  }
  
  display_alert_status();
  check_messages();
}

void broadcast(void)
{
  radio.stopListening();
  
  for (int i = 0; i < 2; i++)
  {
    radio.openWritingPipe(broadcast_pipes[i]);
    bool arm_system = true;
    radio.write(&arm_system, sizeof(bool));
  }
  
  radio.startListening();
}

void check_messages(void)
{
  bool intruder_alert = (pipe1_alert || pipe2_alert);
  uint8_t pipe_number;
  if (radio.available(&pipe_number))
  {
    radio.read(&intruder_alert, sizeof(bool));
  }
  
  switch (pipe_number)
  {
    case 1://pipe 1
      pipe1_alert = intruder_alert;
      break;
    case 2://pipe 2
      pipe2_alert = intruder_alert;
      break;
  }
}

void display_alert_status(void)
{
  if (pipe1_alert || pipe2_alert)
  {
    digitalWrite(GREEN, LOW);
    digitalWrite(RED, HIGH);
  }
  else
  {
    digitalWrite(GREEN, HIGH);
    digitalWrite(RED, LOW);
  }
}
