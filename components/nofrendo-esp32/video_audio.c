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

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <freertos/queue.h>
//Nes stuff wants to define this as well...
#undef false
#undef true
#undef bool

// #define SOC_I2S_HW_VERSION_2 1

#include <math.h>
#include <string.h>
#include <noftypes.h>
#include <bitmap.h>
#include <nofconfig.h>
#include <event.h>
#include <gui.h>
#include <log.h>
#include <nes.h>
#include <nes_pal.h>
#include <nesinput.h>
#include <osd.h>
#include <stdint.h>
#include "driver/i2s_std.h"
#include "driver/i2s_pdm.h"
#include "sdkconfig.h"
#include <spi_lcd.h>

#include <psxcontroller.h>


#include "driver/dac_continuous.h"



#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "esp_task_wdt.h"

#define  DEFAULT_SAMPLERATE   22100
#define  DEFAULT_FRAGSIZE     128

#define  DEFAULT_WIDTH        256
#define  DEFAULT_HEIGHT       NES_VISIBLE_HEIGHT

#define AUDIO_SAMPLERATE 22050
#define AUDIO_BUFFER_LENGTH 64
#define BITS_PER_SAMPLE 16
// Always bits_per_sample / 8
#define BYTES_PER_SAMPLE 2
#define I2S_DEVICE_ID 0
#define PDM_TX_DOUT_IO 25

// adding audio
#define CONFIG_SOUND_ENA 1

TimerHandle_t timer;

//Seemingly, this will be called only once. Should call func with a freq of frequency,
int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize)
{
	printf("Timer install, freq=%d\n", frequency);
	timer=xTimerCreate("nes",configTICK_RATE_HZ/frequency, pdTRUE, NULL, func);
	xTimerStart(timer, 0);
   return 0;
}


/*
** Audio
*/
static int samplesPerPlayback = -1;
static void (*audio_callback)(void *buffer, int length) = NULL;
#if CONFIG_SOUND_ENA
QueueHandle_t queue;
static uint16_t *audio_frame;
static i2s_chan_handle_t dac_tx = NULL;
static dac_continuous_handle_t dac_handle = NULL;
#endif

static void do_audio_frame() {

#if CONFIG_SOUND_ENA
	// int left=DEFAULT_SAMPLERATE/NES_REFRESH_RATE;
	int left = AUDIO_SAMPLERATE /NES_REFRESH_RATE;
	while(left) {
		int n=DEFAULT_FRAGSIZE;
		if (n>left) n=left;
		audio_callback(audio_frame, n); //get more data
		//16 bit mono -> 32-bit (16 bit r+l)
		for (int i=n-1; i>=0; i--) {
			audio_frame[i*2+1]=audio_frame[i];
			audio_frame[i*2]=audio_frame[i];
		}
		size_t bytes_write = 0;
		i2s_channel_write(dac_tx, audio_frame, 4 * n, &bytes_write, portMAX_DELAY);
		left-=n;
	}

#endif
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
   //Indicates we should call playfunc() to get more data.
   audio_callback = playfunc;
}

static void osd_stopsound(void)
{
   audio_callback = NULL;
   printf("Sound stopped.\n");
   /*停止i2s*/

#if CONFIG_SOUND_ENA
	free(audio_frame);
#endif
}


static int osd_init_sound(void)
{
#if CONFIG_SOUND_ENA

	
	printf("Initializing sound\n");
	/*申请音频缓存*/
	// audio_frame=malloc(4*DEFAULT_FRAGSIZE);
	// audio_frame = malloc(BYTES_PER_SAMPLE * AUDIO_BUFFER_LENGTH);
	audio_frame = malloc(2048);
	/*创建TX通道*/
	i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
	ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &dac_tx, NULL));
	/*配置时钟，slot，gpio*/
	/*初始化标准模式*/

#if SOC_I2S_HW_VERSION_2	
	i2s_pdm_tx_clk_config_t clk_cfg = I2S_PDM_TX_CLK_DAC_DEFAULT_CONFIG(AUDIO_SAMPLERATE);

	i2s_pdm_tx_slot_config_t slot_cfg = I2S_PDM_TX_SLOT_DAC_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
#else 	
	i2s_pdm_tx_clk_config_t clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(AUDIO_SAMPLERATE);

	i2s_pdm_tx_slot_config_t slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
#endif

	i2s_pdm_tx_gpio_config_t gpio_cfg = {
		.clk = I2S_GPIO_UNUSED,
		.dout = PDM_TX_DOUT_IO,
		.invert_flags = {
			.clk_inv = false,
		}
	};

	i2s_pdm_tx_config_t pdm_cfg = {
		.clk_cfg = clk_cfg,
		.slot_cfg = slot_cfg,
		.gpio_cfg = gpio_cfg,
	};

	ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(dac_tx, &pdm_cfg));

	/*使能通道*/
	ESP_ERROR_CHECK(i2s_channel_enable(dac_tx));

	samplesPerPlayback = AUDIO_SAMPLERATE / NES_REFRESH_RATE;

	/*打印调试信息*/
	printf("Finish initializing sound\n");

#endif

	audio_callback = NULL;

	return 0;
}

void osd_getsoundinfo(sndinfo_t *info)
{
   info->sample_rate = DEFAULT_SAMPLERATE;
   info->bps = 16;
}

