/**
This is the accumulator sketch. It is the node that is responsible for sounding the alarm if the right sequence of events
happen as reported by the other sensor nodes. To save money and space, it is also a mic/pir/switch sensor node.

It sounds the alarm if it receives a signal from two different nodes (including possibly itself). If it receives
a signal from a node, it enters the alert mode, whereupon it starts a countdown. If, within that countdown, it receives
a second signal (not from the same node), it enters intruder_detected mode and sounds the alarm. If it doesn't receive
the second signal within the alotted time, it resets back to all_clear mode.

If it enters intruder_detected mode, it will sound the alarm for several seconds, and wait for the disarm signal to come.
If it never gets the disarm signal, it eventually resets to all_clear.
*/
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <SPI.h>
#include "RF24.h"

/**Typedefs**/
typedef enum { ALL_CLEAR_MODE, ALERT_MODE, INTRUDER_DETECTED_MODE } alert_level_type;

/**Constants**/
const unsigned long ALARM_COUNTDOWN = 30000;//30,000 milliseconds (30 seconds)//The time the alarm sounds for
const unsigned long INTRUDER_COUNTDOWN = 20000;//20,000 milliseconds (20 seconds)//The amount of time to wait for a second threat signal before reset
const uint16_t THREAT_SIGNAL = 0x1BA0;
const uint16_t DISARM_SIGNAL = 0x1151;
const uint16_t ARM_SIGNAL = 0x1221;
const unsigned int NUMBER_OF_NODES = 5;//Number of nodes in the system including this one

/**Pin declarations**/
const int SENSORS = 3;//PIR and Mag switch through a NOR gate - so LOW when bad guy detected
const int SPEAKER = 8;
const int RADIO_PIN_1 = 9;
const int RADIO_PIN_2 = 10;

/**Radio code**/
RF24 radio(RADIO_PIN_1, RADIO_PIN_2);
//Accumulator and four other nodes. Node 4send is the arm/disarm node. The fifth is a private channel from node4 to accmltr
byte node_ids[][6] = { "cmltr", "1send", "2send", "3send", "4send" , "5xxxx" };

/**State**/
volatile boolean system_armed = false;
volatile alert_level_type danger_level = ALL_CLEAR_MODE;
volatile unsigned long countdown_timer = 0;
volatile boolean alert_from_node[] = { false, false, false, false, false };//Array of alerts from nodes in the system - node 0 is this node.
volatile boolean disarm_flag = false;
volatile boolean arm_system_flag = false;
volatile boolean adjust_threat_level_flag = false;

void setup(void)
{
  /**Pins**/
  pinMode(SENSORS, INPUT);
  pinMode(SPEAKER, OUTPUT);
  
  /**Radio**/
  radio.begin();
  radio.setRetries(15, 15);//Retry 15 times with a delay of 15 microseconds between attempts
  radio.openReadingPipe(1, node_ids[1]);
  radio.openReadingPipe(2, node_ids[2]);
  radio.openReadingPipe(3, node_ids[3]);
  radio.openReadingPipe(4, node_ids[4]);//arm/disarm node broadcast channel
  radio.openReadingPipe(5, node_ids[5]);//private channel to arm/disarm node
  radio.startListening();
  
  attachInterrupt(0, check_messages_ISR, LOW);//Attach an interrupt on a LOW signal on Pin 2
  
  Serial.begin(115200);
}

void loop(void)
{ 
  /*Go through the flags and deal with them*/
  if (disarm_flag)
  {
    disarm_flag = false;
    Serial.println("Disarm flag.");
    disarm();
  }
  
  if (adjust_threat_level_flag)
  {
    adjust_threat_level_flag = false;
    Serial.println("Adjust threat level flag.");
    increase_threat_level();
  }
  
  if (arm_system_flag)
  {
    arm_system_flag = false;
    Serial.println("Arm system flag.");
    system_armed = true;
  }
  
  if (! system_armed)
  {
    Serial.println("Going to sleep.");
    delay(1000);
    //Just go to sleep until the system is armed
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_mode();
  }
  
  /**Manipulate countdown timer and adjust mode accordingly**/
  if (countdown_timer > INTRUDER_COUNTDOWN)
  {//Timer has wrapped around for some reason, reset it.
    Serial.println("Countdown timer has wrapped around. This shouldn't have happened.");
    countdown_timer = 0;
  }
  else if (countdown_timer > 0)
  {
    for (int i = 1; i < 5; i++)
    {
      Serial.print("Node "); Serial.print(i); Serial.print(" "); Serial.println(alert_from_node[i]);
    }

    
    Serial.print("Counting down: "); Serial.println(countdown_timer);
    countdown_timer -= 1000;
  }
  else
  {
    Serial.println("Resetting node information.");
    digitalWrite(SPEAKER, LOW);//stop sounding the alarm
    countdown_timer = 0;
    danger_level = ALL_CLEAR_MODE;
    reset_nodes();
    
    Serial.println("All clear, so going back to sleep.");
    delay(1000);
    if (system_armed)
      attachInterrupt(1, sensor_ISR, LOW);
      
    //when in all clear mode, go to sleep
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_mode();
  }
  delay(1000);
}

