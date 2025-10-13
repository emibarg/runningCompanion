#include "MPU6050.h"
#include "main.h"
#include "i2c.h"

Struct_MPU6050 MPU6050;

static int32_t LSB_Sensitivity_ACC;
static int32_t LSB_Sensitivity_GYRO;

void MPU6050_Writebyte(uint8_t reg_addr, uint8_t val)
{
	HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, reg_addr, I2C_MEMADD_SIZE_8BIT, &val, 1, 1000);
}

void MPU6050_Writebytes(uint8_t reg_addr, uint8_t len, uint8_t* data)
{
	HAL_I2C_Mem_Write(&hi2c2, MPU6050_ADDR, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, 1000);
}

void MPU6050_Readbyte(uint8_t reg_addr, uint8_t* data)
{
	HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, reg_addr, I2C_MEMADD_SIZE_8BIT, data, 1, 1000);
}

void MPU6050_Readbytes(uint8_t reg_addr, uint8_t len, uint8_t* data)
{
	HAL_I2C_Mem_Read(&hi2c2, MPU6050_ADDR, reg_addr, I2C_MEMADD_SIZE_8BIT, data, len, 1000);
}

void MPU6050_Initialization(void)
{
	HAL_Delay(50);
	uint8_t who_am_i = 0;
	//printf("Checking MPU6050...\n");

	MPU6050_Readbyte(MPU6050_WHO_AM_I, &who_am_i);
	if(who_am_i == 0x68)
	{
		//printf("MPU6050 who_am_i = 0x%02x...OK\n", who_am_i);
	}
	else
	{
		//printf("ERROR!\n");
		//printf("MPU6050 who_am_i : 0x%02x should be 0x68\n", who_am_i);
		while(1)
		{
			//printf("who am i error. Can not recognize mpu6050\n");
			HAL_Delay(100);
		}
	}

	//Reset the whole module before initialization
	MPU6050_Writebyte(MPU6050_PWR_MGMT_1, 0x1<<7);
	HAL_Delay(100);

	//Power Management setting
	/* Default is sleep mode
	 * necessary to wake up MPU6050*/
	MPU6050_Writebyte(MPU6050_PWR_MGMT_1, 0x00);
	HAL_Delay(50);

	//Sample rate divider
	/*Sample Rate = Gyroscope Output Rate / (1 + SMPRT_DIV) */
	//	MPU6050_Writebyte(MPU6050_SMPRT_DIV, 0x00); // ACC output rate is 1kHz, GYRO output rate is 8kHz
	MPU6050_Writebyte(MPU6050_SMPRT_DIV, 39); // Sample Rate = 200Hz
	HAL_Delay(50);

	//FSYNC and DLPF setting
	/*DLPF is set to 0*/
	MPU6050_Writebyte(MPU6050_CONFIG, 0x00);
	HAL_Delay(50);

	//GYRO FULL SCALE setting
	/*FS_SEL  Full Scale Range
	  0    	+-250 degree/s
	  1		+-500 degree/s
	  2		+-1000 degree/s
	  3		+-2000 degree/s	*/
	uint8_t FS_SCALE_GYRO = 0x0;
	MPU6050_Writebyte(MPU6050_GYRO_CONFIG, FS_SCALE_GYRO<<3);
	HAL_Delay(50);

	//ACCEL FULL SCALE setting
	/*FS_SEL  Full Scale Range
	  0    	+-2g
	  1		+-4g
	  2		+-8g
	  3		+-16g	*/
	uint8_t FS_SCALE_ACC = 0x0;
	MPU6050_Writebyte(MPU6050_ACCEL_CONFIG, FS_SCALE_ACC<<3);
	HAL_Delay(50);

	MPU6050_Get_LSB_Sensitivity(FS_SCALE_GYRO, FS_SCALE_ACC);
	//printf("LSB_Sensitivity_GYRO: %f, LSB_Sensitivity_ACC: %f\n",LSB_Sensitivity_GYRO, LSB_Sensitivity_ACC);

	//Interrupt PIN setting
	uint8_t INT_LEVEL = 0x0; //0 - active high, 1 - active low
	uint8_t LATCH_INT_EN = 0x0; //0 - INT 50us pulse, 1 - interrupt clear required
	uint8_t INT_RD_CLEAR = 0x0; //0 - INT flag cleared by reading INT_STATUS, 1 - INT flag cleared by any read operation
	MPU6050_Writebyte(MPU6050_INT_PIN_CFG, (INT_LEVEL<<7)|(LATCH_INT_EN<<5)|(INT_RD_CLEAR<<4)); //
	HAL_Delay(50);

	//Interrupt enable setting
	uint8_t DATA_RDY_EN = 0x1; // 1 - enable, 0 - disable
	MPU6050_Writebyte(MPU6050_INT_ENABLE, DATA_RDY_EN);
	HAL_Delay(50);

	//printf("MPU6050 setting is finished\n");
}
/*Get Raw Data from sensor*/
void MPU6050_Get6AxisRawData(Struct_MPU6050* mpu6050)
{
	uint8_t data[14];
	MPU6050_Readbytes(MPU6050_ACCEL_XOUT_H, 14, data);

	mpu6050->acc_x_raw = (data[0] << 8) | data[1];
	mpu6050->acc_y_raw = (data[2] << 8) | data[3];
	mpu6050->acc_z_raw = (data[4] << 8) | data[5];

	mpu6050->temperature_raw = (data[6] << 8) | data[7];

	mpu6050->gyro_x_raw = ((data[8] << 8) | data[9]);
	mpu6050->gyro_y_raw = ((data[10] << 8) | data[11]);
	mpu6050->gyro_z_raw = ((data[12] << 8) | data[13]);
}

