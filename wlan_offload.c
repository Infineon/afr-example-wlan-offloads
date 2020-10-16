/*******************************************************************************
 * File Name:   wlan_offload.c
 *
 * Description: This file contains functions necessary to initialize various
 * WLAN offload types such as ARP, Packet Filter, and TCP Keepalive. This also
 * has function to suspend the network stack which helps the PSoC 6 MCU in
 * entering the deep sleep power mode.
 *
 *******************************************************************************
 * (c) 2020, Cypress Semiconductor Corporation. All rights reserved.
 *******************************************************************************
 * This software, including source code, documentation and related materials
 * ("Software"), is owned by Cypress Semiconductor Corporation or one of its
 * subsidiaries ("Cypress") and is protected by and subject to worldwide patent
 * protection (United States and foreign), United States copyright laws and
 * international treaty provisions. Therefore, you may use this Software only
 * as provided in the license agreement accompanying the software package from
 * which you obtained this Software ("EULA").
 *
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software source
 * code solely for use in connection with Cypress's integrated circuit products.
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
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer of such
 * system or application assumes all risk of such use and in doing so agrees to
 * indemnify Cypress against all liability.
 ******************************************************************************/

/* Include header files */
#include "FreeRTOS.h"
#include "task.h"
#include "iot_wifi.h"
#include "iot_wifi_common.h"
#include "platform/iot_threads.h"
#include <stdbool.h>
#include <lwip/netif.h>
#include "cyhal.h"
#include "cybsp.h"

/* Low Power Assistant header files. */
#include "network_activity_handler.h"
#include "cy_lpa_wifi_arp_ol.h"
#include "cy_lpa_wifi_pf_ol.h"
#include "cy_lpa_wifi_tko_ol.h"

/* LPA offload manager (OLM) header file. */
#include "cy_OlmInterface.h"

/* Wi-Fi Host Driver (WHD) header files. */
#include "whd_wifi_api.h"

/* Secure socket structure definition */
#include "iot_secure_sockets.h"

/* Low Power Assistant offload and the network configuration. */
#include "wlan_offload.h"
#include "wifi_config.h"

/*******************************************************************************
 * Macros
 ******************************************************************************/
#define APP_TASK_STACK_SIZE       (configMINIMAL_STACK_SIZE * 8)
#define APP_TASK_PRIORITY         tskIDLE_PRIORITY

/*******************************************************************************
 * Static Global Structures and variables
 ******************************************************************************/
#if ARP_OFFLOAD
/*
 * Offload Manager (OLM) configuration for the ARP offload.
 * The ARP offload is active when the PSoC 6 MCU is awake or sleeping.
 * Enables ARP agent in the WLAN firmware in Peer Auto Reply mode with
 * IP Snoop enabled. This means that the WLAN will respond to ARP request
 * from any clients without soliciting the Host MCU (PSoC 6). With the snoop
 * being enabled, the WLAN monitors for ARP responses from the Host to the
 * Network, and caches them in its IP table.
 */
static const arp_ol_cfg_t arp_offload_config =
{
    .awake_enable_mask = ARP_OL_AWAKE_ENABLE_MASK,
    .sleep_enable_mask = ARP_OL_SLEEP_ENABLE_MASK,
    .peerage = ARP_OL_PEER_AGE_SEC,
};

/* ARP offload context. */
static arp_ol_t arpol_context;
#endif

#if PACKET_FILTER_OFFLOAD
/*
 * Offload Manager (OLM) configuration for the Packet Filter offload.
 * Maximum of up to 20 packet filters can be configured.
 */
static cy_pf_ol_cfg_t packet_filter_offload_config[MAX_PACKET_FILTER];

/* Packet filter offload context. */
static pf_ol_t  pfol_context;
#endif

#if TCP_KEEPALIVE_OFFLOAD
/* TCP Keepalive offload context. */
static tko_ol_t tkol_context;
#endif

