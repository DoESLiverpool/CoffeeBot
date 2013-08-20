/*
  CoffeeMonitor
  Reads a feed from Xively and displays the amount of cups
  from 0-6 on a dial.
  
  Attach a 5V analog dial to pin 6
*/

#include <SPI.h>
#include <Ethernet.h>
#include <HttpClient.h>
#include <Xively.h>

// MAC address for your Ethernet shield
byte mac[] = { 0xDE, 0xCA, 0xFF, 0xC0, 0xFF, 0xEE };

// Your Xively key to let you upload data
char XivelyKey[] = "API_KEY HERE";

// Define the string for our datastream ID
char streamId[] = "number_of_cups";

XivelyDatastream datastreams[] = {
  XivelyDatastream(streamId, strlen(streamId), DATASTREAM_FLOAT),
};
// Finally, wrap the datastreams into a feed
// You'll need the feed ID for your CoffeeBot
XivelyFeed feed(106284, datastreams, 1 /* number of datastreams */);

EthernetClient client;
XivelyClient Xivelyclient(client);

const int pwmPin = 6;

unsigned long getTimer = 0;
unsigned long getInterval = 5000; //minimum refresh interval

void setup() {
  pinMode(pwmPin, OUTPUT);
  
  // put your setup code here, to run once:
  Serial.begin(9600);
  
  Serial.println("Get coffee machine status from Xively");
  Serial.println("=====================================");
  Serial.println();
  
  analogWrite(pwmPin, 100);

  while (Ethernet.begin(mac) != 1)
  {
    Serial.println("Error getting IP address via DHCP, trying again...");
    delay(15000);
  }
  
  analogWrite(pwmPin, 200);
  delay(250);
  analogWrite(pwmPin, 0);
  delay(1000);

  // Initialise the dial to show it's working with a nice sweep.
  sweepDial(2, 2);
  delay(250);

}

void loop() {
  if(millis() - getTimer >= getInterval) {
    Serial.println("Doing Xivelyclient.get...");
    int ret = Xivelyclient.get(feed, XivelyKey);
    Serial.print("Xivelyclient.get returned ");
    Serial.println(ret);
  
    if (ret > 0)
    {
      Serial.println("Datastream is...");
      Serial.println(feed[0]);
  
      Serial.print("No. cups are: ");
      Serial.println(feed[0].getFloat());
    }
    
    float cups = feed[0].getFloat();
    if (cups > 6) {cups = 6;}
    int val = 255.0 * (cups / 6.0);
    analogWrite(pwmPin, val);
    
    Serial.print("Set cups to ");
    Serial.println(cups); 
    Serial.print("Val: ");
    Serial.println(val); 
  
    Serial.println();
    
    getTimer = millis();
  }
  
  delay(100);
  
}

void sweepDial(int sweepSpeed, int repeats) {
  for (int n = 1; n <= repeats; n++) {
    int x = 1;
    for (int i = 0; i > -1; i = i + x) {
        analogWrite(pwmPin, i);
        if (i == 255) {
          x = -1;             // switch direction at peak
          delay(250);
        }
        delay(sweepSpeed);
     }
     delay(250);
  }
}
