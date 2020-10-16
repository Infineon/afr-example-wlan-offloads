/******************************************************************************
* File Name: main.c
*
* Description: Demonstrates various WLAN offloads supported by the Low Power
* Assistant (LPA) middleware library.
*
* Related Document: See README.md
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
*******************************************************************************/

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

#ifdef CY_USE_LWIP
#include "lwip/tcpip.h"
#endif

/* AWS library includes. */
#include "iot_system_init.h"
#include "iot_logging_task.h"
#include "iot_wifi.h"

/* BSP & HAL includes. */
#include "cybsp.h"
#include "cyhal.h"
#include "cy_retarget_io.h"

/* LPA offload configuration includes. */
#include "wlan_offload.h"

/* For print macro expansion. */
#include "wifi_config.h"

/* Logging Task Defines. */
#define mainLOGGING_MESSAGE_QUEUE_LENGTH    ( 200 )
#define mainLOGGING_TASK_STACK_SIZE         ( configMINIMAL_STACK_SIZE * 8 )

/* The task delay for allowing the lower priority logging task to print out Wi-Fi
 * failure status before blocking indefinitely.
 */
#define mainLOGGING_WIFI_STATUS_DELAY       pdMS_TO_TICKS( 1000 )

/* The name of the devices for xApplicationDNSQueryHook. */
#define mainDEVICE_NICK_NAME                "wlan_offloads"

/**
 * @brief Application task startup hook for applications using Wi-Fi. If you are not
 * using Wi-Fi, then start network dependent applications in the vApplicationIPNetorkEventHook
 * function. If you are not using Wi-Fi, this hook can be disabled by setting
 * configUSE_DAEMON_TASK_STARTUP_HOOK to 0.
 */
void vApplicationDaemonTaskStartupHook(void);

/**
 * @brief Initializes the board.
 */
static void prvMiscInitialization(void);

/**
 * @brief Initialize the user application.
 */
void InitApplication(void);

extern int uxTopUsedPriority;

/*-----------------------------------------------------------*/

/**
 * @brief Application runtime entry point.
 */
int main(void)
{
    /* Performs any hardware initialization that does not
     * require the RTOS to be running.
     */
    prvMiscInitialization();

    /* Create tasks that are not dependent on the Wi-Fi being initialized. */
    xLoggingTaskInitialize(mainLOGGING_TASK_STACK_SIZE,
                           tskIDLE_PRIORITY,
                           mainLOGGING_MESSAGE_QUEUE_LENGTH);

    /* Start the scheduler. Initialization that requires the OS to be running,
     * including the Wi-Fi initialization, is performed in the RTOS daemon task
     * startup hook.
     */
    vTaskStartScheduler();

    return 0;
}
/*-----------------------------------------------------------*/

static void prvMiscInitialization(void)
{
    cy_rslt_t result = cybsp_init();

    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0);
    }

    /* Initializes UART pins for logging on the serial terminal. */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    if (CY_RSLT_SUCCESS != result)
    {
        CY_ASSERT(0);
    }
}
/*-----------------------------------------------------------*/

