/******************************************************************************
* File Name: wlan_offload.h
*
* Description: This file contains the offload configuration for the
* Low Power Assistant (LPA) middleware.
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
#ifndef _WLAN_OFFLOAD_H_
#define _WLAN_OFFLOAD_H_


/* When enabled (1), the application takes the offload configuration from
 * the device-configurator generated sources. By default, the offload
 * manager (OLM) initializes with the generated sources configuration.
 * When disabled (0), the application takes the locally defined offload
 * configuration by the user. It then applies the offload configuration to
 * the OLM, and restarts the OLM to initialize with the new configuration.
 */
#define USE_CONFIGURATOR_GENERATED_CONFIG    (1)

/* 
 * Macros related to suspending and resuming the network stack.
 */
/* This macro specifies the interval in milliseconds that the device monitors
 * the network for inactivity. If the network is inactive for duration lesser 
 * than INACTIVE_WINDOW_MS in this interval, the MCU does not suspend the network 
 * stack and informs the calling function that the MCU wait period timed out 
 * while waiting for network to become inactive.
 */
#define INACTIVE_INTERVAL_MS                 (300)

/* This macro specifies the continuous duration in milliseconds for which the 
 * network has to be inactive. If the network is inactive for this duaration,
 * the MCU will suspend the network stack. Now, the MCU will not need to service
 * the network timers which allows it to stay longer in sleep/deepsleep.
 */
#define INACTIVE_WINDOW_MS                   (200)

/*
 * Delay between subsequent calls to suspending the network stack.
 * This is a safe delay which helps in preventing the race conditions
 * that might occur when activating and de-activating the offload.
 */
#define NETWORK_SUSPEND_DELAY_MS             (100)

/*******************************************************************************
 * The following defines the user-specified offload configuration.
 * These are used when Configurator generated configuration is not used.
 ******************************************************************************/
/* Enable(1) or Disable(0) Address Resolution Protocol (ARP) offload. */
#define ARP_OFFLOAD                          (1)

/* Enable(1) or Disable(0) Packet Filters. Packet filters are configured
 * via code when this option is enabled. However, it is the responsibility
 * of the user to configure the required packet types needed by the application
 * in the code.
 */
#define PACKET_FILTER_OFFLOAD                (1)

/* Enable(1) or Disable(0) TCP Keepalive offload. It is the responsibility of the user
 * to configure the TCP socket connection parameters.
 */
#define TCP_KEEPALIVE_OFFLOAD                (1)

/* Three types of LPA offloads available: ARP, Packet Filter, and TCP Keepalive. */
#define NUM_OFFLOAD_TYPES                    (3)

/************************ARP OFFLOAD CONFIGURATION*******************************/
#if ARP_OFFLOAD
/* Enables ARP agent in the WLAN firmware in Peer Auto Reply mode with IP Snoop enabled.
 * This means that the WLAN will respond to ARP request from any clients without soliciting
 * the Host MCU (PSoC 6). With the snoop being enabled, the WLAN monitors for ARP responses
 * from the Host to the Network, and caches them in its IP table.
 */
#define ARP_OL_AWAKE_ENABLE_MASK             (CY_ARP_OL_AGENT_ENABLE | CY_ARP_OL_PEER_AUTO_REPLY_ENABLE | CY_ARP_OL_SNOOP_ENABLE)
#define ARP_OL_SLEEP_ENABLE_MASK             (CY_ARP_OL_PEER_AUTO_REPLY_ENABLE)

/* The ARP cache table in the WLAN device expires after ARP_OL_PEER_AGE_SEC seconds. */
#define ARP_OL_PEER_AGE_SEC                  (1200UL)
#endif
/*******************************************************************************/

/*************************TCP KEEPALIVE OFFLOAD*********************************/
/*
 * Enable(1) or Disable(0) the Host TCP Keepalive via ENABLE_HOST_TCP_KEEPALIVE.
 * It is enabled by default so that when the PSoC 6 MCU wakes up, the lwIP network
 * stack will resume TCP Keepalives.
 */
#define ENABLE_HOST_TCP_KEEPALIVE            (1)
#define TCP_CLIENT_PORT_NUMBER               (3353)
#define TCP_SERVER_PORT_NUMBER               (3360)

#define TKO_INTERVAL_SECS                    (5) /* The WLAN will send TCP Keepalive packets in every 5 seconds. */
#define TKO_RETRY_INTERVAL_SECS              (3) /* The WLAN will send a retry packet in every 3 seconds, if no ACK was received from the server. */
#define TKO_RETRY_COUNT                      (3) /* The WLAN will retry for 3 times before returning error. */
#define REMOTE_TCP_SERVER_IP_ADDRESS         "192.168.0.108"
/*******************************************************************************/

/*************************PACKET FILTER OFFLOAD*********************************/
#if PACKET_FILTER_OFFLOAD

/* Maximum number of packet filters which can be applied to the OLM.
 * Only 19 valid entries are allowed. 20th entry is reserved for LPA to identify
 * the end of configuration.
 */
#define MAX_PACKET_FILTER                    (20)

/* A macro which indicates an empty packet filter configuration. */
#define FEATURE_TYPE_EMPTY                   (0)

/* Allow the following EtherType and PortType packets for this application. */
#define ETH_TYPE_ARP_PACKET                  (0x806)
#define ETH_TYPE_8021X_PACKET                (0x888E)
#define PORT_TYPE_DHCP_UDP                   (68)
#define PORT_TYPE_DNS_UDP                    (53)
#define PORT_TYPE_TCP_CLIENT                 TCP_CLIENT_PORT_NUMBER
#define PORT_TYPE_TCP_SERVER                 TCP_SERVER_PORT_NUMBER

#define GO_TO_LAST_FILTER_INDEX(x)           while ((FEATURE_TYPE_EMPTY != packet_filter_offload_config[x].feature) && \
                                                    (MAX_PACKET_FILTER > x)) \
                                             { \
                                                 x++; \
                                             }

#endif
/******************************************************************************/

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void RunApplicationTask(void *pArgument);
cy_rslt_t prvWifiConnect(void);
cy_rslt_t tcp_socket_connection_start(void);
cy_rslt_t olm_apply_offload_configuration(void);

#endif /* _WLAN_OFFLOAD_H_ */


/* [] END OF FILE */

