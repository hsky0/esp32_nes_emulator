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

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"


#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "psxcontroller.h"
#include "sdkconfig.h"

#define PSX_CLK CONFIG_HW_PSX_CLK
#define PSX_DAT CONFIG_HW_PSX_DAT
#define PSX_ATT CONFIG_HW_PSX_ATT
#define PSX_CMD CONFIG_HW_PSX_CMD

#define DELAY() asm("nop; nop; nop; nop;nop; nop; nop; nop;nop; nop; nop; nop;nop; nop; nop; nop;")


#if CONFIG_HW_PSX_ENA

/* Sends and receives a byte from/to the PSX controller using SPI */
static int psxSendRecv(int send) {
	int x;
	int ret=0;
	volatile int delay;
	
#if 0
	while(1) {
		GPIO.out_w1ts=(1<<PSX_CMD);
		GPIO.out_w1ts=(1<<PSX_CLK);
		GPIO.out_w1tc=(1<<PSX_CMD);
		GPIO.out_w1tc=(1<<PSX_CLK);
	}
#endif

	GPIO.out_w1tc=(1<<PSX_ATT);
	for (delay=0; delay<100; delay++);
	for (x=0; x<8; x++) {
		if (send&1) {
			GPIO.out_w1ts=(1<<PSX_CMD);
		} else {
			GPIO.out_w1tc=(1<<PSX_CMD);
		}
		DELAY();
		for (delay=0; delay<100; delay++);
		GPIO.out_w1tc=(1<<PSX_CLK);
		for (delay=0; delay<100; delay++);
		GPIO.out_w1ts=(1<<PSX_CLK);
		ret>>=1;
		send>>=1;
		if (GPIO.in&(1<<PSX_DAT)) ret|=128;
	}
	return ret;
}

static void psxDone() {
	DELAY();
	GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, (1<<PSX_ATT));
}


int psxReadInput() {
	int b1, b2;

	psxSendRecv(0x01); //wake up
	psxSendRecv(0x42); //get data
	psxSendRecv(0xff); //should return 0x5a
	b1=psxSendRecv(0xff); //buttons byte 1
	b2=psxSendRecv(0xff); //buttons byte 2
	psxDone();
	return (b2<<8)|b1;

}


void psxcontrollerInit() {
	volatile int delay;
	int t;
	gpio_config_t gpioconf[2]={
		{
			.pin_bit_mask=(1<<PSX_CLK)|(1<<PSX_CMD)|(1<<PSX_ATT), 
			.mode=GPIO_MODE_OUTPUT, 
			.pull_up_en=GPIO_PULLUP_DISABLE, 
			.pull_down_en=GPIO_PULLDOWN_DISABLE, 
			.intr_type=GPIO_PIN_INTR_DISABLE
		},{
			.pin_bit_mask=(1<<PSX_DAT), 
			.mode=GPIO_MODE_INPUT, 
			.pull_up_en=GPIO_PULLUP_ENABLE, 
			.pull_down_en=GPIO_PULLDOWN_DISABLE, 
			.intr_type=GPIO_PIN_INTR_DISABLE
		}
	};
	gpio_config(&gpioconf[0]);
	gpio_config(&gpioconf[1]);
	
	//Send a few dummy bytes to clean the pipes.
	psxSendRecv(0);
	psxDone();
	for (delay=0; delay<500; delay++) DELAY();
	psxSendRecv(0);
	psxDone();
	for (delay=0; delay<500; delay++) DELAY();
	//Try and detect the type of controller, so we can give the user some diagnostics.
	psxSendRecv(0x01);
	t=psxSendRecv(0x00);
	psxDone();
	if (t==0 || t==0xff) {
		printf("No PSX/PS2 controller detected (0x%X). You will not be able to control the game.\n", t);
	} else {
		printf("PSX controller type 0x%X\n", t);
	}
}


#elif CONFIG_HW_USE_NES_FC 


