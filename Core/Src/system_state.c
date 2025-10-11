#include "system_state.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Globals for state management
static SystemState_t currentState = STATE_INIT;
static uint32_t lastActionTime = 0;
static FIL fil;
extern UART_HandleTypeDef huart1;
extern uint8_t rxByte;
extern FATFS fs;
static bool gpsOK = false;
static bool sdOK = false;
static bool mpuOK = false;

//GPS
char formattedBuffer[GPS_BUFFER_SIZE];



void systemState_Init(void) {
	//Delay for SD card
	  HAL_Delay(500);
    // Mount SD card
    if (f_mount(&fs, "", 0) != FR_OK) {
        currentState = STATE_ERROR;
        return;
    }
    sdOK = true;

	f_open(&fil, "write.txt", FA_OPEN_ALWAYS | FA_WRITE | FA_READ);
	f_lseek(&fil, fil.fsize);
	f_puts("HOLA PROFE", &fil);
	f_close(&fil);

	//SCREEN
	ST7735_Init();

    // Initialize sensors
    MPU6050_Initialization();

    // Start GPS interrupt reception
    HAL_UART_Receive_IT(&huart1, &rxByte, 1);

    //BOOT SCREEN
    ST7735_FillScreen(ST7735_BLACK);
    ST7735_WriteString(0, 0, "RUNNING", Font_11x18, ST7735_WHITE, ST7735_BLACK);
    ST7735_WriteString(0, 10 + 18 + 2, "COMPANION", Font_11x18, ST7735_WHITE, ST7735_BLACK);
    ST7735_WriteString(0, 30 + 18 + 2, "by emi barg", Font_11x18, ST7735_COLOR565(110,0,0), ST7735_BLACK);
    ST7735_WriteString(0, 50 + 18 + 2, "oct  2025", Font_7x10,ST7735_COLOR565(110,0,0) , ST7735_BLACK);

    currentState = STATE_INIT;
    lastActionTime = HAL_GetTick();

    //PREPARING SCREEN FOR INIT
    ST7735_FillScreenFast(ST7735_BLACK);
    ST7735_WriteString(0, 0, "System init:", Font_11x18,ST7735_WHITE , ST7735_BLACK);
    ST7735_WriteString(0, 10 + 15, "SD: ON", Font_11x18, ST7735_WHITE, ST7735_BLACK);
    ST7735_WriteString(0, 30 + 15, "GPS: OFF", Font_11x18, ST7735_WHITE, ST7735_BLACK);
    ST7735_WriteString(0, 50 + 15, "MPU: OFF", Font_11x18,ST7735_WHITE , ST7735_BLACK);

}


void systemState_Run(void) {
    switch (currentState) {
    case STATE_INIT:
    	//CHECKING IF ALL MODULES ARE CONNECTED and working

    	if (MPU6050_DataReady() == 1 && mpuOK == false) {
    	    mpuOK = true;

    	    // Clear the full area where the old text was drawn
    	    ST7735_FillRectangle(0, 50 + 15, 11 * 8, 18, ST7735_BLACK);
    	    // ^ Font_11x18 → width ≈ 11 px per char, height 18 px
    	    // If "MPU: NOT OK" was drawn before, that’s about 11 chars * 8 = 88 px wide

    	    // Now draw the new text
    	    ST7735_WriteString(0, 50 + 15, "MPU: OK", Font_11x18, ST7735_WHITE, ST7735_BLACK);
    	}

    	if (gpsIsReady() == true && gpsOK == false){
    		gpsOK =true;
    		 ST7735_FillRectangle(0, 30 + 15, 11 * 8, 18, ST7735_BLACK);
    		 ST7735_WriteString(0, 30 + 15, "GPS: OK", Font_11x18, ST7735_WHITE, ST7735_BLACK);

    	}
    	if(gpsOK && sdOK && mpuOK){
          currentState = STATE_IDLE;
          ST7735_FillScreenFast(ST7735_BLACK);

    	}
          break;
    case STATE_IDLE:

    	currentState= STATE_RUNNING;
    	break;

    case STATE_RUNNING:
        // Every few ms, decide next action
        if (HAL_GetTick() - lastActionTime >= 10) {
            if (MPU6050_DataReady()) {
                currentState = STATE_MPU_READ;
            }
            else if (gpsHasReadyBuffer()) {
                currentState = STATE_SD_WRITE;
            }
            else {
                currentState = STATE_GPS_PROCESS;
            }
            lastActionTime = HAL_GetTick();
        }
        break;

    case STATE_MPU_READ:
        MPU6050_ProcessData(&MPU6050);
        currentState = STATE_IDLE;
        break;

    case STATE_GPS_PROCESS:
        // Nothing blocking here — handled via interrupts
        currentState = STATE_IDLE;
        break;

    case STATE_SD_WRITE: {
        const char *buf = gpsGetReadyBuffer();
        if (buf) {
            char formattedBuffer[2048];
            gpsFormatBuffer(formattedBuffer, sizeof(formattedBuffer), buf);

            FRESULT res = f_open(&fil, "write.txt", FA_OPEN_ALWAYS | FA_WRITE);
            if (res == FR_OK) {
                f_lseek(&fil, fil.fsize);
                f_puts(formattedBuffer, &fil);
                f_close(&fil);
                gpsMarkBufferWritten();
            } else {
                currentState = STATE_ERROR;
            }
        }
        currentState = STATE_IDLE;
        break;
    }


    case STATE_ERROR:
        // SD or sensor error handling
        HAL_Delay(500);
        break;
    }
}
