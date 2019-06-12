/**
 *  @file
 *
 *  @brief Configuration header for the AppController.c file.
 *
 */

/* header definition ******************************************************** */
#ifndef APPCONTROLLER_H_
#define APPCONTROLLER_H_

/* local interface declaration ********************************************** */
#include "XDK_Utils.h"

/* local type and macro definitions */

/* local module global variable declarations */

/* local inline function definitions */

/* WLAN configurations ****************************************************** */

#warning Please configure your WLAN below and remove this line.

/**
 * WLAN_SSID is the WIFI network name where user wants connect the XDK device.
 * Make sure to update the WLAN_PSK constant according to your required WIFI network.
 */
#define WLAN_SSID                           "XDK_Hotspot"

/**
 * WLAN_PSK is the WIFI router WPA/WPA2 password used at the Wifi network connection.
 * Make sure to update the WLAN_PSK constant according to your router password.
 */
#define WLAN_PSK                            "FreeWifi"

/**
 * WLAN_STATIC_IP is a boolean. If "true" then static IP will be assigned and if "false" then DHCP is used.
 */
#define WLAN_STATIC_IP                      false

/**
 * WLAN_IP_ADDR is the WIFI router WPA/WPA2 static IPv4 IP address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_IP_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_IP_ADDR                        XDK_NETWORK_IPV4(0, 0, 0, 0)

/**
 * WLAN_GW_ADDR is the WIFI router WPA/WPA2 static IPv4 gateway address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_GW_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_GW_ADDR                        XDK_NETWORK_IPV4(0, 0, 0, 0)

/**
 * WLAN_DNS_ADDR is the WIFI router WPA/WPA2 static IPv4 DNS address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_DNS_ADDR constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_DNS_ADDR                       XDK_NETWORK_IPV4(0, 0, 0, 0)

/**
 * WLAN_MASK is the WIFI router WPA/WPA2 static IPv4 mask address (unused if WLAN_STATIC_IP is false)
 * Make sure to update the WLAN_MASK constant according to your required WIFI network,
 * if WLAN_STATIC_IP is "true".
 */
#define WLAN_MASK                           XDK_NETWORK_IPV4(0, 0, 0, 0)

/* Server configurations ***************************************************** */

/**
 * DEST_SERVER_HOST is the host name of the web server we will send HTTP requests to.
 * If you want to test this example without setting up your own server, you can use publicly available services.
 */
#define DEST_SERVER_HOST                "fas-webteach.sunderland.ac.uk"

/**
 * DEST_SERVER_PORT is the TCP port to which we will send HTTP requests to.
 * The default of 80 should be fine for most applications.
 */
#define DEST_SERVER_PORT                UINT16_C(22)

/**
 * DEST_SERVER_PORT_SECURE is the TCP port to which we will send HTTPS requests to.
 * The default of 443 should be fine for most applications.
 */
#define DEST_SERVER_PORT_SECURE         UINT16_C(22)

/**
 * DEST_POST_PATH is the path relative to the DEST_SERVER_HOST that we will send
 * the HTTP POST request to.
 *
 * Change this value if you use your own web server.
 */
#define DEST_POST_PATH                  "/~ex0eby/sendValuesToDatabase.php"

/**
 * POST_REQUEST_BODY is sent as body with the HTTP POST request
 */
#define POST_REQUEST_BODY               "{\"acoustic\":\"3\"}"

/**
 * The time we wait (in milliseconds) between sending HTTP requests.
 */
#define INTER_REQUEST_INTERVAL          UINT32_C(10000)

#warning Configure HTTP_SECURE_ENABLE to 1 to enable the HTTPS based Communication otherwise Communication will happen through HTTP.

/**
 * HTTP_SECURE_ENABLE is Set to Use HTTP With Security
 */
#define HTTP_SECURE_ENABLE              UINT32_C(1)

#if HTTP_SECURE_ENABLE /* Below are SNTP related macros which are only valid for HTTPS */

/**
 * SNTP_SERVER_URL is the SNTP server URL.
 */
#define SNTP_SERVER_URL                 "0.uk.pool.ntp.org"

/**
 * SNTP_SERVER_PORT is the SNTP server port number
 */
#define SNTP_SERVER_PORT                UINT16_C(123)

#endif /* #if HTTP_SECURE_ENABLE */

/**
 * The maximum amount of data we download in a single request (in bytes). This number is
 * limited by the platform abstraction layer implementation that ships with the
 * XDK. The maximum value that will work here is 512 bytes.
 */
#define REQUEST_MAX_DOWNLOAD_SIZE       UINT32_C(512)

/**
 * @brief Gives control to the Application controller.
 *
 * @param[in] cmdProcessorHandle
 * Handle of the main command processor which shall be used based on the application needs
 *
 * @param[in] param2
 * Unused
 */
void AppController_Init(void * cmdProcessorHandle, uint32_t param2);

#endif /* APPCONTROLLER_H_ */