#define PSX_SELECT 1
#define PSX_START (1 << 3)
#define PSX_UP (1 << 4)
#define PSX_RIGHT (1 << 5)
#define PSX_DOWN (1 << 6)
#define PSX_LEFT (1 << 7)
#define PSX_L2 (1 << 8)
#define PSX_R2 (1 << 9)
#define PSX_L1 (1 << 10)
#define PSX_R1 (1 << 11)
#define PSX_TRIANGLE (1 << 12)
#define PSX_CIRCLE (1 << 13)
#define PSX_X (1 << 14)
#define PSX_SQUARE (1 << 15)
#define A_BUTTON PSX_CIRCLE
#define B_BUTTON PSX_X
#define TURBO_A_BUTTON PSX_TRIANGLE
#define TURBO_B_BUTTON PSX_SQUARE
#define MENU_BUTTON PSX_L1
#define POWER_BUTTON PSX_R1

int inpDelay;
bool showMenu;

int turboACounter = 0;
int turboBCounter = 0;
int turboASpeed = 3;
int turboBSpeed = 3;
int MAX_TURBO = 6;
int TURBO_COUNTER_RESET = 210;

bool nes_100ask_is_turbo_a_pressed(int ctl)
{
	return !(ctl & TURBO_A_BUTTON);
}
bool nes_100ask_is_turbo_b_pressed(int ctl)
{
	return !(ctl & TURBO_B_BUTTON);
}


void nes_100ask_joypad_check_delay(uint16_t t)
{
    while(t--);
}

/*
    A: 		0000 0001 1     (1 << 0)
    B: 		0000 0010 2     (1 << 1)
    start: 	0000 0100 8     (1 << 2)
    select:	0000 1000 4     (1 << 3)
    Up:		0001 0000 16    (1 << 4)
    Down:   0010 0000 32    (1 << 5)
    Left:	0100 0000 64    (1 << 6)
    Right:  1000 0000 128   (1 << 7)
*/
// A、B、START、SELECT、UP、DOWN、LEFT、RIGHT
int sfc_ps_button_info[] = {(1 << 13), (1 << 14), (1), (1 << 3), (1 << 4), (1 << 6), (1 << 7), (1 << 5)};

typedef struct {    
    char prev_player_value;
    char player;
    bool joypad_state;
} fc_joypad_t;


static fc_joypad_t g_fc_joypad;

int psxReadInput() {

	int b2b1 = 65535;
	if (inpDelay > 0){
		inpDelay--;
	}
	
	int8_t tmp_value = g_fc_joypad.player;


	gpio_set_level(CONFIG_HW_NES_PS_LATCH_PIN, 1);
    nes_100ask_joypad_check_delay(400);
	// vTaskDelay(pdMS_TO_TICKS(5));				// 5ms
	vTaskDelay(pdMS_TO_TICKS(5));				// 5ms
    gpio_set_level(CONFIG_HW_NES_PS_LATCH_PIN, 0);
	// vTaskDelay(pdMS_TO_TICKS(5));				// 5ms

	g_fc_joypad.joypad_state = false;
	g_fc_joypad.player = 0;
    for(int i = 0; i < 8; i++)
    {
        if(gpio_get_level(CONFIG_HW_NES_PS_DATA_PIN) == 0)
		{
    	    b2b1 -= sfc_ps_button_info[i];
			// if (!(g_fc_joypad.joypad_state))
			// {
    	    //     b2b1 -= sfc_ps_button_info[i];
			// }
			// g_fc_joypad.joypad_state = true;
		}
        gpio_set_level(CONFIG_HW_NES_PS_CLOCK_PIN, 1);
		// vTaskDelay(pdMS_TO_TICKS(5));				// 5ms
        nes_100ask_joypad_check_delay(400);
		// DELAY();
        gpio_set_level(CONFIG_HW_NES_PS_CLOCK_PIN, 0);
		// vTaskDelay(pdMS_TO_TICKS(5));				// 5ms
        nes_100ask_joypad_check_delay(400);
		// DELAY();
    }
	// printf("in psx b2b1 = %x\n", b2b1);

	

	if (!showMenu)
	{
		if (turboASpeed > 0 && nes_100ask_is_turbo_a_pressed(b2b1))
		{
			b2b1 |= A_BUTTON;
			if ((turboACounter % (turboASpeed*2)) == 0)
			{
				b2b1 -= A_BUTTON;
			}
			turboACounter = (turboACounter + 1) % TURBO_COUNTER_RESET; // 30 is the LCM of numers 1 thru 6
		}
		else
		{
			turboACounter = 0;
		}

		if (turboBSpeed > 0 && nes_100ask_is_turbo_b_pressed(b2b1))
		{
			b2b1 |= B_BUTTON;
			if ((turboBCounter % (turboBSpeed*2)) == 0)
			{
				b2b1 -= B_BUTTON;
			}
			turboBCounter = (turboBCounter + 1) % TURBO_COUNTER_RESET; // 30 is the LCM of numers 1 thru 6
		}
		else
		{
			turboBCounter = 0;
		}
	}

	// printf("b2b1 = %x\n", b2b1);
	return b2b1;

}