void sensor_ISR(void)
{
  Serial.println("Detaching sensor ISR. Sensors will no longer work on this node until disarm.");
  detachInterrupt(1);//Now that we have seen something with this node, don't bother sensing any more until we get disarmed.
  
  if (alert_from_node[0])
    return;//this node is already on alert - this is not new information
  else
    alert_from_node[0] = true;
   
   //if we have made it this far, it means we didn't already know that this node was on alert - increase the threat level
   adjust_threat_level_flag = true;
}

void check_messages_ISR(void)
{
  detachInterrupt(0);
  
  /**Check what happened to trigger interrupt**/
  bool tx, fail, rx = false;
  radio.whatHappened(tx, fail, rx);

  if (!rx)
  {
    attachInterrupt(0, check_messages_ISR, LOW);
    return; //if it wasn't rx that woke us, we have nothing else to do.  
  }
  
  /**If rx, get what we received**/
  uint16_t signal;
  uint8_t reading_pipe_number;
  if (radio.available(&reading_pipe_number))
  {
    radio.read(&signal, sizeof(uint16_t));
  }
  
  
  /**If what we received was the disarm signal, disarm the system**/
  if (signal == DISARM_SIGNAL)
  {//regardless of where we are, reset the system
    disarm_flag = true;
    attachInterrupt(0, check_messages_ISR, LOW);
    return;
  }
  else if (signal == ARM_SIGNAL)
  {//Arm the system and return
    arm_system_flag = true;
    attachInterrupt(0, check_messages_ISR, LOW);//Attach an interrupt on a LOW signal on Pin 3
    return;
  }
  else if (signal == THREAT_SIGNAL)
  {
    if (reading_pipe_number == 5)
      reading_pipe_number = 4;//Should have made the private channel be the fourth node and the broadcast channel the fifth, but too late now!
    
    Serial.print("Heard from: "); Serial.println(reading_pipe_number);
    
    /**If it isn't a disarm signal, it is a threat signal. Ignore it if it is from a radio we already heard threat from**/
    if (alert_from_node[reading_pipe_number])//number could be 1, 2, 3, etc. 0 is reserved for accumulator node (this one).
      return;
    else
      alert_from_node[reading_pipe_number] = true;
    
    
    /**If we haven't left the ISR, it is because we have a new threat from a new node. Adjust mode and countdown.**/
    adjust_threat_level_flag = true;
  }
}

void disarm(void)
{
  detachInterrupt(0);//don't check messages until after the disarm is complete
  detachInterrupt(1);//don't check sensors until armed again
 
  system_armed = false;
  
  digitalWrite(SPEAKER, LOW);//stop sounding the alarm
  countdown_timer = 0;
  danger_level = ALL_CLEAR_MODE;
  reset_nodes();
 
  attachInterrupt(0, check_messages_ISR, LOW);
}

void increase_threat_level(void)
{
  switch (danger_level)
  {
    case ALL_CLEAR_MODE://start counting down while you wait for the next signal
      Serial.println("Upgrading threat level to alert mode!");
      danger_level = ALERT_MODE;
      countdown_timer = INTRUDER_COUNTDOWN;
      system_armed = true;
      break;
    case ALERT_MODE://start sounding the alarm
      Serial.println("Upgrading threat level to intruder detected mode! Sound the alarm!");
      danger_level = INTRUDER_DETECTED_MODE;
      digitalWrite(SPEAKER, HIGH);//sound the alarm!
      countdown_timer = ALARM_COUNTDOWN;
      system_armed = true;
      break;
    case INTRUDER_DETECTED_MODE://reset alarm countdown
      Serial.println("Reset the alarm countdown! Bad guy(s) still here!");
      countdown_timer = ALARM_COUNTDOWN;
      system_armed = true;
      break;
    default:
      Serial.println("An error has occurred in figuring out what danger level we are at.");
      break;
  }
}

void reset_nodes(void)
{
  for (int i = 0; i < NUMBER_OF_NODES; i++)
    alert_from_node[i] = false;
}
