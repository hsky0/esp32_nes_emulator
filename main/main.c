#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
// #include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "nofrendo.h"
#include "esp_partition.h"



char *osd_getromdata() {
	char* romdata;
	const esp_partition_t* part;
	// spi_flash_mmap_handle_t hrom;
	esp_partition_mmap_handle_t hrom;
	esp_err_t err;
	nvs_flash_init();
	// part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "rom");
	// part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0xFF, NULL);
	part=esp_partition_find_first(0x40, 0x01, NULL);

	// part = esp_partition_get(0x100000);
	if (part==0) 
	{
		printf("Couldn't find rom part!\n");
		return NULL;
	}

	err=esp_partition_mmap(part, 0, 3*1024*1024, ESP_PARTITION_MMAP_DATA, (const void**)&romdata, &hrom);
	if (err!=ESP_OK) 
	{
		printf("Couldn't map rom part!\n");
		return NULL;
	}
	printf("Initialized. ROM@%p\n", romdata);
	// esp_partition_munmap(hrom);
    return (char*)romdata;
}


esp_err_t event_handler(void *ctx, esp_event_base_t *event_base, int32_t event_id, void *event_data)
{
    return ESP_OK;
}

int app_main(void)
{
	printf("NoFrendo start!\n");
	nofrendo_main(0, NULL);
	printf("NoFrendo died? WtF?\n");
	asm("break.n 1");
    return 0;
}

