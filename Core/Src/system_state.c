#include "system_state.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "tim.h"

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

//Stopwatch
extern TIM_HandleTypeDef htim4;
volatile uint32_t stopwatchSec = 0;
static bool stopwatchRunning = false;
static uint32_t lastDisplayedSec = 0;


//MPU
char stepsStr[20];
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) {
        stopwatchSec++;  // increment every second
    }
}


//BUTTON

#define BTN_PORT GPIOB
#define BTN_PIN  GPIO_PIN_9
bool buttonPressed = false;
static uint32_t lastButtonTime = 0;
static GPIO_PinState lastButtonState = GPIO_PIN_SET;


void checkButton(void) {
    GPIO_PinState state = HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN);

    if (state == GPIO_PIN_RESET && lastButtonState == GPIO_PIN_SET) { // pressed
        uint32_t now = HAL_GetTick();
        if (now - lastButtonTime > 300) { // debounce 300ms
            lastButtonTime = now;
            buttonPressed = true;
        }
    }
    lastButtonState = state;
}

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

    const char enableGGA_RMC_VTG[] = "$PMTK314,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)enableGGA_RMC_VTG, strlen(enableGGA_RMC_VTG), HAL_MAX_DELAY);


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
    // --- BUTTON HANDLING ---
    checkButton();
    if (buttonPressed) {
        buttonPressed = false;

        if (stopwatchRunning) {
            // STOP: stop timer, go to idle
            HAL_TIM_Base_Stop_IT(&htim4);
            stopwatchRunning = false;

            // Write remaining GPS data
            gpsForceReadyBuffer();
            if (gpsHasReadyBuffer()) {
                currentState = STATE_SD_WRITE;
            }

            // --- WRITE SUMMARY TO FILE ---
            FRESULT res = f_open(&fil, "write.txt", FA_OPEN_ALWAYS | FA_WRITE);
            if (res == FR_OK) {
                f_lseek(&fil, fil.fsize);

                uint32_t s = stopwatchSec % 60;
                uint32_t m = (stopwatchSec / 60) % 60;
                uint32_t h = stopwatchSec / 3600;

                char summary[128];
                snprintf(summary, sizeof(summary),
                         "Session: %02lu:%02lu:%02lu, Steps: %lu\r\n\r\n",
                         h, m, s, StepDetector_GetCount());

                f_puts(summary, &fil);
                f_close(&fil);
            }

            StepDetector_Reset();
            currentState = STATE_IDLE;

            // show ready screen
            ST7735_FillScreenFast(ST7735_BLACK);
            ST7735_WriteString(0, 0, "Ready?", Font_11x18, ST7735_COLOR565(255, 0, 0), ST7735_BLACK);
        }
 else  if (!stopwatchRunning && currentState == STATE_IDLE ){
            // START: reset timer and start
            stopwatchSec = 0;
            HAL_TIM_Base_Start_IT(&htim4);
            stopwatchRunning = true;
            FRESULT res = f_open(&fil, "write.txt", FA_OPEN_ALWAYS | FA_WRITE);
            if (res == FR_OK) {
                f_lseek(&fil, fil.fsize);
                const char *date = gpsGetLastDate();
                const char *time = gpsGetLastTime();

                char header[128];
                snprintf(header, sizeof(header),
                         "\r\n==== New Session ====\r\nDate: %s\r\nTime: %s\r\n=====================\r\n",
                         date, time);

                f_puts(header, &fil);
                f_close(&fil);
            }
            currentState = STATE_RUNNING;
        }
    }

    // --- STATE MACHINE ---
    switch (currentState) {

    case STATE_INIT:
        if (MPU6050_DataReady() && !mpuOK) {
            mpuOK = true;
            ST7735_FillRectangle(0, 50 + 15, 11 * 8, 18, ST7735_BLACK);
            ST7735_WriteString(0, 50 + 15, "MPU: OK", Font_11x18, ST7735_WHITE, ST7735_BLACK);
        }

        if (gpsIsReady() && !gpsOK && gpsHasValidFix()) {
            gpsOK = true;
            ST7735_FillRectangle(0, 30 + 15, 11 * 8, 18, ST7735_BLACK);
            ST7735_WriteString(0, 30 + 15, "GPS: OK", Font_11x18, ST7735_WHITE, ST7735_BLACK);
        }

        if (gpsOK && sdOK && mpuOK) {
            currentState = STATE_IDLE;
            ST7735_FillScreenFast(ST7735_BLACK);
            ST7735_WriteString(0, 0, "Ready?", Font_11x18, ST7735_COLOR565(255, 0, 0), ST7735_BLACK);
        }
        break;

    case STATE_IDLE:
        // nothing else to do while idle
        break;

    case STATE_RUNNING:
        // update stopwatch display
        if (stopwatchSec != lastDisplayedSec) {
            lastDisplayedSec = stopwatchSec;
            char timeStr[16];
            uint32_t s = stopwatchSec % 60;
            uint32_t m = (stopwatchSec / 60) % 60;
            uint32_t h = stopwatchSec / 3600;
            snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu:%02lu", h, m, s);

            ST7735_FillRectangle(0, 0, 128, 18, ST7735_BLACK);
            ST7735_WriteString(0, 0, timeStr, Font_11x18, ST7735_WHITE, ST7735_BLACK);
        }

        // update steps
        snprintf(stepsStr, sizeof(stepsStr), "Steps: %lu", StepDetector_GetCount());
        ST7735_FillRectangle(0, 20, 128, 10, ST7735_BLACK);
        ST7735_WriteString(0, 20, stepsStr, Font_7x10, ST7735_WHITE, ST7735_BLACK);

        // sensor reads
        if (HAL_GetTick() - lastActionTime >= 10) {
            if (gpsHasReadyBuffer()) {
                currentState = STATE_SD_WRITE;
            } else if (MPU6050_DataReady()) {
                            currentState = STATE_MPU_READ;
                        }else {
                currentState = STATE_GPS_PROCESS;
            }
            lastActionTime = HAL_GetTick();
        }
        break;

    case STATE_MPU_READ:
        if (stopwatchRunning) {
            MPU6050_ProcessData(&MPU6050);
        }
        currentState = stopwatchRunning ? STATE_RUNNING : STATE_IDLE;
        break;

    case STATE_GPS_PROCESS:
        // non-blocking GPS processing
        currentState = stopwatchRunning ? STATE_RUNNING : STATE_IDLE;
        break;

    case STATE_SD_WRITE:
        if (stopwatchRunning) {
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
        }
        currentState = stopwatchRunning ? STATE_RUNNING : STATE_IDLE;
        break;

    case STATE_ERROR:
        HAL_Delay(500);
        break;
    }
}

