/* --------------------------------------------------------------------------- |
 * INCLUDES & DEFINES ******************************************************** |
 * -------------------------------------------------------------------------- */
#include "XdkAppInfo.h"
#undef BCDS_MODULE_ID  // [i] Module ID define before including Basics package
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

/* own header files */
#include "AppController.h"

/* system header files */
#include <stdio.h>

#include <stdio.h>
#include "BCDS_CmdProcessor.h"
#include "FreeRTOS.h"
#include "XdkSensorHandle.h"
#include "timers.h"

#include "XDK_WLAN.h"
#include "XDK_ServalPAL.h"
#include "XDK_HTTPRestClient.h"
#include "XDK_SNTP.h"
#include "BCDS_BSP_Board.h"

#include "BCDS_WlanNetworkConfig.h"
#include "BCDS_WlanNetworkConnect.h"
#include "BCDS_CmdProcessor.h"
#include "BCDS_Assert.h"
#include "XDK_Utils.h"
#include "FreeRTOS.h"
#include "task.h"

/* constant definitions ***************************************************** */

#if HTTP_SECURE_ENABLE
#define APP_RESPONSE_FROM_SNTP_SERVER_TIMEOUT           UINT32_C(10000)/**< Timeout for SNTP server time sync */
#endif /* HTTP_SECURE_ENABLE */

#define APP_RESPONSE_FROM_HTTP_SERVER_POST_TIMEOUT      UINT32_C(25000)/**< Timeout for completion of HTTP rest client POST */

#define APP_RESPONSE_FROM_HTTP_SERVER_GET_TIMEOUT       UINT32_C(25000)/**< Timeout for completion of HTTP rest client GET */

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

float accelerometerXValue = 0;
float accelerometerYValue = 0;
float accelerometerZValue = 0;
float acousticValue = 0;
long int temperatureValue = 0;
long int pressureValue = 0;
long int humidityValue = 0;
int gyroscopeXValue = 0;
int gyroscopeYValue = 0;
int gyroscopeZValue = 0;
unsigned int lightValue = 0;
long int magnetometerXValue = 0;
long int magnetometerYValue = 0;
long int magnetometerZValue = 0;

/* --------------------------------------------------------------------------- |
 * VARIABLES ***************************************************************** |
 * -------------------------------------------------------------------------- */

const float aku340ConversionRatio = pow(10,(-38/20));

static WLAN_Setup_T WLANSetupInfo =
        {
                .IsEnterprise = false,
                .IsHostPgmEnabled = false,
                .SSID = WLAN_SSID,
                //.Username = WLAN_PSK, /* Unused for Personal WPA2 connection */
                .Password = WLAN_PSK,
        };/**< WLAN setup parameters */

#if HTTP_SECURE_ENABLE
static SNTP_Setup_T SNTPSetupInfo =
        {
                .ServerUrl = SNTP_SERVER_URL,
                .ServerPort = SNTP_SERVER_PORT,
        };/**< SNTP setup parameters */
#endif /* HTTP_SECURE_ENABLE */

static HTTPRestClient_Setup_T HTTPRestClientSetupInfo =
        {
                .IsSecure = HTTP_SECURE_ENABLE,
        };/**< HTTP rest client setup parameters */

static HTTPRestClient_Config_T HTTPRestClientConfigInfo =
        {
                .IsSecure = HTTP_SECURE_ENABLE,
                .DestinationServerUrl = DEST_SERVER_HOST,
                .DestinationServerPort = DEST_SERVER_PORT,
                .RequestMaxDownloadSize = REQUEST_MAX_DOWNLOAD_SIZE,
        }; /**< HTTP rest client configuration parameters */

