/* --------------------------------------------------------------------------- |
 * INCLUDES & DEFINES ******************************************************** |
 * -------------------------------------------------------------------------- */
#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID  // [i] Module ID define before including Basics package
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

#include <stdio.h>
#include "BCDS_CmdProcessor.h"
#include "FreeRTOS.h"
#include "XdkSensorHandle.h"
#include "timers.h"

/* --------------------------------------------------------------------------- |
 * HANDLES ******************************************************************* |
 * -------------------------------------------------------------------------- */

static CmdProcessor_T * AppCmdProcessor;
xTimerHandle calibratedAccelerometerHandle = NULL;
xTimerHandle acousticHandle = NULL;
xTimerHandle environmentalHandle = NULL;
xTimerHandle gyroscopeHandle = NULL;
xTimerHandle lightSensorHandle  = NULL;
xTimerHandle magnetometerHandle = NULL;


/* --------------------------------------------------------------------------- |
 * VARIABLES ***************************************************************** |
 * -------------------------------------------------------------------------- */

const float aku340ConversionRatio = pow(10,(-38/20));

/* --------------------------------------------------------------------------- |
 * EXECUTING FUNCTIONS ******************************************************* |
 * -------------------------------------------------------------------------- */

static void readCalibratedAccelerometer(xTimerHandle xTimer)
{
    (void) xTimer;

    CalibratedAccel_Status_T calibrationAccuracy = CALIBRATED_ACCEL_UNRELIABLE;
    Retcode_T calibrationStatus = RETCODE_FAILURE;

    calibrationStatus = CalibratedAccel_getStatus(&calibrationAccuracy);

    if (calibrationAccuracy == CALIBRATED_ACCEL_HIGH && calibrationStatus == RETCODE_OK){
        Retcode_T returnDataValue = RETCODE_FAILURE;

        /* Reading of the data of the calibrated accelerometer */
        CalibratedAccel_XyzMps2Data_T getAccelMpsData = { INT32_C(0), INT32_C(0), INT32_C(0) };
        returnDataValue = CalibratedAccel_readXyzMps2Value(&getAccelMpsData);

        if (returnDataValue == RETCODE_OK){
            printf("Calibrated acceleration: %10f m/s2[X] %10f m/s2[Y] %10f m/s2[Z]\n\r",
                        (float) getAccelMpsData.xAxisData, (float) getAccelMpsData.yAxisData, (float) getAccelMpsData.zAxisData);
        }
    }
}

float calcSoundPressure(float acousticRawValue){
    return (acousticRawValue/aku340ConversionRatio);
}

static void readAcousticSensor(xTimerHandle xTimer)
{
    (void) xTimer;

    float acousticData;

    if (RETCODE_OK == NoiseSensor_ReadRmsValue(&acousticData,10U)) {
        printf("Sound pressure: %f \r\n", calcSoundPressure(acousticData));
    }
}

static void readEnvironmental(xTimerHandle xTimer)
{
    (void) xTimer;

    Retcode_T returnValue = RETCODE_FAILURE;

    /* read and print BME280 environmental sensor data */

    Environmental_Data_T bme280 = { INT32_C(0), UINT32_C(0), UINT32_C(0) };

    returnValue = Environmental_readData(xdkEnvironmental_BME280_Handle, &bme280);

    if ( RETCODE_OK == returnValue) {
        printf("Environmental Data : p =%ld Pa T =%ld mDeg h =%ld %%rh\n\r",
        (long int) bme280.pressure, (long int) bme280.temperature, (long int) bme280.humidity);
    }
}

static void readGyroscope(xTimerHandle xTimer)
{
    (void) xTimer;

    Retcode_T returnValue = RETCODE_FAILURE;

    Gyroscope_XyzData_T bmg160 = {INT32_C(0), INT32_C(0), INT32_C(0)};

        memset(&bmg160, 0, sizeof(CalibratedGyro_DpsData_T));
        returnValue = Gyroscope_readXyzDegreeValue(xdkGyroscope_BMG160_Handle, &bmg160);

        if (RETCODE_OK == returnValue){
            printf("Gyroscope Data: %10d mDeg[X] %10d mDeg[Y] %10d mDeg[Z]\n\r",
                (int) bmg160.xAxisData, (int) bmg160.yAxisData, (int) bmg160.zAxisData);
        }
}

