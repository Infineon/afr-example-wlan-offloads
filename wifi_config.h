/******************************************************************************
* File Name: wifi_config.h
*
* Description: This file contains macros for network configuration such as
*  Wi-Fi SSID, Password, and Security type.
*
******************************************************************************
* Copyright (2020), Cypress Semiconductor Corporation.
******************************************************************************
* This software, including source code, documentation and related materials
* (“Software”), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries (“Cypress”) and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software (“EULA”).
*
* If no EULA applies, Cypress hereby grants you a personal, nonexclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress’s integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death (“High Risk Product”). By
* including Cypress’s product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

/*******************************************************************************
* Include guard
*******************************************************************************/
#ifndef _WIFI_CONFIG_H_
#define _WIFI_CONFIG_H_

/* Wi-Fi Credentials: Modify WIFI_SSID and WIFI_PASSWORD to match your Wi-Fi network
 * Credentials.
 */
#define WIFI_SSID                                "WIFI_SSID"
#define WIFI_PASSWORD                            "WIFI_PASSWORD"

/* Security type of the Wi-Fi access point. See 'WIFISecurity_t' structure
 * in "iot_wifi.h" for more details.
 */
#define WIFI_SECURITY                            eWiFiSecurityWPA2

#define MAX_WIFI_RETRY_COUNT                     (3)
#define NULL_IP_ADDRESS                          "0.0.0.0"

#define APP_INFO(x)                              do { configPRINTF(("Info: ")); configPRINTF(x); } while(0);
#define ERR_INFO(x)                              do { configPRINTF(("Error: ")); configPRINTF(x); } while(0);

#define PRINT_AND_ASSERT(result, msg, args...)   do                                  \
                                                 {                                   \
                                                     if (CY_RSLT_SUCCESS  != result) \
                                                     {                               \
                                                         ERR_INFO((msg, ## args));   \
                                                         configASSERT(0);            \
                                                     }                               \
                                                 } while(0);

#define TCP_SOCKET_ERROR_DELAY_MS                (2000)

#endif /* _WIFI_CONFIG_H_ */


/* [] END OF FILE */