static HTTPRestClient_Post_T HTTPRestClientPostInfo =
        {
                .Payload = "{ \"AccelerometerX\": \"%f\","
                		" \"AccelerometerY\": \"%f\","
                		" \"AccelerometerZ\": \"%f\","
                		" \"Acoustic\": \"%f\","
                		" \"Digital_light\": \"%d\","
                		" \"GyroscopeX\": \"%10d\","
                		" \"GyroscopeY\": \"%10d\","
                		" \"GyroscopeZ\": \"%10d\","
                		" \"MagnetometerX\": \"%ld\","
                		" \"MagnetometerY\": \"%ld\","
                		" \"MagnetometerZ\": \"%ld\","
                		" \"Pressure\": \"%ld\","
                		" \"Temperature\": \"%ld\"}",
						accelerometerXValue,
						accelerometerYValue,
						accelerometerZValue,
						acousticValue,
						lightValue,
						gyroscopeXValue,
						gyroscopeYValue,
						gyroscopeZValue,
						magnetometerXValue,
						magnetometerYValue,
						magnetometerZValue,
						pressureValue,
						temperatureValue,
                .PayloadLength = (sizeof(POST_REQUEST_BODY) - 1U),
                .Url = DEST_POST_PATH,
        }; /**< HTTP rest client POST parameters */


static xTaskHandle AppControllerHandle = NULL; /**< OS thread handle for Application controller */

static CmdProcessor_T * AppCmdProcessor; /**< Handle to store the main Command processor handle to be reused by ServalPAL thread */

/* --------------------------------------------------------------------------- |
 * EXECUTING FUNCTIONS ******************************************************* |
 * -------------------------------------------------------------------------- */

/**
 * @brief This will validate the WLAN network connectivity
 *
 * If there is no connectivity then it will scan for the given network and try to reconnect
 *
 * @return  RETCODE_OK on success, or an error code otherwise.
 */
static Retcode_T AppControllerValidateWLANConnectivity(void)
{
    Retcode_T retcode = RETCODE_OK;
    WlanNetworkConnect_IpStatus_T nwStatus;
    nwStatus = WlanNetworkConnect_GetIpStatus();

    if (WLANNWCT_IPSTATUS_CT_AQRD != nwStatus)
    {
#if HTTP_SECURE_ENABLE
        static bool isSntpDisabled = false;
        if (false == isSntpDisabled)
        {
            retcode = SNTP_Disable();
        }
        if (RETCODE_OK == retcode)
        {
            isSntpDisabled = true;
            retcode = WLAN_Reconnect();
        }
        if (RETCODE_OK == retcode)
        {
            retcode = SNTP_Enable();
        }
#else
        retcode = WLAN_Reconnect();
#endif /* HTTP_SECURE_ENABLE */

    }
    return retcode;

}

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
            accelerometerXValue = (float) getAccelMpsData.xAxisData;
            accelerometerYValue = (float) getAccelMpsData.yAxisData;
            accelerometerZValue = (float) getAccelMpsData.zAxisData;
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
        acousticValue = calcSoundPressure(acousticData);
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
        pressureValue = (long int) bme280.pressure;
        temperatureValue = (long int) bme280.temperature;
        humidityValue = (long int) bme280.humidity;
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
            gyroscopeXValue = (int) bmg160.xAxisData;
            gyroscopeYValue = (int) bmg160.yAxisData;
            gyroscopeZValue = (int) bmg160.zAxisData;
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
            lightValue = (unsigned int) max44009;
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
    magnetometerXValue = (long int) bmm150.xAxisData;
    magnetometerYValue = (long int) bmm150.yAxisData;
    magnetometerZValue = (long int) bmm150.zAxisData;
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

/**
 * @brief Responsible for controlling the HTTP Example application control flow.
 *
 * - Synchronize the node with the SNTP server for time-stamp (if HTTPS)
 * - Check whether the WLAN network connection is available
 * - Do a HTTP rest client POST
 * - Wait for INTER_REQUEST_INTERVAL if POST was successful
 * - Redo the last 4 steps
 *
 * @param[in] pvParameters
 * Unused
 */
