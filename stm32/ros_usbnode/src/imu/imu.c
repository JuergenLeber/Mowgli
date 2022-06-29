
/**
  ******************************************************************************
  * @file    imu.c
  * @author  Georg Swoboda <cn@warp.at>
  * @brief   Mowgli IMU calibration routines as long as they are IMU independent 
  ******************************************************************************
  * @attention
  *
  * details: https://learn.adafruit.com/adafruit-sensorlab-magnetometer-calibration?view=all
  *          https://makersportal.com/blog/calibration-of-a-magnetometer-with-raspberry-pi
  ******************************************************************************
  */

#include <math.h>

#include "imu/imu.h"
#include "i2c.h"
#include "main.h"

/* accelerometer calibration values */
float imu_cal_ax = 0.0;
float imu_cal_ay = 0.0;
float imu_cal_az = 0.0;
/* gyro calibration values */
float imu_cal_gx = 0.0;
float imu_cal_gy = 0.0;
float imu_cal_gz = 0.0;
/* onboard accelerometer calibration values */
float onboard_imu_cal_ax = 0.0;
float onboard_imu_cal_ay = 0.0;
float onboard_imu_cal_az = 0.0;

/* covariance matrixes for IMU data */
float imu_cov_ax = 0.01;
float imu_cov_ay = 0.01;
float imu_cov_az = 0.01;
// ---------------------
float imu_cov_gx = 0.01;
float imu_cov_gy = 0.01;
float imu_cov_gz = 0.01;
// ---------------------
float onboard_imu_cov_ax = 0.01;
float onboard_imu_cov_ay = 0.01;
float onboard_imu_cov_az = 0.01;

/**
  * @brief  Reads the 3 magnetometer channels and stores them in *x,*y,*z  
  * 
  * units are tesla uncalibrated
  */ 
void IMU_ReadMagnetometer(float *x, float *y, float *z)
{  
    float imu_x, imu_y, imu_z;    
    IMU_ReadMagnetometerRaw(&imu_x, &imu_y, &imu_z);
    IMUApplyMagTransformation(imu_x, imu_y, imu_z, x, y, z);
}

/**
  * @brief  Reads the 3 accelerometer axis and stores them in *x,*y,*z  
  * 
  * units are ms^2 uncalibrated
  */ 
void IMU_ReadAccelerometer(float *x, float *y, float *z)
{
    float imu_x, imu_y, imu_z;
    IMU_ReadAccelerometerRaw(&imu_x, &imu_y, &imu_z);
    // apply calibration
    *x = imu_x - imu_cal_ax;
    *y = imu_y - imu_cal_ay;
    *z = imu_z - imu_cal_az;
}

/*
 * Set covariance matrix values
 */
void IMU_AccelerometerSetCovariance(float *cm)
{
   cm[0] = imu_cov_ax;
   cm[4] = imu_cov_ay;
   cm[8] = imu_cov_az;
}


/**
  * @brief  Reads the 3 accelerometer gyro and stores them in *x,*y,*z  
  * 
  * units are rad/sec uncalibrated
  */ 
void IMU_ReadGyro(float *x, float *y, float *z)
{
    float imu_x, imu_y, imu_z;
    IMU_ReadGyroRaw(&imu_x, &imu_y, &imu_z);
    // apply calibration
    *x = imu_x - imu_cal_gx;
    *y = imu_y - imu_cal_gy;
    *z = imu_z - imu_cal_gz;
}

/*
 * Set covariance matrix values
 */
void IMU_GyroSetCovariance(float *cm)
{
   cm[0] = imu_cov_gx;
   cm[4] = imu_cov_gy;
   cm[8] = imu_cov_gz;
}

/*
 * Read onboard IMU acceleration in ms^2
 */
void IMU_Onboard_ReadAccelerometer(float *x, float *y, float *z)
{
   I2C_ReadAccelerometer(x, y, z);
}

/*
 * Set covariance matrix values
 */
void IMU_Onboard_AccelerometerSetCovariance(float *cm)
{
   cm[0] = onboard_imu_cov_ax;
   cm[4] = onboard_imu_cov_ay;
   cm[8] = onboard_imu_cov_az;
}

/*
 * Read onboard IMU temperature in °C
 */
float IMU_Onboard_ReadTemp(void)
{
  return(I2C_ReadAccelerometerTemp());
}


/**
  * @brief Calibrates IMU accelerometers and gyro by averaging and storing those values as calibration factors 
  * it expects that the bot is leveled and not moving
  * 
  * magnetometer is not calibrated here, but uses the madgwick filter with bias values
  * 
  */ 