void vApplicationDaemonTaskStartupHook(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Need to export this var to elf so that debugger can work properly */
    (void)uxTopUsedPriority;

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen */
    APP_INFO(("\x1b[2J\x1b[;H"));
    APP_INFO(("================================================\n"));
    APP_INFO(("AWS IoT and FreeRTOS for PSoC 6: WLAN Offloads\n"));
    APP_INFO(("================================================\n\n"));

    /* Initialize secure sockets */
    if(SYSTEM_Init() == pdPASS)
    {
#ifdef CY_USE_LWIP
        /* 
         * Initializes the lwIP stack. This needs the RTOS to be up
         * since this function will spawn the tcp_ip thread.
         */
        tcpip_init(NULL, NULL);
#endif

        /* Initializes the Wi-Fi interface.
         * The Wi-Fi interface needs to be only initialized but not connected to the
         * Access Point before applying the WLAN offload configuration to the Offload
         * manager.
         */
        if (eWiFiSuccess != WIFI_On())
        {
            PRINT_AND_ASSERT(CY_RSLT_TYPE_ERROR, "Failed to initialize the Wi-Fi interface.");
        }

#if !(USE_CONFIGURATOR_GENERATED_CONFIG)
        /* 
         * Configures the offload list and applies them to the offload manager (OLM).
         * Note that, the offload configuration list should be applied to the OLM before
         * the Wi-Fi associates to the Access Point. If the Wi-Fi is already associated,
         * disconnect it, apply the offload configuration list to the OLM, and then
         * reassociate to the AP.
         */
        result = olm_apply_offload_configuration();
        PRINT_AND_ASSERT(result, "Failed to apply the offload configuration.\n");
#else
        APP_INFO(("--Offload Manager is initialized with the device configurator generated configuration--\n"));
#endif

        /*
         * Connect to Wi-Fi Access Point.
         */
        result = prvWifiConnect();
        PRINT_AND_ASSERT(result, "Wi-Fi connection failed.\n");

        /*
         * Establishes TCP socket connection with the configured TCP server.
         * Ensure the remote TCP server has already started before running this application.
         */
        result = tcp_socket_connection_start();
        if (CY_RSLT_SUCCESS != result)
        {
            ERR_INFO(("One or more TCP socket connections failed.\n"));

            /*
             * Wait for few seconds to let the user notice about the socket
             * connection failure before the network stats start to print
             * on the serial terminal that would cause the user to miss seeing
             * the error message.
             */
            vTaskDelay(pdMS_TO_TICKS(TCP_SOCKET_ERROR_DELAY_MS));
        }

        /*
         * Suspends/Resumes the lwIP network stack.
         * This task will cause PSoC 6 MCU to go into deep sleep power mode.
         * The PSoC 6 MCU wakes up only when the network stack resumes due
         * to Tx/Rx activity detected by the Offload Manager (OLM).
         */
        InitApplication();
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief User defined Idle task function.
 *
 * @note Do not make any blocking operations in this function.
 */
void vApplicationIdleHook(void)
{
    /* FIX ME. If necessary, update to application idle periodic actions. */

    static TickType_t xLastPrint = 0;
    TickType_t xTimeNow;
    const TickType_t xPrintFrequency = pdMS_TO_TICKS(5000);

    xTimeNow = xTaskGetTickCount();

    if((xTimeNow - xLastPrint) > xPrintFrequency)
    {
        xLastPrint = xTimeNow;
    }
}

void vApplicationTickHook()
{
}
/*-----------------------------------------------------------*/

/**
* @brief User defined application hook to process names returned by the DNS server.
*/
#if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 )
    BaseType_t xApplicationDNSQueryHook(const char *pcName)
    {
        /* FIX ME. If necessary, update to applicable DNS name lookup actions. */

        BaseType_t xReturn;

        /* Determine if a name lookup is for this node.  Two names are given
         * to this node: that returned by pcApplicationHostnameHook() and that set
         * by mainDEVICE_NICK_NAME. */
        if(strcmp(pcName, pcApplicationHostnameHook()) == 0)
        {
            xReturn = pdPASS;
        }
        else if(strcmp(pcName, mainDEVICE_NICK_NAME) == 0)
        {
            xReturn = pdPASS;
        }
        else
        {
            xReturn = pdFAIL;
        }

        return xReturn;
    }

#endif /* if ( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 ) */
/*-----------------------------------------------------------*/

/**
 * @brief User defined assertion call. This function is plugged into configASSERT.
 * See FreeRTOSConfig.h to define configASSERT to something different.
 */
void vAssertCalled(const char *pcFile, uint32_t ulLine)
{
    /* FIX ME. If necessary, update to applicable assertion routine actions. */

    const uint32_t ulLongSleep = 1000UL;
    volatile uint32_t ulBlockVariable = 0UL;
    volatile char * pcFileName = (volatile char *)pcFile;
    volatile uint32_t ulLineNumber = ulLine;

    (void)pcFileName;
    (void)ulLineNumber;

    ERR_INFO(("vAssertCalled %s, %ld\n", pcFile, (long)ulLine));
    fflush(stdout);

    /* Setting ulBlockVariable to a non-zero value in the debugger will allow
     * this function to be exited.
     */
    taskDISABLE_INTERRUPTS();
    {
        while (ulBlockVariable == 0UL)
        {
            vTaskDelay(pdMS_TO_TICKS(ulLongSleep));
        }
    }
    taskENABLE_INTERRUPTS();
}
/*-----------------------------------------------------------*/

/**
 * @brief Warn user if pvPortMalloc fails.
 *
 * Called if a call to pvPortMalloc() fails because there is insufficient
 * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
 * internally by FreeRTOS API functions that create tasks, queues, software
 * timers, and semaphores.  The size of the FreeRTOS heap is set by the
 * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h.
 *
 */
void vApplicationMallocFailedHook()
{
    ERR_INFO(("Malloc failed to allocate memory\r\n"));
    taskDISABLE_INTERRUPTS();

    /* Loop forever */
    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/

/**
 * @brief Loop forever if stack overflow is detected.
 *
 * If configCHECK_FOR_STACK_OVERFLOW is set to 1,
 * this hook provides a location for applications to
 * define a response to a stack overflow.
 *
 * Use this hook to help identify that a stack overflow
 * has occurred.
 *
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    ERR_INFO(("stack overflow with task %s\r\n", pcTaskName));
    portDISABLE_INTERRUPTS();

    /* Unused Parameters */
    (void) xTask;

    /* Loop forever */
    for( ; ; )
    {
    }
}
/*-----------------------------------------------------------*/

