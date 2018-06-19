#include <Timezone.h>
#include <Time.h>
#include <sunMoon.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ctime>
#include <Servo.h> 
#include <BlynkSimpleEsp8266.h>

#include "config.h" // SSID, PASS, AUTH, LAT, LONG are defined here


// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);

const int MINUTES_TO_FADE_TO_SUNRISE = 5;
const int MINUTES_TO_FADE_AFTER_SUNSET = 5;

const int SERVO_CLOSED = 2200;
const int SERVO_OPENED = 800;

const int SERVO_PIN = 14;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 0, 10000);
sunMoon  sm;
Servo servo;

time_t t_now = 0;
bool initial_position_set = false;
time_t servo_enable_timeout = 0;
int servo_us = 1500;
bool open_at_sunrise = true;
int open_at_time;
bool close_at_sunset = true;
int close_at_time;

time_t start_opening;
time_t stop_opening;
time_t start_closing;
time_t stop_closing;
  
void setup()
{
  Serial.begin(115200);

  WiFi.begin(SSID, PASS);
  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }

  timeClient.begin();

  sm.init(0, LAT, LONG);

  Blynk.config(AUTH);
  Blynk.connect();
  Blynk.syncVirtual(V1);
  
  servo.attach(SERVO_PIN);
}

BLYNK_WRITE(V0)
{
  int manual_override = param.asInt();
  if(manual_override == -1)
  {
      servo_us = SERVO_CLOSED;
      servo_enable_timeout = t_now + 5;
      Serial.println("Manual close");
  }
  if(manual_override == +1)
  {
      servo_us = SERVO_OPENED;
      servo_enable_timeout = t_now + 5;
      Serial.println("Manual open");
  }
}

// "Open blinds from... to..."
BLYNK_WRITE(V1)
{
    Serial.println("Got time input");
  TimeInputParam t_input(param);
  
  open_at_sunrise = t_input.isStartSunrise();
  if(open_at_sunrise)
  {
    Serial.println("Open at sunrise");
  }
  else
  {
    open_at_time = LocalToUTC(t_input.getStartHour(), t_input.getStartMinute());
    Serial.print("Open at ");
    printDateTime(CE, open_at_time);
    Serial.print(" (in ");
    PrintTime(t_now - open_at_time);
    Serial.println(")");
  }

  close_at_sunset = t_input.isStopSunset();
  if(close_at_sunset)
  {
    Serial.println("Close at sunset");
  }
  else
  {
    close_at_time = LocalToUTC(t_input.getStopHour(), t_input.getStopMinute());
    Serial.print("Close at ");
    printDateTime(CE, close_at_time);
    Serial.print(" (in ");
    PrintTime(t_now - close_at_time);
    Serial.println(")");
    Serial.println();
  }

  PrintDebug();
}

time_t LocalToUTC(int hours, int minutes)
{
  time_t t_now = timeClient.getEpochTime();
  time_t midnight = t_now - hour(t_now) * SECS_PER_HOUR - minute(t_now) * SECS_PER_MIN - second(t_now);
  time_t local = midnight + hours * SECS_PER_HOUR + minutes * SECS_PER_MIN;
  return CE.toUTC(local);
}