/*
 * Offload Manager (OLM) configuration for the TCP Keepalive offload.
 * Maximum up to 4 socket connections can be configured.
 *   interval       - The WLAN will send TCP Keepalive packets in every 5 seconds.
 *   retry_interval - The WLAN will send a retry packet in every 3 seconds,
 *                    if no ACK was received from the server.
 *   retry_count    - The WLAN will retry for 3 times before returning error.
 *   local_port     - Local port number (TCP client).
 *   remote_port    - Remote port number (TCP server).
 *   remote_ip      - Remote TCP server IP address.
 *
 * Note: The application will establish the TCP socket connections
 * with the below configuration, irrespective of TCP_KEEPALIVE_OFFLOAD
 * enabled or disabled and when USE_CONFIGURATOR_GENERATED_CONFIG
 * is set to 0 in the file wlan_offload.h.
 */
static const cy_tko_ol_cfg_t tcp_keepalive_offload_config =
{
    .interval       = TKO_INTERVAL_SECS,
    .retry_interval = TKO_RETRY_INTERVAL_SECS,
    .retry_count    = TKO_RETRY_COUNT,
    .ports[0]       = {
                        .local_port = TCP_CLIENT_PORT_NUMBER,
                        .remote_port = TCP_SERVER_PORT_NUMBER,
                        .remote_ip = REMOTE_TCP_SERVER_IP_ADDRESS
                      },
};

/* Holds all the offload configuration. This will be used by the offload manager
 * (OLM) to apply the configuration to the WLAN device. The offload manager
 * requires a NULL entry as the last entry in the list. So the offload
 * list at index NUM_OFFLOAD_TYPES+1 will have all NULLs.
 */
static ol_desc_t user_configuration_list[NUM_OFFLOAD_TYPES + 1];

/* TCP socket handle for each connection */
Socket_t global_socket[MAX_TKO_CONN] = {NULL};

/*******************************************************************************
 * Function definitions
 ******************************************************************************/
/*******************************************************************************
* Function Name: InitApplication
********************************************************************************
* Summary:
*  Initializes all the tasks, queues, and other resources required to run the
*  application.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void InitApplication(void)
{
    /* Set up the application task */
    Iot_CreateDetachedThread(RunApplicationTask, NULL, APP_TASK_PRIORITY, APP_TASK_STACK_SIZE);
}

/***************************************************************************************
* Function Name: add_offload_configuration_to_olm_list
****************************************************************************************
* Summary:
*  Adds the given offload configuration for the given offload type to the
*  offload manager configuration list.
*
* Parameters:
*  offload_name     : Name of the offload type in string for identification.
*                     The offload names can be "ARP" or "Pkt_Filter" or "TKO".
*  offload_config   : Holds the offload configuration data for the given offload type.
*  offload_functions: Initialization and management functions for the given offload type.
*  offload_context  : Instance of the offload table.
*
* Return:
*  cy_rslt_t        : Returns CY_RSLT_SUCCESS if the offload configuration has been
*  added to the OLM successfully. Otherwise, it returns CY_RSLT_TYPE_ERROR.
*
***************************************************************************************/
static cy_rslt_t add_offload_configuration_to_olm_list(const char *offload_name,
                                                       const void *offload_config,
                                                       const struct ol_fns *offload_functions,
                                                       void *offload_context)
{
    int index = 0;
    ol_desc_t *offload_list = user_configuration_list;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Go to the NULL entry list to add the new OLM configuration. */
    for (index = 0; ((offload_list->name[0] != '\0') &&
         (index < NUM_OFFLOAD_TYPES)); index++)
    {
        offload_list++;
    }

    if (NUM_OFFLOAD_TYPES == index)
    {
        ERR_INFO(("The OLM list is full and cannot accept more entries.\n"));
        result = CY_RSLT_TYPE_ERROR;
    }
    else
    {
        /* Add a new OLM configuration to the list. OLM supports
         * ARP, Packet Filter, and TCP Keepalive WLAN offloads.
         */ 
        offload_list->name = offload_name; 
        offload_list->cfg  = offload_config;
        offload_list->fns  = offload_functions;
        offload_list->ol   = offload_context;
    }

    return result;
}

