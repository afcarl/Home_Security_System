/**
This is the 1send node. It is a purely sensor node with a PIR 
that has been NOT'ed.

It sleeps until it hears the arm signal, after which it
goes to sleep again, but with an interrupt to listen on its
NOR gate line to see if any intruders are present. If so, it
sends a threat signal to the accumulator node.

If it ever receives the disarm signal, it goes back to sleeping
and waiting for the arm signal.

For now, if a node sends too many signals to the accumulator, it stops sending them
until a disarm/arm cycle. In the future, it would be best for them to send the signal
until the accumulator actually hears and cares about the signal. That way, if the node
sends a signal to the accumulator and it results in a countdown that times out, the node
isn't just left disabled until a disarm/arm cycle - instead it will send the signal again if
it sees something.
*/
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <SPI.h>
#include "RF24.h"

/**Constants**/
const unsigned int TIMES_TO_SEND = 6;//The number of times to try to send a message before giving up
const unsigned int MAX_TIMES_TO_SEND_SIGNAL = 15;//The maximum number of times to send the threat signal to the accumulator per disarm/arm cycle
const uint16_t THREAT_SIGNAL = 0x1BA0;
const uint16_t DISARM_SIGNAL = 0x1151;
const uint16_t ARM_SIGNAL = 0x1221;

/**Pin declarations**/
const int PIR = 3;
const int RADIO_PIN_1 = 9;
const int RADIO_PIN_2 = 10;

/**Radio code**/
RF24 radio(RADIO_PIN_1, RADIO_PIN_2);
//Accumulator and four other nodes. Node 4send is the arm/disarm node. The fifth is a private channel from node4 to accmltr
byte node_ids[][6] = { "cmltr", "1send", "2send", "3send", "4send" , "5send" };

/**State**/
volatile unsigned int sent_signal_times = 0;//Number of times this node has sent the threat signal to the accumulator

void setup(void)
{
  /**Pins**/
  pinMode(PIR, INPUT);
  
  /**Radio**/
  radio.begin();
  radio.setRetries(15, 15);//Retry 15 times with a delay of 15 microseconds between attempts
  radio.openWritingPipe(node_ids[1]);//open up a writing pipe to the accumulator node (the accumulator node reads pipes 1 through 4)
  radio.openReadingPipe(1, node_ids[4]);//arm/disarm node
  radio.startListening();
  
  Serial.begin(115200);
  
  attachInterrupt(0, check_wake_up_ISR, LOW);//Attach an interrupt on a LOW signal on Pin 2
}

void loop(void)
{
  Serial.println("Going to sleep.");
  delay(1000);
  //Just go to sleep - interrupts take care of everything
  set_sleep_mode(SLEEP_MODE_STANDBY);//PWR_DOWN probably takes too long to catch the amplitude signal when waking up
  sleep_enable();
  sleep_mode();
}

void arm_system(void)
{
  Serial.println("Arming.");
  attachInterrupt(1, sensor_ISR, LOW);//Attach an interrupt on a LOW signal on Pin 3
}

void disarm(void)
{
  sent_signal_times = 0;
  Serial.println("Disarming.");
  detachInterrupt(1);//Don't check sensors until armed again
}

void sensor_ISR(void)
{
  if (sent_signal_times < MAX_TIMES_TO_SEND_SIGNAL)
  {
    Serial.println("Bad guys! Sending threat signal.");
    
    //Bad guys are present! Send a threat signal!
    radio.stopListening();
    if (write_to_radio(&THREAT_SIGNAL, sizeof(uint16_t)))
      sent_signal_times++;
    radio.startListening();
  }
}

void check_wake_up_ISR(void)
{
  /**Check what happened to trigger the interrupt**/
  bool tx, fail, rx = false;
  radio.whatHappened(tx, fail, rx);
  
  if (!rx)
    return;//If it wasn't rx, I don't care what it was.
  
  /**If rx, get what we received**/
  uint16_t signal;
  uint8_t reading_pipe_number;
  if (radio.available(&reading_pipe_number))
    radio.read(&signal, sizeof(uint16_t));
    
  if (signal == DISARM_SIGNAL)
  {
    disarm();
  }
  else if (signal == ARM_SIGNAL)
  {
    arm_system();
  }
}

boolean write_to_radio(const void * to_write, uint8_t len)
{
  for (int i = 0; i < TIMES_TO_SEND; i++)
  {
    if (radio.write(to_write, len))
      return true;
  }
  return false;
}