int last_blynk_button_state = -1;
const int BLYNK_BUTTON_STATE_CLOSED = 0;
const int BLYNK_BUTTON_STATE_OPEN = 1;
void loop()
{
  timeClient.update();
  t_now = timeClient.getEpochTime();

  RunBlynk();
  

  if(open_at_sunrise)
  {
    // Get current sunrise time
    stop_opening = sm.sunRise(t_now);
  }
  else
  {
    stop_opening = open_at_time;
  }
  start_opening = stop_opening - MINUTES_TO_FADE_TO_SUNRISE * SECS_PER_MIN;
  if(close_at_sunset)
  {
    // Get current sunset time
    start_closing = sm.sunSet(t_now);
  }
  else
  {
    start_closing = close_at_time;
  }
  stop_closing = start_closing + MINUTES_TO_FADE_AFTER_SUNSET * SECS_PER_MIN;

  
  if(!initial_position_set)
  {
    if(t_now <= start_opening || t_now >= stop_closing)
    {
      servo_us = SERVO_CLOSED;
      Blynk.virtualWrite(V0, BLYNK_BUTTON_STATE_CLOSED);
    }
    else
    {
      servo_us = SERVO_OPENED;
      Blynk.virtualWrite(V0, BLYNK_BUTTON_STATE_OPEN);
    }
    servo.writeMicroseconds(servo_us);
    servo_enable_timeout = t_now + 5;
    initial_position_set = true;
  }
  
  if(t_now >= start_opening && t_now < stop_opening)
  {
    if(last_blynk_button_state != BLYNK_BUTTON_STATE_CLOSED)
    {
      last_blynk_button_state = BLYNK_BUTTON_STATE_CLOSED;
      Blynk.virtualWrite(V0, BLYNK_BUTTON_STATE_CLOSED);
    }

    int fade_us = map(t_now, start_opening, stop_opening, SERVO_CLOSED, SERVO_OPENED);

    // if we're already (halfway) there, don't go back during fade
    if(SERVO_OPENED > SERVO_CLOSED)
    {
      servo_us = _max(servo_us, fade_us);
    }
    else
    {
      servo_us = _min(servo_us, fade_us);
    }
    servo_enable_timeout = t_now + 5;
  }

  if(t_now >= start_closing && t_now < stop_closing)
  {
    if(last_blynk_button_state != BLYNK_BUTTON_STATE_OPEN)
    {
      last_blynk_button_state = BLYNK_BUTTON_STATE_OPEN;
      Blynk.virtualWrite(V0, BLYNK_BUTTON_STATE_OPEN);
    }
    
    int fade_us = map(t_now, start_closing, stop_closing, SERVO_OPENED, SERVO_CLOSED);

    // if we're already (halfway) there, don't go back during fade
    if(SERVO_OPENED > SERVO_CLOSED)
    {
      servo_us = _min(servo_us, fade_us);
    }
    else
    {
      servo_us = _max(servo_us, fade_us);
    }
    servo_enable_timeout = t_now + 5;
  }

  if(t_now <= servo_enable_timeout)
  {
    servo.attach(SERVO_PIN);
    servo.writeMicroseconds(servo_us);
  }
  else
  {
    servo.detach();
  }

//  PrintDebug();

  delay(100);
}

void PrintDebug()
{
  Serial.print("Now: ");
  printDateTimeNL(CE, timeClient.getEpochTime());  
  
  Serial.print("Opening from ");
  printDateTime(CE, start_opening);
  Serial.print(" to ");
  printDateTimeNL(CE, stop_opening);
  
  Serial.print("Closing from ");
  printDateTime(CE, start_closing);
  Serial.print(" to ");
  printDateTimeNL(CE, stop_closing);
  
  Serial.print("Servo now (us): ");
  Serial.println(servo_us);
  Serial.println();
}

void PrintTime(time_t t)
{
if(t < 0) t = -t;

int hours = t / SECS_PER_HOUR;
t -= hours * SECS_PER_HOUR;
int mins = t / SECS_PER_MIN;
t -= mins * SECS_PER_MIN;
int secs = t;

if(hours < 10) Serial.print('0');
Serial.print(hours, DEC);
Serial.print(':');
if(mins < 10) Serial.print('0');
Serial.print(mins, DEC);
Serial.print(':');
if(secs < 10) Serial.print('0');
Serial.print(secs, DEC);
}

void printDateTimeNL(Timezone tz, time_t utc)
{
  printDateTime(tz, utc);
  Serial.println();
}

// given a Timezone object, UTC and a string description, convert and print local time with time zone
void printDateTime(Timezone tz, time_t utc)
{
    time_t t = tz.toLocal(utc);
    Serial.print(year(t));Serial.print("-");
    if(month(t)<10)Serial.print("0");Serial.print(month(t));Serial.print("-");
    if(day(t)<10)Serial.print("0");Serial.print(day(t));Serial.print(" ");
    if(hour(t)<10)Serial.print("0");Serial.print(hour(t));Serial.print(":");
    if(minute(t)<10)Serial.print("0");Serial.print(minute(t));Serial.print(":");
    if(second(t)<10)Serial.print("0");Serial.print(second(t));
}

void RunBlynk(){
  if(!Blynk.connected())
  {
    // reconnect
    Blynk.connect();
    Blynk.syncVirtual(V1);
  }
  else{
    Blynk.run();
  }
}