void MPU6050_Get_LSB_Sensitivity(uint8_t FS_SCALE_GYRO, uint8_t FS_SCALE_ACC)
{
	switch(FS_SCALE_GYRO)
	{
	case 0:
		LSB_Sensitivity_GYRO = 131*SCALE_10;
		break;
	case 1:
		LSB_Sensitivity_GYRO = 655;//10 porque era 65.5
		break;
	case 2:
		LSB_Sensitivity_GYRO = 328;//10 porque era 65.5
		break;
	case 3:
		LSB_Sensitivity_GYRO = 164*SCALE_10; //PENIS
		break;
	}
	switch(FS_SCALE_ACC)
	{
	case 0:
		LSB_Sensitivity_ACC = 16384;
		break;
	case 1:
		LSB_Sensitivity_ACC = 8192;
		break;
	case 2:
		LSB_Sensitivity_ACC = 4096;
		break;
	case 3:
		LSB_Sensitivity_ACC = 2048;
		break;
	}
}

/*Convert Unit. acc_raw -> g, gyro_raw -> degree per second*/
void MPU6050_DataConvert(Struct_MPU6050* mpu6050)
{
	//printf("LSB_Sensitivity_GYRO: %f, LSB_Sensitivity_ACC: %f\n",LSB_Sensitivity_GYRO,LSB_Sensitivity_ACC);
	mpu6050->acc_x = mpu6050->acc_x_raw *SCALE_1K/ LSB_Sensitivity_ACC;
	mpu6050->acc_y = mpu6050->acc_y_raw  *SCALE_1K / LSB_Sensitivity_ACC;
	mpu6050->acc_z = mpu6050->acc_z_raw *SCALE_1K/ LSB_Sensitivity_ACC;

	mpu6050->temperature = ((mpu6050->temperature_raw) *SCALE_1K)/340+36530;

	mpu6050->gyro_x = mpu6050->gyro_x_raw  *SCALE_10K/ LSB_Sensitivity_GYRO; // ACA ES * 10K PORQUE LA SENSIBILIDAD VIENE EN FACTOR x10
	mpu6050->gyro_y = mpu6050->gyro_y_raw *SCALE_10K / LSB_Sensitivity_GYRO;
	mpu6050->gyro_z = mpu6050->gyro_z_raw  *SCALE_10K/ LSB_Sensitivity_GYRO;
}


int MPU6050_DataReady(void)
{
    uint8_t status;
    MPU6050_Readbyte(MPU6050_INT_STATUS, &status);
    return (status & 0x01);
}


void MPU6050_ProcessData(Struct_MPU6050* mpu6050)
{
	MPU6050_Get6AxisRawData(mpu6050);
	MPU6050_DataConvert(mpu6050);
	StepDetector_Update(mpu6050);
}



// ====================== STEP DETECTION ====================== //

static uint32_t stepCount = 0;
static uint32_t lastStepTime = 0;

// moving gravity baseline (squared)
static int64_t gravitySq = 0;

// step detection parameters (tweak as needed)
#define STEP_THRESHOLD_DELTA  (700LL * 700LL)  // sensitivity
#define STEP_MIN_INTERVAL_MS  250               // minimum time between steps
#define GRAVITY_LPF_ALPHA     100               // baseline smoothing factor

void StepDetector_Update(const Struct_MPU6050 *mpu)
{
    int32_t ax = mpu->acc_x;
    int32_t ay = mpu->acc_y;
    int32_t az = mpu->acc_z;

    // magnitude squared (orientation-independent)
    int64_t magSq = (int64_t)ax * ax + (int64_t)ay * ay + (int64_t)az * az;

    // initialize gravity baseline at first call
    if (gravitySq == 0) {
        gravitySq = magSq;
        return;
    }

    // compute deviation from baseline
    int64_t delta = magSq > gravitySq ? magSq - gravitySq : gravitySq - magSq;

    // step detection
    uint32_t now = HAL_GetTick();
    if (delta > STEP_THRESHOLD_DELTA && (now - lastStepTime) > STEP_MIN_INTERVAL_MS) {
        stepCount++;
        lastStepTime = now;
    }

    // update gravity baseline (simple low-pass filter)
    gravitySq = (gravitySq * (GRAVITY_LPF_ALPHA - 1) + magSq) / GRAVITY_LPF_ALPHA;
}

uint32_t StepDetector_GetCount(void)
{
    return stepCount;
}

void StepDetector_Reset(void)
{
    stepCount = 0;
    lastStepTime = 0;
    gravitySq = 0;
}
