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
  namespace animations {

    template <typename T> void shuffle_array(T array[], uint8_t size) {
      uint8_t j;
      T temp;
      
      for (uint8_t i = 0; i < size; i++) {
          j = random(0, size);
          temp = array[j];
          array[j] = array[i];
          array[i] = temp;
        }
    }

    RGB random_color() {
      /*RGB color;

      do {
        color = RGB(random(0, 255), random(0, 255), random(0, 255));
      } while ((uint16_t)color.red + color.green + color.blue < 96);

      return color;*/

      return RGBFader::rainbowAndWhite[random(0, RGBFader::rainbowAndWhiteSize)];
    }

    uint16_t a0(uint8_t stools_array[], uint8_t connected_stools) {
      const uint8_t iterations_num = random(16, 24);
      uint16_t last = 0;
  
      colors[0] = random_color();
      colors[1] = RGB();
  
      for (uint8_t i = 0; i < iterations_num; i++) {

        color_info[i].colors = colors;

        if (i == iterations_num - 1)
          color_info[i].colors_num = 1;
        else
          color_info[i].colors_num = 2;

        color_info[i].step_time = 2;

        do {
          color_info[i].stool_num = stools_array[random(0, connected_stools)];
        } while (i != 0 && connected_stools > 1 && color_info[i].stool_num == color_info[i - 1].stool_num);
  
        if (i == 0)
          light_command(&(color_info[i]));  //Execute the first immediately         
        else {
          last += random(250, 500);
          timers[i - 1].once_ms(last, light_command, &(color_info[i]));
        }
      }

      color_info[iterations_num].colors = colors + 1;  //Black
      color_info[iterations_num].colors_num = 1;
      color_info[iterations_num].step_time = 2;
      color_info[iterations_num].stool_num = color_info[iterations_num - 1].stool_num;
      last += 5000;
      timers[iterations_num - 1].once_ms(last, light_command, &(color_info[iterations_num]));

      return last + 128 * color_info[iterations_num].step_time;
    }
  
    uint16_t a1(uint8_t stools_array[], uint8_t connected_stools) {
      uint8_t stools_array_off[stools_num];
      uint8_t i;
      uint16_t last = 0;
      
      memcpy(stools_array_off, stools_array, sizeof(uint8_t) * connected_stools);
      colors[0] = RGB(0, 255, 0);
      colors[1] = RGB(0, 0, 0);

      if (connected_stools > 1) {
        shuffle_array(stools_array, connected_stools);
        shuffle_array(stools_array_off, connected_stools);
      }

      for (i = 0; i < connected_stools; i++) {
        color_info[i].colors = colors;
        color_info[i].colors_num = 1;
        color_info[i].step_time = 8;
        color_info[i].stool_num = stools_array[i];
        
        if (i == 0)
          light_command(&(color_info[i]));  //Execute the first immediately         
        else {
          last += random(1000, 1750);
          timers[i - 1].once_ms(last, light_command, &(color_info[i]));
        }
      }

      last += 3000;

      for (i = 0; i < connected_stools; i++) {
        color_info[i + connected_stools].colors = colors + 1;
        color_info[i + connected_stools].colors_num = 1;
        color_info[i + connected_stools].step_time = 1;
        color_info[i + connected_stools].stool_num = stools_array_off[i];

        if (i == connected_stools - 1)
          last += 5000;
        else
          last += random(500, 1000);
        timers[i + connected_stools].once_ms(last, light_command, &(color_info[i + connected_stools]));
      }

      return last + 128 * color_info[2 * connected_stools - 1].step_time;
    }
  
    uint16_t a2(uint8_t stools_array[], uint8_t connected_stools) {
      const uint8_t iterations_num = random(3, 6);
      uint16_t last = 0;
      uint8_t group_size;
      uint8_t color_info_i = 0;

      for (uint8_t i = 0; i < iterations_num; i++) {
        shuffle_array(stools_array, connected_stools);

        colors[i * 3] = random_color();
        colors[i * 3 + 1] = colors[i * 3];
        colors[i * 3 + 2] = RGB();

        group_size = random(2, connected_stools + 1);

        for (uint8_t j = 0; j < group_size; j++) {
          color_info[color_info_i].colors = &(colors[i * 3]);
          color_info[color_info_i].colors_num = 3;
          color_info[color_info_i].step_time = 8;
          color_info[color_info_i].stool_num = stools_array[j];

          if (i == 0)
            light_command(&(color_info[color_info_i]));  //Execute the first group immediately         
          else {
            timers[color_info_i - 1].once_ms(last, light_command, &(color_info[color_info_i]));
          }
          ++color_info_i;
        }

        last += 2500;
      }

      colors[iterations_num * 3] = RGB(0, 255, 0);
      color_info[color_info_i].colors = &(colors[iterations_num * 3]);
      color_info[color_info_i].colors_num = 1;
      color_info[color_info_i].step_time = 8;
      color_info[color_info_i].stool_num = stools_array[random(0, connected_stools)];
      timers[color_info_i - 1].once_ms(last, light_command, &(color_info[color_info_i]));
      ++color_info_i;

      last += 5000;
      
      colors[iterations_num * 3 + 1] = RGB();
      color_info[color_info_i].colors = &(colors[iterations_num * 3 + 1]);
      color_info[color_info_i].colors_num = 1;
      color_info[color_info_i].step_time = 8;
      color_info[color_info_i].stool_num = color_info[color_info_i - 1].stool_num;
      timers[color_info_i - 1].once_ms(last, light_command, &(color_info[color_info_i]));

      return last + 128 * color_info[color_info_i].step_time;
    }
    
    uint16_t a3(uint8_t stools_array[], uint8_t connected_stools) {
      const uint8_t iterations_num = random(14, 21);
      const uint8_t start = random(0, connected_stools);
      const bool direction = random(0, 2);
      uint16_t last = 0;

      //BUG! Flickering if multiple LEDs are controlled at the same time, see https://github.com/esp8266/Arduino/issues/836
      //Using only RED, GREEN or BLUE mitigates this effect
      colors[0] = RGBFader::redGreenBlue[random(0, 3)];
      colors[1] = RGB();
      colors[2] = colors[0];  //Bug in the RGBFader library
      colors[3] = colors[0];

      for (uint8_t i = 0; i < iterations_num; i++) {
        color_info[i].colors = colors;
        color_info[i].colors_num = 2;
        color_info[i].step_time = 11;
        if (direction)
          color_info[i].stool_num = stools_array[(i + start) % connected_stools];
        else
          color_info[i].stool_num = stools_array[connected_stools - ((i + start) % connected_stools) - 1];

        if (i == 0)
          light_command(&(color_info[i]));  //Execute the first immediately         
        else
          timers[i - 1].once_ms(last, light_command, &(color_info[i]));

        last += 500;
      }

      color_info[iterations_num].colors = colors + 2;
      color_info[iterations_num].colors_num = 2;
      color_info[iterations_num].step_time = 8;
      color_info[iterations_num].stool_num = color_info[iterations_num - 1].stool_num;
      timers[iterations_num - 1].once_ms(last, light_command, &(color_info[iterations_num]));

      last += 5000;

      color_info[iterations_num + 1].colors = colors + 1;
      color_info[iterations_num + 1].colors_num = 1;
      color_info[iterations_num + 1].step_time = 8;
      color_info[iterations_num + 1].stool_num = color_info[iterations_num].stool_num;
      timers[iterations_num].once_ms(last, light_command, &(color_info[iterations_num + 1]));

      return last + 128 * color_info[iterations_num + 1].step_time;
    }

    uint16_t a4(uint8_t stools_array[], uint8_t connected_stools) {
      return a3(stools_array, connected_stools); //Not implemented
    }

    uint16_t a5(uint8_t stools_array[], uint8_t connected_stools) {
      return a3(stools_array, connected_stools); //Not implemented
    }

    uint16_t a6(uint8_t stools_array[], uint8_t connected_stools) {
      return a3(stools_array, connected_stools); //Not implemented
    }

    uint16_t a7(uint8_t stools_array[], uint8_t connected_stools) {
      return a3(stools_array, connected_stools); //Not implemented
    }

  }
}
