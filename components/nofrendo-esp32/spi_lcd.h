// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _DRIVER_SPI_LCD_H_
#define _DRIVER_SPI_LCD_H_
#include <stdint.h>

//*****************************************************************************
//
// Make sure all of the definitions in this header have a C binding.
//
//*****************************************************************************

#ifdef __cplusplus
extern "C"
{
#endif


// #define PIN_NUM_MISO 0 
// #define PIN_NUM_MOSI 12 
// #define PIN_NUM_CLK  13 
// #define PIN_NUM_CS   27 
// #define PIN_NUM_DC   25 
// #define PIN_NUM_RST  26 
// #define PIN_NUM_BCKL 14 


#define PIN_NUM_MISO 0 
#define PIN_NUM_MOSI 23 
#define PIN_NUM_CLK  19 
#define PIN_NUM_CS   22 
#define PIN_NUM_DC   21 
#define PIN_NUM_RST  18 
#define PIN_NUM_BCKL 5 

void ili9341_write_frame(const uint16_t x, const uint16_t y, const uint16_t width, const uint16_t height, const uint8_t *data[]);
void ili9341_init();


#ifdef __cplusplus
}
#endif

#endif //  __SPI_H__
