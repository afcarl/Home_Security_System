/**
This sketch reads a single card key in to its buffer and
then checks it against its known tags.
*/
#include <SoftwareSerial.h>

SoftwareSerial RFID(6, 7); // RX and TX

int key_a[14] = { 2, 55, 67, 48, 48, 53, 54, 56, 51, 54, 65, 67, 51, 3 };
int key_b[14] = { 2, 55, 67, 48, 48, 49, 57, 68, 56, 66, 51, 48, 69, 3 };
int key_c[14] = { 2, 55, 67, 48, 48, 53, 54, 57, 53, 55, 57, 67, 54, 3 };
int incoming_key[14] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0 }; // used for read comparisons

void setup(void)
{
  RFID.begin(9600);
  Serial.begin(9600);
}

void loop(void)
{
  read_incoming();
}

void read_incoming(void)
{
  if (RFID.available())
  {
    delay(100);
    
    for (int i = 0; i < 14; i++)
    {
      incoming_key[i] = RFID.read();
    }
    RFID.flush();//stops multiple reads
    
    bool access_granted = check_incoming();
    if (access_granted)
    {
      Serial.println("Accepted.");
    }
    else
    {
      Serial.println("Rejected.");
    }
  }
}

boolean check_incoming(void)
{
  if (compare_keys(key_a, incoming_key))
    return true;
  else if (compare_keys(key_b, incoming_key))
    return true;
  else if (compare_keys(key_c, incoming_key))
    return true;
  else
    return false;
}

boolean compare_keys(int aa[14], int bb[14])
{
  boolean ff = false;
  int fg = 0;
  for (int cc = 0 ; cc < 14 ; cc++)
  {
    if (aa[cc] == bb[cc])
    {
      fg++;
    }
  }
  if (fg == 14)
  {
    ff = true;
  }
  return ff;
}