#if PACKET_FILTER_OFFLOAD
/*******************************************************************************
* Function Name: mark_end_of_packet_filter_configuration
********************************************************************************
* Summary:
*  Marks the feature type with CY_PF_OL_FEAT_LAST in order to identify the last
*  configuration by the offload manager.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void mark_end_of_packet_filter_configuration(void)
{
    uint32_t index = 0;

    /* Go to end of packet filter configuration list. */
    GO_TO_LAST_FILTER_INDEX(index);

    /* Prevent buffer overrun. */
    if (MAX_PACKET_FILTER >= (index + 1))
    {
        packet_filter_offload_config[index].feature = CY_PF_OL_FEAT_LAST;
    }
    else
    {
        ERR_INFO(("Failed to mark end of packet filter configuration.\n"));
    }
}

/*******************************************************************************
* Function Name: configure_eth_type_packet_filter
********************************************************************************
* Summary:
*  Configures network layer packet filter. This is the 16-bit EtherType field
*  present in the ethernet packets. Valid EtherType consist of a 16-bit number
*  greater than or equal to 0x800.
*
* Parameters:
*  eth_type: EtherType packet.
*
* Return:
*  void
*
*******************************************************************************/
void configure_eth_type_packet_filter(uint16_t eth_type)
{
    uint32_t index = 0;

    /* Get valid index for configuring the packet filter. */
    GO_TO_LAST_FILTER_INDEX(index);

    /* Prevent buffer overrun. Packet filter configuration can take the
     * index upto (MAX_PACKET_FILTER - 1).
     */
    if (MAX_PACKET_FILTER > (index + 1))
    {
        APP_INFO(("----Eth Filter----\r\nType: 0x%02x\n", eth_type));
        packet_filter_offload_config[index].id = index;
        packet_filter_offload_config[index].feature = CY_PF_OL_FEAT_ETHTYPE;
        packet_filter_offload_config[index].bits = (CY_PF_ACTIVE_SLEEP | CY_PF_ACTIVE_WAKE);
        packet_filter_offload_config[index].u.eth.eth_type = eth_type;
    }
    else
    {
        ERR_INFO(("Failed to add Eth filter configuration.\n"));
    }
}

/*******************************************************************************
* Function Name: configure_port_type_packet_filter
********************************************************************************
* Summary:
*  Configures the application packet types based on the port number. They are
*  used to identify TCP and UDP packets based on source port or destination
*  port.
*
* Parameters:
*  port_num  : PortType packet. It represents a port number.
*  direction : Source port or Destination port.
*  protocol  : TCP or UDP protocol.
*
* Return:
*  void
*
*******************************************************************************/
void configure_port_type_packet_filter(uint16_t port_num,
                                       cy_pn_direction_t direction,
                                       cy_pf_proto_t protocol)
{
    uint32_t index = 0;

    /* Get valid index for configuring the packet filter. */
    GO_TO_LAST_FILTER_INDEX(index);

    /* Prevent buffer overrun. Packet filter configuration can take the
     * index upto (MAX_PACKET_FILTER - 1).
     */
    if (MAX_PACKET_FILTER > (index + 1))
    {
        APP_INFO(("----Port Filter----\r\nPort number: %d, Direction: %s, Protocol: %s\n",
                                                                                 port_num,
                                                         ((direction == PF_PN_PORT_DEST) ?
                                                      "Destination Port" : "Source Port"),
                                                       ((protocol == CY_PF_PROTOCOL_TCP) ?
                                                                         "TCP" : "UDP")));
        packet_filter_offload_config[index].id = index;
        packet_filter_offload_config[index].feature = CY_PF_OL_FEAT_PORTNUM;
        packet_filter_offload_config[index].bits = (CY_PF_ACTIVE_SLEEP | CY_PF_ACTIVE_WAKE);
        packet_filter_offload_config[index].u.pf.portnum.portnum = port_num;
        packet_filter_offload_config[index].u.pf.portnum.range = 0;
        packet_filter_offload_config[index].u.pf.portnum.direction = direction;
        packet_filter_offload_config[index].u.pf.proto = protocol;
    }
    else
    {
        ERR_INFO(("Failed to add Port filter configuration.\n"));
    }
}
#endif /* PACKET_FILTER_OFFLOAD */