static void readLightSensor(xTimerHandle xTimer)
{
    (void) xTimer;

    Retcode_T returnValue = RETCODE_FAILURE;
    uint32_t max44009 = UINT32_C(0);

        returnValue = LightSensor_readLuxData(xdkLightSensor_MAX44009_Handle, &max44009);

        if (RETCODE_OK == returnValue){
            printf("Light sensor data :%d milli lux\n\r",(unsigned int) max44009);
        }
}

static void readMagnetometer(xTimerHandle xTimer){

    (void) xTimer;
    Retcode_T returnValue = RETCODE_FAILURE;

    /* read and print BMM150 magnetometer data */

    Magnetometer_XyzData_T bmm150 = {INT32_C(0), INT32_C(0), INT32_C(0), INT32_C(0)};

    returnValue = Magnetometer_readXyzTeslaData(xdkMagnetometer_BMM150_Handle, &bmm150);

    if (RETCODE_OK == returnValue) {
    printf("Magnetic Data: x =%ld mT y =%ld mT z =%ld mT \n\r",
          (long int) bmm150.xAxisData, (long int) bmm150.yAxisData, (long int) bmm150.zAxisData);
    }
}

static void initSensors(void)
{

	Retcode_T returnValue = RETCODE_FAILURE;

	//Acoustic

	//Accelerometer

	Retcode_T calibratedAccelInitReturnValue = RETCODE_FAILURE;
	calibratedAccelInitReturnValue = CalibratedAccel_init(xdkCalibratedAccelerometer_Handle);

	if (calibratedAccelInitReturnValue != RETCODE_OK) {
	    printf("Initializing Calibrated Accelerometer failed \n\r");
	}

	//Environmental

	 Retcode_T returnOverSamplingValue = RETCODE_FAILURE;
	 Retcode_T returnFilterValue = RETCODE_FAILURE;

	 /* initialize environmental sensor */

	 returnValue = Environmental_init(xdkEnvironmental_BME280_Handle);
	 if ( RETCODE_OK != returnValue) {
		 printf("BME280 Environmental Sensor initialization failed\n\r");
	 }

	 returnOverSamplingValue = Environmental_setOverSamplingPressure(xdkEnvironmental_BME280_Handle,ENVIRONMENTAL_BME280_OVERSAMP_2X);
	 if (RETCODE_OK != returnOverSamplingValue) {
	     printf("Configuring pressure oversampling failed \n\r");
	 }

	 returnFilterValue = Environmental_setFilterCoefficient(xdkEnvironmental_BME280_Handle,ENVIRONMENTAL_BME280_FILTER_COEFF_2);
	 if (RETCODE_OK != returnFilterValue) {
	     printf("Configuring pressure filter coefficient failed \n\r");
	 }

	//Gyroscope

    Retcode_T returnBandwidthValue = RETCODE_FAILURE;
    Retcode_T returnRangeValue = RETCODE_FAILURE;

    returnValue = Gyroscope_init(xdkGyroscope_BMG160_Handle);

    if ( RETCODE_OK != returnValue) {
        printf("BMG160 Gyroscope initialization failed\n\r");
    }
    returnBandwidthValue = Gyroscope_setBandwidth(xdkGyroscope_BMG160_Handle, GYROSCOPE_BMG160_BANDWIDTH_116HZ);
    if (RETCODE_OK != returnBandwidthValue) {
        printf("Configuring bandwidth failed \n\r");
    }
    returnRangeValue = Gyroscope_setRange(xdkGyroscope_BMG160_Handle, GYROSCOPE_BMG160_RANGE_500s);
    if (RETCODE_OK != returnRangeValue) {
        printf("Configuring range failed \n\r");
    }

    //Light

    Retcode_T returnBrightnessValue = RETCODE_FAILURE;
    Retcode_T returnIntegrationTimeValue = RETCODE_FAILURE;

    returnValue = LightSensor_init(xdkLightSensor_MAX44009_Handle);

    if ( RETCODE_OK != returnValue) {
         printf("MAX44009 Light Sensor initialization failed\n\r");
    }
    returnBrightnessValue = LightSensor_setBrightness(xdkLightSensor_MAX44009_Handle,LIGHTSENSOR_NORMAL_BRIGHTNESS);
    if (RETCODE_OK != returnBrightnessValue) {
         printf("Configuring brightness failed \n\r");
    }
    returnIntegrationTimeValue = LightSensor_setIntegrationTime(xdkLightSensor_MAX44009_Handle,LIGHTSENSOR_200MS);
    if (RETCODE_OK != returnIntegrationTimeValue) {
         printf("Configuring integration time failed \n\r");
    }

    //Magnetometer

    Retcode_T returnDataRateValue = RETCODE_FAILURE;
    Retcode_T returnPresetModeValue = RETCODE_FAILURE;

    /* initialize magnetometer */

    returnValue = Magnetometer_init(xdkMagnetometer_BMM150_Handle);

    if(RETCODE_OK != returnValue){
        printf("BMM150 Magnetometer initialization failed \n\r");
    }

    returnDataRateValue = Magnetometer_setDataRate(xdkMagnetometer_BMM150_Handle,
             MAGNETOMETER_BMM150_DATARATE_10HZ);
    if (RETCODE_OK != returnDataRateValue) {
    	printf("Configuring data rate failed \n\r");
    }
    returnPresetModeValue = Magnetometer_setPresetMode(xdkMagnetometer_BMM150_Handle,
             MAGNETOMETER_BMM150_PRESETMODE_REGULAR);
    if (RETCODE_OK != returnPresetModeValue) {
    	printf("Configuring preset mode failed \n\r");
    }
}