void IMU_Calibrate()
{
    float imu_sample_x[IMU_CAL_SAMPLES], imu_sample_y[IMU_CAL_SAMPLES], imu_sample_z[IMU_CAL_SAMPLES]; 
    float stddev_x, stddev_y, stddev_z;
    float mean_x, mean_y, mean_z;
    //float imu_x, imu_y, imu_z;
    float sum_x, sum_y, sum_z;
    uint16_t i;
    //uint16_t samples = 100; // 100 samples every 10ms x3 = 3sec calibration time
    
    debug_printf("   >> IMU Calibration started - make sure bot is level and standing still ...\r\n");    
    
    /***********************************/
    /* calibrate external accelerometer */
    /***********************************/
    sum_x = sum_y = sum_z = 0;
    for (i=0; i<IMU_CAL_SAMPLES; i++)
    {
      IMU_ReadAccelerometerRaw(&imu_sample_x[i], &imu_sample_y[i], &imu_sample_z[i]);
        sum_x += imu_sample_x[i];
      sum_y += imu_sample_y[i];
      sum_z += imu_sample_z[i];
      HAL_Delay(10);      
    }
    mean_x = sum_x / IMU_CAL_SAMPLES;
    mean_y = sum_y / IMU_CAL_SAMPLES;
    mean_z = sum_z / IMU_CAL_SAMPLES;
    imu_cal_ax = mean_x;
    imu_cal_ay = mean_y;
    imu_cal_az = 0;    // we dont want to calibrate Z because our IMU Sensor fusion stack expects gravity
    debug_printf("   >> External IMU Calibration factors accelerometer [%f %f %f]\r\n", imu_cal_ax, imu_cal_ay, imu_cal_az);
    stddev_x = stddev_y = stddev_z = 0;
    for (i=0; i<IMU_CAL_SAMPLES; i++)
    {
        stddev_x += pow(imu_sample_x[i] - mean_x, 2);
        stddev_y += pow(imu_sample_y[i] - mean_y, 2);
        stddev_z += pow(imu_sample_z[i] - mean_z, 2);        
    }
    imu_cov_ax = stddev_x / IMU_CAL_SAMPLES;
    imu_cov_ay = stddev_y / IMU_CAL_SAMPLES;
    imu_cov_az = stddev_z / IMU_CAL_SAMPLES;
    debug_printf("   >> External IMU Calibration accelerometer covariance diagonal [%f %f %f]\r\n", imu_cov_ax, imu_cov_ay, imu_cov_az); 

    /***************************/
    /* calibrate external gyro */
    /***************************/
    sum_x = sum_y = sum_z = 0;    
    for (i=0; i<IMU_CAL_SAMPLES; i++)
    {
      IMU_ReadGyroRaw(&imu_sample_x[i], &imu_sample_y[i], &imu_sample_z[i]);
      sum_x += imu_sample_x[i];
      sum_y += imu_sample_y[i];
      sum_z += imu_sample_z[i];
      HAL_Delay(10);      
    }
    mean_x = sum_x / IMU_CAL_SAMPLES;
    mean_y = sum_y / IMU_CAL_SAMPLES;
    mean_z = sum_z / IMU_CAL_SAMPLES;
    imu_cal_gx = mean_x;
    imu_cal_gy = mean_y;
    imu_cal_gz = mean_z;
    debug_printf("   >> External IMU Calibration factors gyro [%f %f %f]\r\n", imu_cal_gx, imu_cal_gy, imu_cal_gz);
    stddev_x = stddev_y = stddev_z = 0;
    for (i=0; i<IMU_CAL_SAMPLES; i++)
    {
        stddev_x += pow(imu_sample_x[i] - mean_x, 2);
        stddev_y += pow(imu_sample_y[i] - mean_y, 2);
        stddev_z += pow(imu_sample_z[i] - mean_z, 2);        
    }
    imu_cov_gx = stddev_x / IMU_CAL_SAMPLES;
    imu_cov_gy = stddev_y / IMU_CAL_SAMPLES;
    imu_cov_gz = stddev_z / IMU_CAL_SAMPLES;
    debug_printf("   >> External IMU Calibration gyro covariance diagonal [%f %f %f]\r\n", imu_cov_gx, imu_cov_gy, imu_cov_gz); 

    /************************************/
    /* calibrate onboard accelerometer  */
    /************************************/
    sum_x = sum_y = sum_z = 0;    
    for (i=0; i<IMU_CAL_SAMPLES; i++)
    {
      IMU_ReadAccelerometerRaw(&imu_sample_x[i], &imu_sample_y[i], &imu_sample_z[i]);
      sum_x += imu_sample_x[i];
      sum_y += imu_sample_y[i];
      sum_z += imu_sample_z[i];
      HAL_Delay(10);      
    }
    mean_x = sum_x / IMU_CAL_SAMPLES;
    mean_y = sum_y / IMU_CAL_SAMPLES;
    mean_z = sum_z / IMU_CAL_SAMPLES;
    onboard_imu_cal_ax = mean_x;
    onboard_imu_cal_ay = mean_y;
    onboard_imu_cal_az = 0;    // we dont want to calibrate Z because our IMU Sensor fusion stack expects gravity
    debug_printf("   >> Onboard IMU Calibration factors accelerometer [%f %f %f]\r\n", onboard_imu_cal_ax, onboard_imu_cal_ay, onboard_imu_cal_az);
    stddev_x = stddev_y = stddev_z = 0;
    for (i=0; i<IMU_CAL_SAMPLES; i++)
    {
        stddev_x += pow(imu_sample_x[i] - mean_x, 2);
        stddev_y += pow(imu_sample_y[i] - mean_y, 2);
        stddev_z += pow(imu_sample_z[i] - mean_z, 2);        
    }
    onboard_imu_cov_ax = stddev_x / IMU_CAL_SAMPLES;
    onboard_imu_cov_ay = stddev_y / IMU_CAL_SAMPLES;
    onboard_imu_cov_az = stddev_z / IMU_CAL_SAMPLES;
    debug_printf("   >> Onboard IMU Calibration accelerometer covariance diagonal [%f %f %f]\r\n", onboard_imu_cov_ax, onboard_imu_cov_ay, onboard_imu_cov_az); 
}