/*******************************************************************************
* Function Name: olm_apply_offload_configuration
********************************************************************************
* Summary:
*  Adds the various WLAN offloads such as ARP, Packet Filter, and TCP Keepalive
*  to the OLM and restarts it in order to apply the new configuration.
*  Note that, any updates to the OLM configuration should be made when the Wi-Fi
*  is in disconnected state. Enable or disable the offload type macro in the file
*  wifi_offload.h.
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t: Returns CY_RSLT_SUCCESS if the offload configuration has been
*  applied to the OLM successfully.
*
*******************************************************************************/
cy_rslt_t olm_apply_offload_configuration(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Add the various offload configuration to the offload manager (OLM) list. */
#if ARP_OFFLOAD
    APP_INFO(("Applying ARP offload (Peer Auto Reply) configuration to the offload manager.\n"));
    result = add_offload_configuration_to_olm_list((const char *)ARP_NAME,
                                                   (const void *)&arp_offload_config,
                                                   (const struct ol_fns *)&arp_ol_fns,
                                                   (void *)&arpol_context);
    PRINT_AND_ASSERT(result, "Failed to add ARP offload configuration to the OLM list.\n");
#endif

    /* Allow all the packet types required for the application. Any packet
     * types which are not part of the below list will get blocked/filtered
     * by the WLAN device. So the PSoC 6 MCU can stay in deep sleep power
     * mode and wake up only for the intended packets.
     */
#if PACKET_FILTER_OFFLOAD
    APP_INFO(("Applying Packet Filter offload configuration to the OLM.\n"));

    /* All the below packet filters are necessary for a basic Wi-Fi connection
     * to establish successfully.
     */
    configure_eth_type_packet_filter(ETH_TYPE_ARP_PACKET);
    configure_eth_type_packet_filter(ETH_TYPE_8021X_PACKET);
    configure_port_type_packet_filter(PORT_TYPE_DHCP_UDP, PF_PN_PORT_DEST, CY_PF_PROTOCOL_UDP);
    configure_port_type_packet_filter(PORT_TYPE_DNS_UDP, PF_PN_PORT_DEST, CY_PF_PROTOCOL_UDP);

    /* Allow all the TCP ports used in this application. They are necessary for establishing
     * TCP socket connection with the remote TCP server.
     */
    configure_port_type_packet_filter(PORT_TYPE_TCP_CLIENT, PF_PN_PORT_SOURCE, CY_PF_PROTOCOL_TCP);
    configure_port_type_packet_filter(PORT_TYPE_TCP_SERVER, PF_PN_PORT_DEST, CY_PF_PROTOCOL_TCP);
    configure_port_type_packet_filter(PORT_TYPE_TCP_CLIENT, PF_PN_PORT_DEST, CY_PF_PROTOCOL_TCP);
    configure_port_type_packet_filter(PORT_TYPE_TCP_SERVER, PF_PN_PORT_SOURCE, CY_PF_PROTOCOL_TCP);

    /* Marks end of packet filter as the Last feature type. Offload manager (OLM) requires
     * this to identify the end of offload configuration.
     */
    mark_end_of_packet_filter_configuration();

    /* Adds the packet filters to the OLM list. */
    result = add_offload_configuration_to_olm_list((const char *)PKT_FILTER_NAME,
                                                   (const void *)&packet_filter_offload_config,
                                                   (const struct ol_fns *)&pf_ol_fns,
                                                   (void *)&pfol_context);
    PRINT_AND_ASSERT(result, "Failed to add Packet Filter offload configuration to the OLM list.\n");
#endif

#if TCP_KEEPALIVE_OFFLOAD
    APP_INFO(("Applying TCP Keepalive offload configuration to the OLM.\n"));
    result = add_offload_configuration_to_olm_list((const char *)TKO_NAME,
                                                   (const void *)&tcp_keepalive_offload_config,
                                                   (const struct ol_fns *)&tko_ol_fns,
                                                   (void *)&tkol_context);
    PRINT_AND_ASSERT(result, "Failed to add TCP Keepalive offload configuration to the OLM list.\n");
#endif

    /* Applies the new configuration list to the offload manager and
     * reinitializes with the new configuration.
     */
    result = cylpa_restart_olm(user_configuration_list, NULL);

    return result;
}

