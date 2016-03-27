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

namespace slave {

  void wifi_connect() {
    unsigned long start;
    uint8_t retries = 0;
    
    Serial.print(F("Connecting to "));
    Serial.print(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.config(ip, stools_ip[master_stool_num], netmask);
    start = millis();

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print('.');

      if (millis() - start > 30000) {
        if (retries++ >= 3) {
          Serial.println(F("Restarting..."));
          delay(100);
          ESP.restart();
        }
        else {
          Serial.print(F("\n\rRetrying"));
          WiFi.disconnect();
          WiFi.mode(WIFI_STA);
          WiFi.begin(ssid, password);
          WiFi.config(ip, stools_ip[master_stool_num], netmask);
          start = millis();
        }
      }
    }
  
    Serial.print(F(" Done! IP: "));
    WiFi.localIP().printTo(Serial);
    Serial.println();
  }
  
  void connect_to_server() {
    while (!tcp_client.connected()) {
      if (WiFi.status() != WL_CONNECTED)
        wifi_connect();
      
      if (tcp_client.connect(stools_ip[0], tcp_port)) {
        Serial.print(F("TCP connection estabilished with "));
        stools_ip[0].printTo(Serial);
        Serial.print(':');
        Serial.println(tcp_port);
      }
      else {
        Serial.println(F("TCP connection failed"));
        delay(500);
      }
    }
  }
  
  void check_incoming_data() {
    const uint16_t available_bytes = tcp_client.available();
    char buf[slave_buf_size];
      
    if (available_bytes) {
      if (available_bytes < slave_buf_size) {
        StaticJsonBuffer<1024> jsonBuffer;
  
        tcp_client.read((uint8_t *)buf, available_bytes);
        buf[available_bytes] = '\0';
  
        JsonObject& root = jsonBuffer.parseObject(buf);
  
        if (root.success()) {
          uint8_t i = 0;
          
          for (auto value : root["playstools"]["rgb"].asArray()) {
            colors[i++] = RGB(value[0], value[1], value[2]);
            if (i >= max_colors)
              break;
          }
          colors_size = i;

          execute_rgb_sequence(colors, colors_size, root["playstools"]["step_ms"]);

          Serial.println(F("Received data correctly parsed"));
        }
        else {
          Serial.print(F("JSON parsing failed!\n\rRemote IP: "));
          tcp_client.remoteIP().printTo(Serial);
          Serial.println();
        }
      }
      else {
        Serial.print(F("Data from "));
        tcp_client.remoteIP().printTo(Serial);
        Serial.print(F(" ("));
        Serial.print(available_bytes);
        Serial.print(F(" bytes) exceed the buffer size ("));
        Serial.print(slave_buf_size);
        Serial.println(F(" bytes); discarding"));
        tcp_client.read();
      }
    }
  }
  
}
