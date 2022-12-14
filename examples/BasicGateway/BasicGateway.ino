/*
  This example shows how to configure a bunch of MQTT devices and have
  their properties synced with cloud variables.

  For this to work, you'll need to create the following variables in
  Arduino Cloud:
  * shelly02_relay       (type: boolean)
  * shelly02_power       (type: float)
  * shelly02_temperature (type: float)

  License: public domain.
*/

#include "arduino_secrets.h"
#include "thingProperties.h"
#include "Arduino_MQTT_Gateway.h"

void setup() {
  /* This is the standard boilerplate generated by Arduino Cloud */
  Serial.begin(9600);
  delay(1500); 
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  /* 
    Here we configure a Shelly Plug S device. We want to:
    1) control its relay (on/off) remotely from Arduino Cloud;
    2) have its status reflected in Arduino Cloud in case the physical
       button is pressed;
    3) collect its power measurement and temperature data in the cloud.
  */

  // Shelly Plug S - relay
  ArduinoMQTTGateway.add(shelly02_relay)
    .setStateTopic("shellies/shellyplug-s-4022D889B588/relay/0")
    .setCommandTopic("shellies/shellyplug-s-4022D889B588/relay/0/command")
    .setStatePayload("on", "off")
    .setCommandPayload("on", "off");

  // Shelly Plug S - active power measurement
  ArduinoMQTTGateway.add(shelly02_power)
    .setStateTopic("shellies/shellyplug-s-4022D889B588/relay/0/power");

  // Shelly Plug S - temperature
  ArduinoMQTTGateway.add(shelly02_temperature)
    .setStateTopic("shellies/shellyplug-s-4022D889B588/temperature");
}

void loop() {
  ArduinoCloud.update();
  ArduinoMQTTGateway.loop();
}