/* --------------------------------------------------------------------------- |
 * BOOTING- AND SETUP FUNCTIONS ********************************************** |
 * -------------------------------------------------------------------------- */

static void AppControllerEnable(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);

    uint32_t timerBlockTime = UINT32_MAX;

    xTimerStart(calibratedAccelerometerHandle,timerBlockTime);
    xTimerStart(acousticHandle,timerBlockTime);
    xTimerStart(environmentalHandle,timerBlockTime);
    xTimerStart(gyroscopeHandle,timerBlockTime);
    xTimerStart(lightSensorHandle,timerBlockTime);
    xTimerStart(magnetometerHandle,timerBlockTime);
}

static void AppControllerSetup(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);

    Retcode_T retcode = RETCODE_OK;
    uint32_t timerDelay = UINT32_C(1000);
    uint32_t timerAutoReloadOn = UINT32_C(1);

    // Setup of the necessary module
    initSensors();

    // Creation and start of the timer
    calibratedAccelerometerHandle = xTimerCreate((const char *) "readCalibratedAccelerometer", timerDelay, timerAutoReloadOn, NULL, readCalibratedAccelerometer);
    acousticHandle = xTimerCreate((const char *) "readAcousticSensor", timerDelay,timerAutoReloadOn, NULL, readAcousticSensor);
    environmentalHandle = xTimerCreate((const char *) "readEnvironmental", timerDelay,timerAutoReloadOn, NULL, readEnvironmental);
    gyroscopeHandle = xTimerCreate((const char *) "readGyroscope", timerDelay, timerAutoReloadOn, NULL, readGyroscope);
    lightSensorHandle = xTimerCreate((const char *) "readAmbientLight", timerDelay, timerAutoReloadOn, NULL, readLightSensor);
    magnetometerHandle = xTimerCreate((const char *) "readMagnetometer", timerDelay,timerAutoReloadOn, NULL, readMagnetometer);
    retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerEnable, NULL, UINT32_C(0));

    if (RETCODE_OK != retcode)
    {
        printf("AppControllerSetup : Failed \r\n");
        Retcode_RaiseError(retcode);
        assert(0);
    }
}

void AppController_Init(void * cmdProcessorHandle, uint32_t param2)
{
    BCDS_UNUSED(param2);

    Retcode_T retcode = RETCODE_OK;

    if (cmdProcessorHandle == NULL)
    {
        printf("AppController_Init : Command processor handle is NULL \r\n");
        retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_NULL_POINTER);
    }
    else
    {
        AppCmdProcessor = (CmdProcessor_T *) cmdProcessorHandle;
        retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerSetup, NULL, UINT32_C(0));
    }
    if (RETCODE_OK != retcode)
    {
        Retcode_RaiseError(retcode);
        assert(0);
    }
}