/*******************************************************************************
* Function Name: RunApplicationTask
********************************************************************************
* Summary:
*  Suspends the lwIP network stack forever. This allows the PSoC 6 MCU to enter
*  into deep sleep power mode. The Host wakes up only when any network activity
*  on Tx/Rx path is detected in the WLAN driver interface.
*
* Parameters:
*  pArgument: Unused.
*
* Return:
*  void
*
*******************************************************************************/
void RunApplicationTask(void *pArgument)
{
    struct netif *wifi;

    (void)pArgument;

    /* Obtain the reference to the lwIP network interface. This will be
     * used to access the Wi-Fi driver interface to configure the WLAN
     * power save mode.
     */
    wifi = cy_lwip_get_interface();

    while (true)
    {

        /* Configures an emac activity callback to the Wi-Fi interface and
         * suspends the network stack if the network is inactive for a duration
         * of INACTIVE_WINDOW_MS inside an interval of INACTIVE_INTERVAL_MS.
         * The callback is used to signal the presence/absence of network activity
         * to resume/suspend the network stack.
         */
        wait_net_suspend(wifi, portMAX_DELAY, INACTIVE_INTERVAL_MS, INACTIVE_WINDOW_MS);
        vTaskDelay(pdMS_TO_TICKS(NETWORK_SUSPEND_DELAY_MS));
    }
}

/************************************************************************************
 * Function Name: tcp_socket_connection_start
 ************************************************************************************
 * Summary:
 *  Establishes TCP socket connection with the TCP server. Maximum up to MAX_TKO_CONN
 *  number of connections are allowed. The value defaults to 4 as queried from the
 *  WLAN firmware by the LPA middleware.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t: Returns CY_RSLT_SUCCESS if the connection is successfully created
 *  for all the configured sockets, a socket error code otherwise.
 *
 ***********************************************************************************/
cy_rslt_t tcp_socket_connection_start(void)
{
    int index = 0;
    const cy_tko_ol_cfg_t *downloaded = NULL;
    cy_rslt_t result = CY_RSLT_SUCCESS;
    const cy_tko_ol_connect_t *port;
    struct netif *netif = cy_lwip_get_interface();
    cy_rslt_t socket_connection_status = CY_RSLT_SUCCESS;
#if (USE_CONFIGURATOR_GENERATED_CONFIG)
    ol_desc_t *configurator_offload_list = NULL;
#endif

    if (pdPASS != SOCKETS_Init())
    {
        ERR_INFO(("%s: Socket initialization failed.\n", __func__));
        CY_ASSERT(0);
    }

#if (USE_CONFIGURATOR_GENERATED_CONFIG)
    /* Takes device configurator generated configuration. */
    configurator_offload_list = cylpa_find_my_descriptor(TKO_NAME, (ol_desc_t *)get_default_ol_list());
    if (NULL != configurator_offload_list)
    {
        downloaded = (const cy_tko_ol_cfg_t *)configurator_offload_list->cfg;
    }
    else
    {
        APP_INFO(("TCP Keepalive offload is not enabled by the user "
                                    "in the device configurator.\n"));
    }
#else
    /* Takes reference to the local TCP Keepalive configuration defined by the user. */
    downloaded = &tcp_keepalive_offload_config;
#endif

    if (NULL != downloaded)
    {
        /* Offload descriptor was found. Start TCP socket connection to the
         * configured TCP server.
         */
        for (index = 0; index < MAX_TKO_CONN; index++)
        {
             port = &downloaded->ports[index];

             if ((strcmp(port->remote_ip, NULL_IP_ADDRESS) != 0) &&
                 (port->remote_port > 0) &&
                 (port->local_port > 0))
             {
                 /* Configures TCP Keepalive with the given remote TCP server.
                  * This is a helper function provided by the Low Power Assistant (LPA)
                  * middleware, which helps to create a socket, bind to the socket, and
                  * then establishes TCP connection with the given remote TCP server.
                  * Enable(1) or Disable(0) the Host TCP keepalive (or lwIP TCP Keepalive)
                  * using the macro ENABLE_HOST_TCP_KEEPALIVE.
                  */
                 result = cy_tcp_create_socket_connection(netif,
                                                          (void **)&global_socket[index],
                                                          port->remote_ip,
                                                          port->remote_port,
                                                          port->local_port,
                                                          (cy_tko_ol_cfg_t *)downloaded,
                                                          ENABLE_HOST_TCP_KEEPALIVE);

                 if (CY_RSLT_SUCCESS != result)
                 {
                     ERR_INFO(("Socket[%d]: Unable to connect. TCP Server IP: %s, Local Port: %d, "
                                     "Remote Port: %d\n", index, port->remote_ip, port->local_port,
                                                                               port->remote_port));
                     socket_connection_status = result;
                 }
                 else
                 {
                     APP_INFO(("Socket[%d]: Created connection to IP %s, local port %d, remote port %d\n",
                                          index, port->remote_ip, port->local_port, port->remote_port));
                 }
             }
             else
             {
                 APP_INFO(("Skipped TCP socket connection for socket id[%d]. Check the TCP Keepalive "
                                                                        "configuration.\n", index));
             }
        }
    }
#if !(USE_CONFIGURATOR_GENERATED_CONFIG)
    else
    {
        ERR_INFO(("%s: Offload descriptor %s not found. No TCP connection has been established.\n", __func__, "TKO"));
        socket_connection_status = CY_RSLT_TYPE_ERROR;
    }
#else
    else
    {
        /* TCP Keepalive is not enabled in the device configurator and no socket connection is expected
         * to be established.
         */
        socket_connection_status = CY_RSLT_SUCCESS;
    }
#endif

    return socket_connection_status;
}

