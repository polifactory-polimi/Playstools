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

namespace master {
  
  void update_clients_list() {
    uint8_t i;
  
    for (i = 0; i < max_clients; i++) {
      if (clients[i] && !clients[i].connected()) {
        --connected_clients;
        Serial.print(F("Client "));
        Serial.print(clients_ip[i]);
        Serial.print(F(" has disconnected; "));
        Serial.print(connected_clients);
        Serial.println(F(" clients are currently connected"));
        
        clients[i].stop();
      }
    }
  
    while (server.hasClient()) {
      for (i = 0; i < max_clients; i++) {
        if (!clients[i]) {
          clients[i] = server.available();
          clients[i].setNoDelay(true);
          
          clients_ip[i] = clients[i].remoteIP().toString();
          ++connected_clients;
          Serial.print(F("New client: "));
          Serial.print(clients_ip[i]);
          Serial.print(F("; "));
          Serial.print(connected_clients);
          Serial.println(F(" clients are currently connected"));
          
          break;
        }
      }
  
      if (i == max_clients) {
        Serial.println(F("Maximum number of clients reached! Rejecting"));
  
        WiFiClient rejectedClient = server.available();
        rejectedClient.stop();
      }
    }
  }
    
  void check_incoming_data() {
    
    for (uint8_t i = 0; i < max_clients; i++) {
      if (clients[i] && clients[i].connected()){
        const uint16_t available_bytes = clients[i].available();
        char buf[master_buf_size];
        
        if (available_bytes) {
          const IPAddress client_ip = clients[i].remoteIP();
          uint8_t stool_num;

          clients[i].read((uint8_t *)buf, available_bytes);
          buf[available_bytes] = '\0';

          for (stool_num = 0; stool_num < stools_num; stool_num++)
            if (stools_ip[stool_num] == client_ip)
              break;
  
          if (stool_num < stools_num) {
            if (available_bytes < master_buf_size) {
              StaticJsonBuffer<128> jsonBuffer;
    
              JsonObject& root = jsonBuffer.parseObject(buf);
    
              if (root.success()) {
                if (root["playstools"]["touched"].as<bool>()) {
  
                  if (stool_num == 0) {
                    client_ip.printTo(Serial);
                    Serial.println(F(" transmitted something, but this is our IP (IP conflict?); ignoring"));
                    
                  } else {
                    Serial.print(F("Received data from stool "));
                    Serial.print(stool_num);
                    Serial.print(F(" ("));
                    client_ip.printTo(Serial);
                    Serial.println(')');
                    start_animation(stool_num);
                  }
                }
                else {
                  Serial.print(F("JSON from "));
                  client_ip.printTo(Serial);
                  Serial.println(F(" correctly received and parsed, but \"touched\" = \"false\"!"));
                }
              }
              else {
                Serial.print(F("JSON parsing failed!\n\rRemote IP: "));
                client_ip.printTo(Serial);
                Serial.println();
              }
            }
            else {
              Serial.print(F("Data from "));
              client_ip.printTo(Serial);
              Serial.print(F(" ("));
              Serial.print(available_bytes);
              Serial.print(F(" bytes) exceed the buffer size ("));
              Serial.print(master_buf_size);
              Serial.println(F(" bytes); discarding"));
    
              while (clients[i].available())
                clients[i].read();
            }
          }
          else {
            Serial.print(F("Data received from unknown IP ("));
            client_ip.printTo(Serial);
            Serial.println(F(") :"));
            Serial.println(buf);
          }
        }
      }
    }
  }
  
  void flush_rx() {
    for (uint8_t i = 0; i < max_clients; i++)
      if (clients[i] && clients[i].connected())
        while (clients[i].available())
          clients[i].read();
  }
  
  void light_command(Color_info *color_info) {
    if (color_info->stool_num == master_stool_num) {
      execute_rgb_sequence(color_info->colors, color_info->colors_num, color_info->step_time);
      
    } else if (color_info->stool_num < stools_num) {
      uint8_t i;
      StaticJsonBuffer<512> jsonBuffer;
  
      JsonObject& root = jsonBuffer.createObject();
      JsonObject& nestedObject = root.createNestedObject("playstools");
      JsonArray& nestedArray = nestedObject.createNestedArray("rgb");
  
      nestedObject["step_ms"] = color_info->step_time;
  
      for (i = 0; i < color_info->colors_num; i++) {
        JsonArray& color = nestedArray.createNestedArray();
        color.add(color_info->colors[i].red);
        color.add(color_info->colors[i].green);
        color.add(color_info->colors[i].blue);
      }
  
      for (i = 0; i < max_clients; i++) {
        IPAddress client_ip = clients[i].remoteIP();
  
        if (stools_ip[color_info->stool_num] == client_ip) {
          String temp;  //We need a buffered output
          root.printTo(temp);
          clients[i].print(temp);
          break;
        }
      }
  
      if (i >= max_clients) {
        Serial.print(F("Stool "));
        Serial.print(color_info->stool_num);
        Serial.print(F(" ("));
        stools_ip[color_info->stool_num].printTo(Serial);
        Serial.println(F(") is not in the client list."));
      }
    }
    else {
      Serial.print(F("Invalid stool num: "));
      Serial.println(color_info->stool_num);
    }
  }

  void start_animation(uint8_t animation_number) {    
    uint8_t stools_array[stools_num];
    uint8_t connected_stools = 0;
    uint16_t end_animation_ms;

    running_animation = true;
    Serial.print(F("Starting animation "));
    Serial.println(animation_number);
    Serial.print(F("Connected stools:"));

    for (uint8_t i = 0; i < stools_num; i++) {
      if (i == master_stool_num) {
        stools_array[connected_stools] = i;
        ++connected_stools;
        Serial.print(' ');
        Serial.print(i);
      }
      else {
        for (uint8_t j = 0; j < max_clients; j++) {
          if (clients[j] && clients[j].connected() && clients[j].remoteIP() == stools_ip[i]) {
            stools_array[connected_stools] = i;
            ++connected_stools;
            Serial.print(' ');
            Serial.print(i);
            break;
          }
        }
      }
    }
    Serial.println();

    end_animation_ms = animations::animations_jump_table[animation_number](stools_array, connected_stools);
    enable_animations.once_ms(end_animation_ms + 1000, enable_animations_wrapper);
  }

  void enable_animations_wrapper() {
    running_animation = false;
    Serial.println(F("Re-enabling animations"));
  }
  
}
