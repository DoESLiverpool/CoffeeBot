/*
  CoffeeBot	
  Reads an analog input on pin 0, from the strain gauge 
  output in the scales under the coffee machine and 
  converts it to coffee amount in cups, prints the result 
  to the serial monitor and also sends it to a Xively feed.
  
  Attach INA125 output to A2.
  And connect the INA125P's V+ to the Arduino's AREF (for 
  monitoring) and to the Arduino's A5 (for the initial 
  reading).
  Apply a trimpot accross the INA125's gain pins to select 
  based on usable range requirements.
  Add a normally grounded push button to pin 2 and an LED
  to pin 5 for setting the tare reading.
  */

#include <SPI.h>
#include <Ethernet.h>
#include <HttpClient.h>
#include <Xively.h>
#include <EEPROM.h>

// Global Variables
const int analogPin = A2;
const int analogMax = A3;
int sensorValue = 0;
float volts = 0.0;
float cups = 0.0;
float maxCups = 6.0;
float maxVolts = 0.0;
// Set the size of the digital bracketing for the filtering
int currentSensorValue = 0;
char coffeeStatus[] = "";
int tareWeight = 0;
// From testing: 
//      empty sensor value = 660;
//      sensor value with empty pots = 710;
//      full sensor value (12 cups) = 842;
// therefore potWeight = 710-660 = 50;
// and cupWeight = (842-710)/12 = 11;
int potWeight = 50;
int cupWeight = 11;
volatile boolean setTare = 0;
int tareLight = 5;

// Number of milliseconds to wait without receiving any data before we give up
const int kNetworkTimeout = 30*1000;
// Number of milliseconds to wait if no data is available before trying again
const int kNetworkDelay = 1000;

// MAC address for your Ethernet shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// Your Xively key to let you upload data
char XivelyKey[] = "API_KEY HERE";

// Define the strings for our datastream IDs
char rawSensorId[] = "raw_sensor";
char rawVoltageId[] = "sensor_voltage";
char filteredSensorId[] = "filtered_sensor";
char fillLevelId[] = "number_of_cups";
char messageId[] = "status_message";
char tareId[] = "tare_point";
const int bufferSize = 40;
char bufferValue[bufferSize]; // enough space to store the string we're going to send
XivelyDatastream datastreams[] = {
  XivelyDatastream(rawSensorId, strlen(rawSensorId), DATASTREAM_FLOAT),
  XivelyDatastream(rawVoltageId, strlen(rawVoltageId), DATASTREAM_FLOAT),
  XivelyDatastream(filteredSensorId, strlen(filteredSensorId), DATASTREAM_FLOAT),
  XivelyDatastream(fillLevelId, strlen(fillLevelId), DATASTREAM_FLOAT),
  XivelyDatastream(messageId, strlen(messageId), DATASTREAM_BUFFER, bufferValue, bufferSize),
  XivelyDatastream(tareId, strlen(tareId), DATASTREAM_FLOAT),
};
// Finally, wrap the datastreams into a feed
// also set the feed ID to your feed.
XivelyFeed feed(106284, datastreams, 6 /* number of datastreams */);

EthernetClient client;
XivelyClient Xivelyclient(client);

unsigned long lastConnectionTime = 0;                // last time we connected to Xively
const unsigned long connectionInterval = 15*1000;      // delay between connecting to Xively in milliseconds

// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("Send coffee machine status to Xively");
  Serial.println("==================================");

  Serial.println("Initializing network");
  while (Ethernet.begin(mac) != 1) {
    Serial.println("Error getting IP address via DHCP, trying again...");
    delay(15000);
  }
  
  Serial.println("Network initialized");
  Serial.println();
  
  // Measure the reference maximum voltage compared to the 
  // 5V internal reference. Can be running at a lower voltage.
  pinMode(analogMax, INPUT_PULLUP);
  analogReference(INTERNAL);
  delay(500);
  Serial.println(analogRead(analogMax));
  maxVolts = analogRead(analogMax) / 1023.0 * 5.0;
  Serial.print("Maximum Voltage reference: ");
  Serial.println(maxVolts);

  //Set an analogue voltage reference of the return from the V_{in} pin
  delay(500);
  analogReference(EXTERNAL);
  
  // Take the tareReading from the EEPROM
  tareWeight = EEPROM.read(50) * 255 + EEPROM.read(51);
  Serial.print("current tareWeight:");
  Serial.println(tareWeight);
  
  //attach the interrupt call for changing the setTare flag
  pinMode(tareLight, OUTPUT);
  pinMode(2, INPUT);
  attachInterrupt(0, call_tare, RISING);
  
  // ready the current tare point for sending
  datastreams[5].setFloat(tareWeight);
}

// the loop routine runs over and over again forever:
void loop() {
  //If the tare button has been pressed
  if (setTare == 1) {
    Serial.println("Setting the new zero");
    digitalWrite(tareLight, HIGH);
    delay(250);
    
    //start collecting sensor data for 2 seconds
    unsigned long tareTime = millis();
    int tareReading = 0;
    while (millis() - tareTime < 2000UL) {
      int currentTareReading = analogRead(analogPin);
      if (currentTareReading > tareReading) tareReading = currentTareReading;
    }
    
    tareWeight = tareReading;
    setTare = 0;
    delay(250);

    // record the tare to EEEPROM
    EEPROM.write(50, highByte(tareWeight));
    EEPROM.write(51, lowByte(tareWeight));

    digitalWrite(tareLight, LOW);
    delay(100);
    
    Serial.print("new tareWeight:");
    Serial.println(tareWeight);
    
    // ready the current tare point for sending
    datastreams[5].setFloat(tareWeight);

  }
  
  //check the sensor value
  sensorValue = analogRead(analogPin);
  //Then take the maximum value since the last Xively send
  if (sensorValue > currentSensorValue) currentSensorValue = sensorValue;
  
  if (millis() - lastConnectionTime > connectionInterval) {
    //send the last sensor value
    datastreams[0].setFloat(sensorValue);
    Serial.print("sensorValue: ");
    Serial.println(sensorValue);
    
    // convert the reading to a voltage
    volts = sensorValue / 1023.0 * maxVolts;
    datastreams[1].setFloat(volts);
    Serial.print("volts: ");
    Serial.println(volts);

    
    //send the maximum value since the last transmission
    datastreams[2].setFloat(currentSensorValue);
    Serial.print("currentSensorValue: ");
    Serial.println(currentSensorValue);

    // set the number of cups from the sensor value
    cups = float(currentSensorValue - tareWeight - potWeight) / float(cupWeight);
    cups = constrain(cups, 0.0, 12.0);
    datastreams[3].setFloat(cups);
    Serial.print("cups: ");
    Serial.println(cups);
    
    // then set the human specific message
    if (cups > 6.25) { datastreams[4].setBuffer("Two Pots"); }
    else if (cups > 5.0) { datastreams[4].setBuffer("Full Pot"); }
    else if (cups > 2.0) { datastreams[4].setBuffer("Available"); }
    else if (cups > 0.5) { datastreams[4].setBuffer("Low"); }
    else { datastreams[4].setBuffer("Empty"); }
    Serial.print("coffeeStatus: ");
    Serial.println(datastreams[4].getBuffer());
    
    // And send it to Xively
    Serial.println("Uploading it to Xively");
    int ret = Xivelyclient.put(feed, XivelyKey);
    Serial.print("Xivelyclient.put returned ");
    Serial.println(ret);
    Serial.println();
    
    //Reset the max sensor value
    currentSensorValue = -100;
    
    lastConnectionTime = millis();
  }
  
  //Slow the analogRead loop
  delay(50);
}

void call_tare() {
  setTare = 1;
}