void psxcontrollerInit() {

	printf("Initial nes fc controler\n");
	/* 定义一个gpio配置结构体 */
    // gpio_config_t gpio_config_structure;

    // /* 初始化gpio配置结构体(CONFIG_HW_NES_PS_CLOCK_PIN) */
    // gpio_config_structure.pin_bit_mask = (1ULL << CONFIG_HW_NES_PS_CLOCK_PIN);/* 选择 CONFIG_HW_NES_PS_CLOCK_PIN */
    // gpio_config_structure.mode = GPIO_MODE_OUTPUT;              /* 输出模式 */
    // gpio_config_structure.pull_up_en = 0;                       /* 不上拉 */
    // gpio_config_structure.pull_down_en = 0;                     /* 不下拉 */
    // gpio_config_structure.intr_type = GPIO_INTR_DISABLE;    /* 禁止中断 */ 

    // /* 根据设定参数初始化并使能 */  
	// gpio_config(&gpio_config_structure);

	// /* 初始化gpio配置结构体() */
    // gpio_config_structure.pin_bit_mask = (1ULL << CONFIG_HW_NES_PS_LATCH_PIN);/* 选择 CONFIG_HW_NES_PS_LATCH_PIN */
    // gpio_config_structure.mode = GPIO_MODE_OUTPUT;              /* 输出模式 */
    // gpio_config_structure.pull_up_en = 0;                       /* 不上拉 */
    // gpio_config_structure.pull_down_en = 0;                     /* 不下拉 */
    // gpio_config_structure.intr_type = GPIO_INTR_DISABLE;    /* 禁止中断 */ 

    // /* 根据设定参数初始化并使能 */  
	// gpio_config(&gpio_config_structure);

    // /* 初始化gpio配置结构体(CONFIG_HW_NES_PS_DATA_PIN) */
    // gpio_config_structure.pin_bit_mask = (1ULL << CONFIG_HW_NES_PS_DATA_PIN);/* 选择 CONFIG_HW_NES_PS_DATA_PIN */
    // gpio_config_structure.mode = GPIO_MODE_INPUT;               /* 输入模式 */
    // gpio_config_structure.pull_up_en = 0;                       /* 不上拉 */
    // gpio_config_structure.pull_down_en = 0;                     /* 不下拉 */
    // gpio_config_structure.intr_type = GPIO_INTR_DISABLE;    /* 禁止中断 */ 

    /* 根据设定参数初始化并使能 */  
	// gpio_config(&gpio_config_structure);

	gpio_reset_pin(CONFIG_HW_NES_PS_CLOCK_PIN);
    gpio_reset_pin(CONFIG_HW_NES_PS_LATCH_PIN);
    gpio_reset_pin(CONFIG_HW_NES_PS_DATA_PIN);

    /* Set the GPIO as a push/pull output */
    gpio_set_direction(CONFIG_HW_NES_PS_CLOCK_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_HW_NES_PS_LATCH_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_HW_NES_PS_DATA_PIN,  GPIO_MODE_INPUT);

	printf("Initialal fc successful\n");

	inpDelay = 0;
	showMenu = false;

}

#else

int psxReadInput() {
	return 0xFFFF;
}


void psxcontrollerInit() {
	printf("PSX controller disabled in menuconfig; no input enabled.\n");
}

#endif