static void AppControllerFire(void* pvParameters)
{
    BCDS_UNUSED(pvParameters);

    Retcode_T retcode = RETCODE_OK;

#if HTTP_SECURE_ENABLE

    uint64_t sntpTimeStampFromServer = 0UL;

    /* We Synchronize the node with the SNTP server for time-stamp.
     * Since there is no point in doing a HTTPS communication without a valid time */
    do
    {
        retcode = SNTP_GetTimeFromServer(&sntpTimeStampFromServer, APP_RESPONSE_FROM_SNTP_SERVER_TIMEOUT);
        if ((RETCODE_OK != retcode) || (0UL == sntpTimeStampFromServer))
        {
            printf("AppControllerFire : SNTP server time was not synchronized. Retrying...\r\n");
        }
    } while (0UL == sntpTimeStampFromServer);

    BCDS_UNUSED(sntpTimeStampFromServer); /* Copy of sntpTimeStampFromServer will be used be HTTPS for TLS handshake */
#endif /* HTTP_SECURE_ENABLE */

    while (1)
    {
        /* Resetting / clearing the necessary buffers / variables for re-use */
        retcode = RETCODE_OK;

        /* Check whether the WLAN network connection is available */
        retcode = AppControllerValidateWLANConnectivity();

        /* Do a HTTP rest client POST */
        if (RETCODE_OK == retcode)
        {
            retcode = HTTPRestClient_Post(&HTTPRestClientConfigInfo, &HTTPRestClientPostInfo, APP_RESPONSE_FROM_HTTP_SERVER_POST_TIMEOUT);
        }
        if (RETCODE_OK == retcode)
        {
            /* Wait for INTER_REQUEST_INTERVAL */
            vTaskDelay(pdMS_TO_TICKS(INTER_REQUEST_INTERVAL));
        }
        if (RETCODE_OK != retcode)
        {
            printf("Error in Post/get request: Will trigger another post/get after INTER_REQUEST_INTERVAL\r\n");
            vTaskDelay(pdMS_TO_TICKS(INTER_REQUEST_INTERVAL));
            /* Report error and continue */
            Retcode_RaiseError(retcode);
        }
    }
}

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

    Retcode_T retcode = WLAN_Enable();
        if (RETCODE_OK == retcode)
        {
            retcode = ServalPAL_Enable();
        }
    #if HTTP_SECURE_ENABLE
        if (RETCODE_OK == retcode)
        {
            retcode = SNTP_Enable();
        }
    #endif /* HTTP_SECURE_ENABLE */
        if (RETCODE_OK == retcode)
        {
            retcode = HTTPRestClient_Enable();
        }
        if (RETCODE_OK == retcode)
        {
            if (pdPASS != xTaskCreate(AppControllerFire, (const char * const ) "AppController", TASK_STACK_SIZE_APP_CONTROLLER, NULL, TASK_PRIO_APP_CONTROLLER, &AppControllerHandle))
            {
                retcode = RETCODE(RETCODE_SEVERITY_ERROR, RETCODE_OUT_OF_RESOURCES);
            }
        }
        if (RETCODE_OK != retcode)
        {
            printf("AppControllerEnable : Failed \r\n");
            Retcode_RaiseError(retcode);
            assert(0); /* To provide LED indication for the user */
        }

        Utils_PrintResetCause();
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

    retcode = WLAN_Setup(&WLANSetupInfo);
        if (RETCODE_OK == retcode)
        {
            retcode = ServalPAL_Setup(AppCmdProcessor);
        }
    #if HTTP_SECURE_ENABLE
        if (RETCODE_OK == retcode)
        {
            retcode = SNTP_Setup(&SNTPSetupInfo);
        }
    #endif /* HTTP_SECURE_ENABLE */
        if (RETCODE_OK == retcode)
        {
            retcode = HTTPRestClient_Setup(&HTTPRestClientSetupInfo);
        }
        if (RETCODE_OK == retcode)
        {
            retcode = CmdProcessor_Enqueue(AppCmdProcessor, AppControllerEnable, NULL, UINT32_C(0));
        }

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
