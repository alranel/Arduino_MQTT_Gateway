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

#include "TinyMqtt/TinyMqtt.h"
#include <ArduinoJson.h>
#include <string>

namespace AMG {

class Property {
  public:
  Property& setStateTopic(const char* topic) { _state_topic = topic; return *this; };
  Property& setCommandTopic(const char* topic) { _command_topic = topic; return *this; };
  Property& setStatePayloadJSONField(const char* field) { _state_json_field = field; return *this; };  
  virtual Property& setStatePayload(const char* on, const char* off) {
    Serial.println("Error: setStatePayload() is not compatible with this property type");
    while (true) {}
  };
  virtual Property& setCommandPayload(const char* on, const char* off) {
    Serial.println("Error: setCommandPayload() is not compatible with this property type");
    while (true) {}
  };

  protected:
  virtual void updateFromMQTT(const char* payload);
  virtual void updateFromMQTT_JSON(const JsonVariant& payload);
  virtual bool hasChanged() const;
  virtual void updateLastSeen();
  virtual std::string getCommandPayload() const;
  const char* _state_topic      = nullptr;
  const char* _command_topic    = nullptr;
  const char* _state_json_field = nullptr;
  unsigned long _last_seen = 0;

  friend class Gateway;
};

class BoolProperty : public Property {
  public:
  BoolProperty(bool& var) : _var(&var) {};
  BoolProperty& setStatePayload(const char* on, const char* off) { _state_on = on; _state_off = off; return *this; };
  BoolProperty& setCommandPayload(const char* on, const char* off) { _cmd_on = on; _cmd_off = off; return *this; };

  protected:
  void updateFromMQTT(const char* payload);
  void updateFromMQTT_JSON(const JsonVariant& payload);
  bool hasChanged() const { return _last_seen_value != *_var; };
  void updateLastSeen() { _last_seen_value = *_var; _last_seen = millis(); };
  std::string getCommandPayload() const { return std::string(*_var ? _cmd_on : _cmd_off); };

  private:
  bool* _var;
  bool _last_seen_value;
  const char* _state_on   = "on";
  const char* _state_off  = "off";
  const char* _cmd_on     = "on";
  const char* _cmd_off    = "off";
};

class IntProperty : public Property {
  public:
  IntProperty(int& var) : _var(&var) {};

  protected:
  void updateFromMQTT(const char* payload);
  void updateFromMQTT_JSON(const JsonVariant& payload);
  bool hasChanged() const { return _last_seen_value != *_var; };
  void updateLastSeen() { _last_seen_value = *_var; _last_seen = millis(); };
  std::string getCommandPayload() const { return std::to_string(*_var); };

  private:
  int* _var;
  int _last_seen_value;
};

class FloatProperty : public Property {
  public:
  FloatProperty(float& var) : _var(&var) {};

  protected:
  void updateFromMQTT(const char* payload);
  void updateFromMQTT_JSON(const JsonVariant& payload);
  bool hasChanged() const { return _last_seen_value != *_var; };
  void updateLastSeen() { _last_seen_value = *_var; _last_seen = millis(); };
  std::string getCommandPayload() const { return std::to_string(*_var); };

  private:
  float* _var;
  float _last_seen_value;
};

class StringProperty : public Property {
  public:
  StringProperty(String& var) : _var(&var) {};

  protected:
  void updateFromMQTT(const char* payload);
  void updateFromMQTT_JSON(const JsonVariant& payload);
  bool hasChanged() const { return _last_seen_value != *_var; };
  void updateLastSeen() { _last_seen_value = *_var; _last_seen = millis(); };
  std::string getCommandPayload() const { return std::string(_var->c_str()); };

  private:
  String* _var;
  String _last_seen_value;
};

class Gateway {
  public:
  BoolProperty& add(bool& var) {
    auto p = new BoolProperty(var);
    _properties.push_back(p);
    return *p;
  };
  IntProperty& add(int& var)    {
    auto p = new IntProperty(var);
    _properties.push_back(p);
    return *p;
  };
  FloatProperty& add(float& var)  {
    auto p = new FloatProperty(var);
    _properties.push_back(p);
    return *p;
  };
  StringProperty& add(String& var) {
    auto p = new StringProperty(var);
    _properties.push_back(p);
    return *p;
  };
  
  void loop();

  static void onMsg(const TinyMqttClient* client, const Topic& topic, const char* payload, size_t len);

  private:
  bool _started = false;
  uint16_t _port = 1883;
  const char* _hostname = "arduino-broker";
  MqttBroker* _mqtt_broker;
  TinyMqttClient* _mqtt_client;
  std::vector<Property*> _properties;
};

} // namespace AMG

extern AMG::Gateway ArduinoMQTTGateway;
