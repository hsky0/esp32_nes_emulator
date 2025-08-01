#ifndef PSXCONTROLLER_H
#define PSXCONTROLLER_H


#define CONFIG_HW_CONTROLLER_NES_PS     1
#define CONFIG_HW_NES_PS_DATA_PIN       27      // D-
#define CONFIG_HW_NES_PS_CLOCK_PIN      25      // DI
#define CONFIG_HW_NES_PS_LATCH_PIN      26      // D+


#define CONFIG_HW_USE_NES_FC            1

int psxReadInput();
void psxcontrollerInit();

#endif