/*
** Video
*/

static int init(int width, int height);
static void shutdown(void);
static int set_mode(int width, int height);
static void set_palette(rgb_t *pal);
static void clear(uint8 color);
static bitmap_t *lock_write(void);
static void free_write(int num_dirties, rect_t *dirty_rects);
static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects);
static char fb[1]; //dummy

QueueHandle_t vidQueue;

viddriver_t sdlDriver =
{
   "Simple DirectMedia Layer",         /* name */
   init,          /* init */
   shutdown,      /* shutdown */
   set_mode,      /* set_mode */
   set_palette,   /* set_palette */
   clear,         /* clear */
   lock_write,    /* lock_write */
   free_write,    /* free_write */
   custom_blit,   /* custom_blit */
   false          /* invalidate flag */
};


bitmap_t *myBitmap;

void osd_getvideoinfo(vidinfo_t *info)
{
   info->default_width = DEFAULT_WIDTH;
   info->default_height = DEFAULT_HEIGHT;
   info->driver = &sdlDriver;
}

/* flip between full screen and windowed */
void osd_togglefullscreen(int code)
{
}

/* initialise video */
static int init(int width, int height)
{
	return 0;
}

static void shutdown(void)
{
}

/* set a video mode */
static int set_mode(int width, int height)
{
   return 0;
}

uint16 myPalette[256];

/* copy nes palette over to hardware */
static void set_palette(rgb_t *pal)
{
	uint16 c;

   int i;

   for (i = 0; i < 256; i++)
   {
      c=(pal[i].b>>3)+((pal[i].g>>2)<<5)+((pal[i].r>>3)<<11);
      //myPalette[i]=(c>>8)|((c&0xff)<<8);
      myPalette[i]=c;
   }

}

/* clear all frames to a particular color */
static void clear(uint8 color)
{
//   SDL_FillRect(mySurface, 0, color);
}



/* acquire the directbuffer for writing */
static bitmap_t *lock_write(void)
{
//   SDL_LockSurface(mySurface);
   myBitmap = bmp_createhw((uint8*)fb, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_WIDTH*2);
   return myBitmap;
}

/* release the resource */
static void free_write(int num_dirties, rect_t *dirty_rects)
{
   bmp_destroy(&myBitmap);
}


static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects) {
	xQueueSend(vidQueue, &bmp, 0);
	do_audio_frame();
}

// #define TIMG_WDT_WKEY_VALUE 0x50D83AA1
//This runs on core 1.
static void videoTask(void *arg) {
	int x, y;
	bitmap_t *bmp=NULL;
	x = (320-DEFAULT_WIDTH)/2;
    y = ((240-DEFAULT_HEIGHT)/2);
	
    while(1) {
//		xQueueReceive(vidQueue, &bmp, portMAX_DELAY);//skip one frame to drop to 30
		xQueueReceive(vidQueue, &bmp, portMAX_DELAY);
		ili9341_write_frame(x, y, DEFAULT_WIDTH, DEFAULT_HEIGHT, (const uint8_t **)bmp->line);
		// vTaskDelay(1);
	}

}


/*
** Input
*/

static void osd_initinput()
{
	psxcontrollerInit();
}

void osd_getinput(void)
{
	const int ev[16]={
			event_joypad1_select,0,0,event_joypad1_start,event_joypad1_up,event_joypad1_right,event_joypad1_down,event_joypad1_left,
			0,0,0,0,event_soft_reset,event_joypad1_a,event_joypad1_b,event_hard_reset
		};
	// const int ev[16] = {
	// 	event_joypad1_select, 
	// 	0, 
	// 	0, 
	// 	event_joypad1_start, 
	// 	event_joypad1_up, 
	// 	event_joypad1_right, 
	// 	event_joypad1_down, 
	// 	event_joypad1_left,
	// 	0, 
	// 	event_hard_reset, 
	// 	0, 
	// 	event_soft_reset, 
	// 	0, 
	// 	event_joypad1_a, 
	// 	event_joypad1_b, 
	// 	0};
	static int oldb=0xffff;
	int b=psxReadInput();
	int chg=b^oldb;
	int x;
	oldb=b;
	event_t evh;
	// printf("in osd Input: %x\n", b);
	for (x=0; x<16; x++) {
		if (chg&1) {
			evh=event_get(ev[x]);
			if (evh) evh((b&1)?INP_STATE_BREAK:INP_STATE_MAKE);
		}
		chg>>=1;
		b>>=1;
	}
}

static void osd_freeinput(void)
{
}

void osd_getmouse(int *x, int *y, int *button)
{
}

/*
** Shutdown
*/

/* this is at the bottom, to eliminate warnings */
void osd_shutdown()
{
	osd_stopsound();
	osd_freeinput();
}

static int logprint(const char *string)
{
   return printf("%s", string);
}

/*
** Startup
*/

int osd_init()
{
	log_chain_logfunc(logprint);

	if (osd_init_sound())
		return -1;

	ili9341_init();
	ili9341_write_frame(0,0,320,240,NULL);
	vidQueue=xQueueCreate(1, sizeof(bitmap_t *));
	xTaskCreatePinnedToCore(&videoTask, "videoTask", 2048, NULL, 5, NULL, 1);
	osd_initinput();
	return 0;
}
