/* module includes ********************************************************** */

/* own header files */
#include "XdkAppInfo.h"

#undef BCDS_MODULE_ID  /* Module ID define before including Basics package*/
#define BCDS_MODULE_ID XDK_APP_MODULE_ID_APP_CONTROLLER

/* own header files */
#include "AppController.h"

/* system header files */
#include <stdio.h>

/* additional interface header files */
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

/* local variables ********************************************************** */

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
                .Payload = "{ \"AccelerometerX\": \"3\","
                		" \"AccelerometerY\": \"3\","
                		" \"AccelerometerZ\": \"3\","
                		" \"Acoustic\": \"3\","
                		" \"Digital_light\": \"3\","
                		" \"GyroscopeX\": \"3\","
                		" \"GyroscopeY\": \"3\","
                		" \"GyroscopeZ\": \"3\","
                		" \"Humidity\": \"3\","
                		" \"MagnetometerX\": \"3\","
                		" \"MagnetometerY\": \"3\","
                		" \"MagnetometerZ\": \"3\","
                		" \"Pressure\": \"3\","
                		" \"Temperature\": \"3\"}",
                .PayloadLength = (sizeof(POST_REQUEST_BODY) - 1U),
                .Url = DEST_POST_PATH,
        }; /**< HTTP rest client POST parameters */


static xTaskHandle AppControllerHandle = NULL; /**< OS thread handle for Application controller */

static CmdProcessor_T * AppCmdProcessor; /**< Handle to store the main Command processor handle to be reused by ServalPAL thread */

/* local functions ********************************************************** */

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

/**
 * @brief To enable the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - SNTP (if HTTPS)
 * - HTTP rest client
 */
static void AppControllerEnable(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);

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

/**
 * @brief To setup the necessary modules for the application
 * - WLAN
 * - ServalPAL
 * - SNTP (if HTTPS)
 * - HTTP rest client
 *
 * @param[in] param1
 * Unused
 *
 * @param[in] param2
 * Unused
 */
static void AppControllerSetup(void * param1, uint32_t param2)
{
    BCDS_UNUSED(param1);
    BCDS_UNUSED(param2);

    Retcode_T retcode = WLAN_Setup(&WLANSetupInfo);
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
        assert(0); /* To provide LED indication for the user */
    }
}

/* global functions ********************************************************** */

/** Refer interface header for description */
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
        assert(0); /* To provide LED indication for the user */
    }
}

/**@} */
