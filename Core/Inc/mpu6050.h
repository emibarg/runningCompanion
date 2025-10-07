
#include "main.h"

#define MPU6050_ADDR 0xD0


#define MPU6050_SMPRT_DIV 0x19
#define MPU6050_WHO_AM_I 0x75
#define MPU6050_CONFIG 0x1A
#define MPU6050_GYRO_CONFIG 0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_INT_PIN_CFG 0x37
#define MPU6050_INT_ENABLE 0x38
#define MPU6050_INT_STATUS 0x3A
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_ACCEL_XOUT_L 0x3C
#define MPU6050_PWR_MGMT_1 0x6B //most important



#define MPU6050_INT_PORT 	GPIOB
#define MPU6050_INT_PIN 	GPIO_PIN_5

#define SCALE_10 10
#define SCALE_100 100
#define SCALE_1K 1000
#define  SCALE_10K 10000



typedef struct _MPU6050{
	int16_t acc_x_raw;
	int16_t acc_y_raw;
	int16_t acc_z_raw;
	int16_t temperature_raw;
	int16_t gyro_x_raw;
	int16_t gyro_y_raw;
	int16_t gyro_z_raw;

	int32_t acc_x;
	int32_t acc_y;
	int32_t acc_z;
	int32_t temperature;
	int32_t gyro_x;
	int32_t gyro_y;
	int32_t gyro_z;
}Struct_MPU6050;

extern Struct_MPU6050 MPU6050;

void MPU6050_Writebyte(uint8_t reg_addr, uint8_t val);
void MPU6050_Writebytes(uint8_t reg_addr, uint8_t len, uint8_t* data);
void MPU6050_Readbyte(uint8_t reg_addr, uint8_t* data);
void MPU6050_Readbytes(uint8_t reg_addr, uint8_t len, uint8_t* data);
void MPU6050_Initialization(void);
void MPU6050_Get6AxisRawData(Struct_MPU6050* mpu6050);
int MPU6050_DataReady(void);
void MPU6050_Get_LSB_Sensitivity(uint8_t FS_SCALE_GYRO, uint8_t FS_SCALE_ACC);
void MPU6050_DataConvert(Struct_MPU6050* mpu6050);
void MPU6050_ProcessData(Struct_MPU6050* mpu6050);