/*******************************************************************************
 * Function Name: prvWifiConnect
 *******************************************************************************
 * Summary:
 *  Connects to the Wi-Fi Access Point. The AP network parameters such as
 *  WIFI_SSID, WIFI_PASSWORD, and WIFI_SECURITY needs to be configured by
 *  the user in the lowpower_task.h file.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t: Returns CY_RSLT_SUCCESS if the Wi-Fi connection is successfully
 *  established. Otherwise, it returns CY_RSLT_TYPE_ERROR.
 *
 ******************************************************************************/
cy_rslt_t prvWifiConnect(void)
{
    WIFINetworkParams_t  xNetworkParams;
    WIFIReturnCode_t xWifiStatus;
    uint8_t ip_addr[4] = {0};
    uint32_t retry_count = 0;
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Setup Wi-Fi network parameters. */
    xNetworkParams.pcSSID = WIFI_SSID;
    xNetworkParams.ucSSIDLength = sizeof(WIFI_SSID);
    xNetworkParams.pcPassword = WIFI_PASSWORD;
    xNetworkParams.ucPasswordLength = sizeof(WIFI_PASSWORD);
    xNetworkParams.xSecurity = WIFI_SECURITY;

    APP_INFO(("Wi-Fi module initialized. Connecting to AP: %s\n", xNetworkParams.pcSSID));

    /* Connect to Access Point */
    for (retry_count = 0; retry_count < MAX_WIFI_RETRY_COUNT; retry_count++)
    {
        xWifiStatus = WIFI_ConnectAP(&(xNetworkParams));

        if (eWiFiSuccess == xWifiStatus)
        {
            APP_INFO(("Wi-Fi connected to AP: %s\n", xNetworkParams.pcSSID));

            xWifiStatus = WIFI_GetIP(ip_addr);

            if (eWiFiSuccess == xWifiStatus)
            {
                APP_INFO(("IP Address acquired: %s\n", ip4addr_ntoa((const ip4_addr_t *)&ip_addr)));
            }
            else
            {
                ERR_INFO(("Failed to get IP address.\n"));
            }

            break;
        }

        ERR_INFO(("Failed to join Wi-Fi network. Retrying...\n"));
    }

    if (eWiFiSuccess != xWifiStatus)
    {
        result = CY_RSLT_TYPE_ERROR;
    }

    return result;
}


/* [] END OF FILE */

