/*

Arduino Cloud - MQTT Gateway

Copyright (C) 2022 Alessandro Ranellucci

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <ArduinoIoTCloud.h>
#include "Arduino_MQTT_Gateway.h"
#include <ESPmDNS.h>

namespace AMG {

// Ignore MQTT state updates when the last value change from cloud (or local loop)
// was more recent than this duration. This prevents race conditions when device 
// sends regular state updates but we process them after we process cloud changes.
// In other words, this ensures that cloud changes always win over device states.
constexpr unsigned long IGNORE_STATES_FOR = 500;

void Gateway::loop()
{
  // Wait until network connection is established before initializing our MQTT broker
  if (!_started) {
    if (ArduinoCloud.getConnection()->check() != NetworkConnectionState::CONNECTED) {
      return;
    }

    Serial.println("*** Starting Arduino MQTT Broker ***");

    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname(_hostname);

    if (!MDNS.begin(_hostname)) {
      Serial.println("Error setting up MDNS responder!");
      while (1) delay(1000);
    }
    MDNS.addService("mqtt", "tcp", _port);

    Serial.print("mqtt://");
    Serial.print(_hostname);
    Serial.print(".local:");
    Serial.print(_port);
    Serial.print(" (");
    Serial.print(WiFi.localIP());
    Serial.println(")");

    // Start the MQTT broker
    _mqtt_broker = new MqttBroker(_port);
    _mqtt_broker->begin();

    // Start listening for incoming messages
    _mqtt_client = new TinyMqttClient(_mqtt_broker);
    _mqtt_client->setCallback(&Gateway::onMsg);
    _mqtt_client->subscribe("#");

    _started = true;
  }

  _mqtt_broker->loop();
  _mqtt_client->loop();

  // Check if any variable has changed since last time we saw it.
  // This method is a bit resource-intensive, but since ArduinoIoTCloud does not 
  // provide an accessible API for this, we can't rely on its callbacks or timestamps
  // and we need to keep a copy of the values ourselves.
  for (Property* p : ArduinoMQTTGateway._properties) {
    if (p->hasChanged()) {
#ifdef DEBUG_MQTT_GATEWAY
      Serial.println("Property has changed since last loop!");
#endif
      // This means it was changed from cloud or from our loop(), so we need to sync
      // it to the MQTT device.
      if (p->_command_topic != nullptr) {
#ifdef DEBUG_MQTT_GATEWAY
        Serial.print("-> publishing MQTT update to ");
        Serial.print(p->_command_topic);
        Serial.print("; payload = ");
        Serial.println(p->getCommandPayload().c_str());
#endif
        _mqtt_client->publish(p->_command_topic, p->getCommandPayload());
      }
      p->updateLastSeen();
    }
  }
}

void Gateway::onMsg(const TinyMqttClient* client, const Topic& topic, const char* payload, size_t len)
{
  Serial.print("--> received [");
  Serial.print(topic.c_str());
  Serial.print("]: ");
  Serial.println(payload);

  bool deserializedJSON = false;
  bool deserializionFailed = false;
  StaticJsonDocument<200> doc;

  for (Property* p : ArduinoMQTTGateway._properties) {
    if (strcmp(topic.c_str(), p->_state_topic) != 0) continue;
    if ((millis() - p->_last_seen) < IGNORE_STATES_FOR) {
#ifdef DEBUG_MQTT_GATEWAY
      Serial.print("millis() = ");
      Serial.print(millis());
      Serial.print("; p->_last_seen = ");
      Serial.print(p->_last_seen);
      Serial.println("; < IGNORE_STATES_FOR; skipping");
#endif
      continue;
    }
       
    // If a JSON field was specified, extract payload from it
    if (p->_state_json_field != nullptr) {
      if (!deserializedJSON && !deserializionFailed) {
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          deserializionFailed = true;
        }
        deserializedJSON = true;
      }
      if (deserializionFailed) continue;

      p->updateFromMQTT_JSON(doc[p->_state_json_field]);
    } else {
      p->updateFromMQTT(payload);
    }
  }
}

void BoolProperty::updateFromMQTT(const char* payload)
{
  if (strcmp(payload, _state_on) == 0) {
    *_var = true;
    updateLastSeen();
  } else if (strcmp(payload, _state_off) == 0) {
    *_var = false;
    updateLastSeen();
  }
}

void BoolProperty::updateFromMQTT_JSON(const JsonVariant& payload)
{
  if (payload.is<bool>()) {
    *_var = payload.as<bool>();
    updateLastSeen();
  } else if (payload.is<const char*>()) {
    updateFromMQTT(payload.as<const char *>());
  }
}

void IntProperty::updateFromMQTT(const char* payload)
{
  *_var = ::atoi(payload);
  updateLastSeen();
}

void IntProperty::updateFromMQTT_JSON(const JsonVariant& payload)
{
  if (payload.is<int>()) {
    *_var = payload.as<int>();
    updateLastSeen();
  }
}

void FloatProperty::updateFromMQTT(const char* payload)
{
  *_var = ::atof(payload);
  updateLastSeen();
}

void FloatProperty::updateFromMQTT_JSON(const JsonVariant& payload)
{
  if (payload.is<float>()) {
    *_var = payload.as<float>();
    updateLastSeen();
  }
}

void StringProperty::updateFromMQTT(const char* payload)
{
  *_var = String(payload);
  updateLastSeen();
}

void StringProperty::updateFromMQTT_JSON(const JsonVariant& payload)
{
  if (payload.is<const char*>()) {
    *_var = payload.as<String>();
    updateLastSeen();
  }
}

} // namespace AMG

AMG::Gateway ArduinoMQTTGateway;
