/*
 * Copyright (C) 2016 Nicola Corna <nicola@corna.info>
 *
 * This file is part of Playstools
 *
 * Playstools is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Playstools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <cstdint>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <Ticker.h>

#include <ArduinoJson.h>
#include <Adafruit_MPR121.h>

#include "RGBFader/RGBFader.h"

const char *ssid = "Playstools";
const char *password = "[password_here]";
const uint16_t tcp_port = 33965;
const uint16_t master_buf_size = 64;
const uint16_t slave_buf_size = 256;
const IPAddress netmask (255, 255, 255, 0);

const uint8_t stools_num = 8;
const IPAddress stools_ip[stools_num] = {
  IPAddress(192, 168, 118, 1),
  IPAddress(192, 168, 118, 2),
  IPAddress(192, 168, 118, 3),
  IPAddress(192, 168, 118, 4),
  IPAddress(192, 168, 118, 5),
  IPAddress(192, 168, 118, 6),
  IPAddress(192, 168, 118, 7),
  IPAddress(192, 168, 118, 8)
};

const uint8_t addr_pin_2 = 16;
const uint8_t addr_pin_1 = 14;
const uint8_t addr_pin_0 = 12;

const uint8_t red_pin = 15;
const uint8_t green_pin = 2;
const uint8_t blue_pin = 13;

const uint8_t sda_pin = 4;
const uint8_t scl_pin = 5;

const uint8_t mpr121_address = 0x5A;

const uint16_t i2cPoll_ms = 100;

const uint8_t max_clients = 10;
const uint8_t max_colors = 255;
const uint8_t max_timers = 64;

const uint8_t master_stool_num = 0;

//--------------------------------------------------------------------

struct Color_info {
  const RGB *colors;
  uint8_t colors_num;
  uint8_t step_time;
  uint8_t stool_num;
};

const RGB blackred[] = { RGB(0, 0, 0), RGB(255, 0, 0) };

const RGB rgb_pins = { .red = red_pin, .green = green_pin, .blue = blue_pin };

RGBFader *fader;
Ticker nextStepTicker;
Ticker i2cPollTicker;
IPAddress ip;
Adafruit_MPR121 touch = Adafruit_MPR121();

bool touched = false;
uint8_t my_stool_num;
char my_name[15];

RGB colors[max_colors] = { RGB() }; //Default-initialization of RGB is black
uint8_t colors_size = 1;

namespace master {
  WiFiServer server(tcp_port);
  WiFiClient clients[max_clients];
  String clients_ip[max_clients];

//  RGB colors[max_colors];
  Color_info color_info[max_timers];
  Ticker timers[max_timers];
  Ticker enable_animations;
  uint8_t active_timers;

  bool running_animation = false;
  uint8_t connected_clients = 0;
}

namespace slave {
  WiFiClient tcp_client;
}

//--------------------FUNCTION DECLARATIONS--------------------------

namespace master {
  void update_clients_list();
  void check_incoming_data();
  void flush_rx();
  void light_command(Color_info *color_info);
  void start_animation(uint8_t animation_number);
  void enable_animations_wrapper();

  namespace animations {
    template <typename T> void shuffle_array(T array[], uint8_t size);
    RGB random_color();
    
    uint16_t a0(uint8_t stools_array[], uint8_t connected_stools);
    uint16_t a1(uint8_t stools_array[], uint8_t connected_stools);
    uint16_t a2(uint8_t stools_array[], uint8_t connected_stools);
    uint16_t a3(uint8_t stools_array[], uint8_t connected_stools);
    uint16_t a4(uint8_t stools_array[], uint8_t connected_stools);
    uint16_t a5(uint8_t stools_array[], uint8_t connected_stools);
    uint16_t a6(uint8_t stools_array[], uint8_t connected_stools);
    uint16_t a7(uint8_t stools_array[], uint8_t connected_stools);
    uint16_t test_rgb(uint8_t stools_array[], uint8_t connected_stools);

    uint16_t (* const animations_jump_table[]) (uint8_t [], uint8_t) = {
      &a0, &a1, &a2, &a3, &a4, &a5, &a6, &a7
    };
  }
}

namespace slave {
  void wifi_connect();
  void connect_to_server();
  void check_incoming_data();
}

void i2cPoll();
void nextStep_wrapper();
void execute_rgb_sequence(const RGB colors[], const uint8_t colorsNum, uint8_t step_time);

void setup() {  
  delay(1000);

  pinMode(addr_pin_0, INPUT);
  pinMode(addr_pin_1, INPUT);
  pinMode(addr_pin_2, INPUT);
  
  pinMode(red_pin, OUTPUT);
  pinMode(green_pin, OUTPUT);
  pinMode(blue_pin, OUTPUT);

  analogWriteFreq(150);

  analogWrite(red_pin, 0);
  analogWrite(green_pin, 0);
  analogWrite(blue_pin, 0);

  Serial.begin(115200);

  randomSeed(analogRead(0));

  my_stool_num = digitalRead(addr_pin_2) == HIGH ? 4 : 0;
  my_stool_num += digitalRead(addr_pin_1) == HIGH ? 2 : 0;
  my_stool_num += digitalRead(addr_pin_0) == HIGH ? 1 : 0;

  ip = stools_ip[my_stool_num];

  sprintf_P(my_name, PSTR("stool%d"), my_stool_num);
  if (my_stool_num == master_stool_num)
    strcat_P(my_name, PSTR("m"));

  fader = new RGBFader(rgb_pins, blackred, 2, 128, 0, 4, 255, false, RGBFader::EXPONENTIAL);
  nextStepTicker.attach_ms(6, nextStep_wrapper);

  Serial.print(F("\n\rWelcome to Playstools!\n\r\n\r"
                 "I'm the stool "));
  Serial.print(my_stool_num);

  if (my_stool_num == master_stool_num)
    Serial.print(F(", I'm the master"));

  Serial.print(F("\n\rMy hostname is "));
  Serial.println(my_name);

  Serial.print(F("\n\rAccess point parameters:\n\r"
                 "  ssid:\t\t"));
  Serial.print(ssid);
  Serial.print(F("\n\r  password:\t"));
  Serial.print(password);
  Serial.print(F("\n\r  IP:\t\t"));
  ip.printTo(Serial);
  Serial.println("\n\r");

  if (my_stool_num == master_stool_num) {   
    Serial.print(F("Configuring access point... "));

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, ip, netmask);
    WiFi.softAP(ssid, password);
  
    Serial.print(F("Done!\n\rStarting TCP server on port "));
    Serial.print(tcp_port);
    Serial.print(F("..."));
  
    master::server.begin();
    //master::server.setNoDelay(true);  //Exception 2???

    Serial.println(F("Done!"));
  }
  else {
    slave::connect_to_server();
  }

  Serial.print("Configuring and starting OTA server... ");

  ArduinoOTA.setHostname(my_name);

  ArduinoOTA.onStart([]() {
    Serial.println("Starting OTA update");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\n\rOTA update has finished!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("OTA update authorization failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("OTA update begin failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("OTA update connect failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("OTA update receive failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("OTA update end failed"));
    else Serial.println(F("OTA unknown error"));
  });
  ArduinoOTA.begin();

  fader->setNextColors(blackred[0]);
  nextStepTicker.attach_ms(6, nextStep_wrapper);

  Serial.print(F("\nInitializing I2C and touch controller..."));
  
  Wire.begin(sda_pin, scl_pin);
  Wire.setClock(400000);
  
  if (touch.begin(mpr121_address)) {
    touch.writeRegister(MPR121_ECR, 0x00);  //Stop the device
    touch.setThresholds(4, 2);  //Increase the sensitivity
    touch.writeRegister(MPR121_ECR, 0x81);  //Restart the device with only the channel 0 active

    i2cPollTicker.attach_ms(i2cPoll_ms, i2cPoll);

    Serial.println(F(" Done!"));
  }
  else
    Serial.println(F(" Can't find touch controller (MPR121)!!!"));

  Serial.print(F("All done!\n\rI have "));
  Serial.print(ESP.getFreeHeap());
  Serial.println(F(" bytes free on the heap\n\r"));
}

void loop() {
  if (my_stool_num == master_stool_num) {
    master::update_clients_list();

    if (!master::running_animation)
      master::check_incoming_data();
    else
      master::flush_rx();
    
    if (touched) {
      touched = false;
      if (!master::running_animation)
        master::start_animation(master_stool_num);
    }
  }
  else {
    if (!slave::tcp_client.connected()) {
      fader->setNextColors(blackred[1]);
      nextStepTicker.attach_ms(6, nextStep_wrapper);
      slave::connect_to_server();
      fader->setNextColors(blackred[0]);
      nextStepTicker.attach_ms(6, nextStep_wrapper);
    }
    slave::check_incoming_data();

    if (touched && slave::tcp_client.connected()) {
      String out = F("{\"playstools\":{\"touched\":\"true\"}}");
      touched = false;
      slave::tcp_client.print(out);
      Serial.println(F("Sending touch informations"));
    }
  }

  ArduinoOTA.handle();
}

void i2cPoll() {
  static bool was_touched = false;
  const bool is_touched = touch.touched() & 0x0001;

  if (!was_touched && is_touched) //Rising edge
    touched = true;
  
  was_touched = is_touched;  
}

void execute_rgb_sequence(const RGB colors[], const uint8_t colorsNum, uint8_t step_time) {
  fader->setNextColors(colors, colorsNum);
  nextStepTicker.attach_ms(step_time, nextStep_wrapper);
}

void nextStep_wrapper() {
  fader->nextStep();
  if (fader->getColorEnded())
    nextStepTicker.detach();
}

