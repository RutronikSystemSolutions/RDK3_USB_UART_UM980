/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the RDK3 USB UART UM980
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*
*  Created on: 2023-11-24
*  Company: Rutronik Elektronische Bauelemente GmbH
*  Address: Jonavos g. 30, Kaunas 44262, Lithuania
*  Author: GDR
*
*******************************************************************************
* (c) 2019-2021, Cypress Semiconductor Corporation. All rights reserved.
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
*
* Rutronik Elektronische Bauelemente GmbH Disclaimer: The evaluation board
* including the software is for testing purposes only and,
* because it has limited functions and limited resilience, is not suitable
* for permanent use under real conditions. If the evaluation board is
* nevertheless used under real conditions, this is done at one’s responsibility;
* any liability of Rutronik is insofar excluded
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"

/*Application Definitions*/
#define DATA_BITS_8     		8
#define STOP_BITS_1     		1
#define BAUD_RATE       		115200
#define KITPROG_INT_PRIORITY    3
#define ARDUINO_INT_PRIORITY    2
#define KITPROG_RX_BUF_SIZE		1024
#define ARDUINO_RX_BUF_SIZE		1024
#define LED_INDICATION_TIME		10000

/*Function prototypes used for this demo.*/
void kitprog_uart_event_handler(void *handler_arg, cyhal_uart_event_t event);
void arduino_uart_event_handler(void *handler_arg, cyhal_uart_event_t event);

/*Global Variables for KITPROG/ARDUINO UART Bridge*/
cyhal_uart_t kitprog_uart_obj;
cyhal_uart_t arduino_uart_obj;
uint8_t kitprog_rx_buf[KITPROG_RX_BUF_SIZE] = {0};
uint8_t arduino_rx_buf[ARDUINO_RX_BUF_SIZE] = {0};
uint32_t kitprog_rx_led_activate = 0;
uint32_t arduino_rx_led_activate = 0;

/* Initialize the KITPROG UART configuration structure */
const cyhal_uart_cfg_t kitprog_uart_config =
{
    .data_bits = DATA_BITS_8,
    .stop_bits = STOP_BITS_1,
    .parity = CYHAL_UART_PARITY_NONE,
    .rx_buffer = NULL,
    .rx_buffer_size = 0
};

/* Initialize the ARDUINO UART configuration structure */
const cyhal_uart_cfg_t arduino_uart_config =
{
    .data_bits = DATA_BITS_8,
    .stop_bits = STOP_BITS_1,
    .parity = CYHAL_UART_PARITY_NONE,
    .rx_buffer = NULL,
    .rx_buffer_size = 0
};

int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
    	CY_ASSERT(0);
    }

    __enable_irq();

    /*Initialize LEDs*/
    result = cyhal_gpio_init( LED1, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}
    result = cyhal_gpio_init( LED2, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}
    result = cyhal_gpio_init( LED3, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /*Initialize and Disable Charger Control pin*/
    result = cyhal_gpio_init(CHR_DIS, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /*Initialize UM980 POWER_N pin*/
    result = cyhal_gpio_init(ARDU_IO7, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, false);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /*Initialize UM980 RESET_N pin*/
    result = cyhal_gpio_init(ARDU_IO8, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_OPENDRAINDRIVESLOW, true);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /*Reset the UM980*/
    cyhal_gpio_write(ARDU_IO8, false);
    CyDelay(100);
    cyhal_gpio_write(ARDU_IO8, true);

    /*Initialize LE910C1 POWER_N pin*/
    result = cyhal_gpio_init(ARDU_IO6, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, true);
    if (result != CY_RSLT_SUCCESS)
    {CY_ASSERT(0);}

    /* Initialize the KITPROG UART Block */
    result = cyhal_uart_init(&kitprog_uart_obj, KITPROG_TX, KITPROG_RX, NC, NC, NULL, &kitprog_uart_config);
    CY_ASSERT(CY_RSLT_SUCCESS == result);

    /* The KITPROG UART callback handler registration */
    cyhal_uart_register_callback(&kitprog_uart_obj, kitprog_uart_event_handler, NULL);

    /* Enable required KITPROG UART events */
    cyhal_uart_enable_event(&kitprog_uart_obj, (cyhal_uart_event_t)(CYHAL_UART_IRQ_RX_NOT_EMPTY | CYHAL_UART_IRQ_RX_ERROR), KITPROG_INT_PRIORITY, true);

    /* Initialize the ARDUINO UART Block */
    result = cyhal_uart_init(&arduino_uart_obj, ARDU_TX, ARDU_RX, NC, NC, NULL, &arduino_uart_config);
    CY_ASSERT(CY_RSLT_SUCCESS == result);

    /* The ARDUINO UART callback handler registration */
    cyhal_uart_register_callback(&arduino_uart_obj, arduino_uart_event_handler, NULL);

    /* Enable required ARDUINO UART events */
    cyhal_uart_enable_event(&arduino_uart_obj, (cyhal_uart_event_t)(CYHAL_UART_IRQ_RX_NOT_EMPTY | CYHAL_UART_IRQ_RX_ERROR), ARDUINO_INT_PRIORITY, true);

    for (;;)
    {
    	/*Blink for RX*/
    	if(kitprog_rx_led_activate)
    	{
    		kitprog_rx_led_activate--;
    		cyhal_gpio_write(LED2, CYBSP_LED_STATE_ON);
    	}
    	else
    	{
    		cyhal_gpio_write(LED2, CYBSP_LED_STATE_OFF);
    	}

    	/*Blink for TX*/
    	if(arduino_rx_led_activate)
    	{
    		arduino_rx_led_activate--;
    		cyhal_gpio_write(LED3, CYBSP_LED_STATE_ON);
    	}
    	else
    	{
    		cyhal_gpio_write(LED3, CYBSP_LED_STATE_OFF);
    	}
    }
}

/* KITPROG Event handler callback function */
void kitprog_uart_event_handler(void *handler_arg, cyhal_uart_event_t event)
{
	uint32_t bytes = 0;
	cy_rslt_t result;
    (void) handler_arg;

    if ((event & CYHAL_UART_IRQ_RX_NOT_EMPTY) == CYHAL_UART_IRQ_RX_NOT_EMPTY)
    {
    	bytes = cyhal_uart_readable(&kitprog_uart_obj);
    	if(bytes)
    	{
    		result = cyhal_uart_read (&kitprog_uart_obj, kitprog_rx_buf, (size_t*)&bytes);
    		if((result == CY_RSLT_SUCCESS) && (bytes > 0))
    		{
    			kitprog_rx_led_activate = LED_INDICATION_TIME;
    			(void)cyhal_uart_write (&arduino_uart_obj, kitprog_rx_buf, (size_t*)&bytes);
    		}
    	}
    }
    else if ((event & CYHAL_UART_IRQ_RX_ERROR) == CYHAL_UART_IRQ_RX_ERROR)
    {
    	(void)cyhal_uart_clear(&kitprog_uart_obj);
    }
}

/* ARDUINO Event handler callback function */
void arduino_uart_event_handler(void *handler_arg, cyhal_uart_event_t event)
{
	uint32_t bytes = 0;
	cy_rslt_t result;
    (void) handler_arg;

    if ((event & CYHAL_UART_IRQ_RX_NOT_EMPTY) == CYHAL_UART_IRQ_RX_NOT_EMPTY)
    {
    	bytes = cyhal_uart_readable(&arduino_uart_obj);
    	if(bytes)
    	{
    		result = cyhal_uart_read (&arduino_uart_obj, arduino_rx_buf, (size_t*)&bytes);
    		if((result == CY_RSLT_SUCCESS) && (bytes > 0))
    		{
    			arduino_rx_led_activate = LED_INDICATION_TIME;
    			(void)cyhal_uart_write (&kitprog_uart_obj, arduino_rx_buf, (size_t*)&bytes);
    		}
    	}
    }
    else if ((event & CYHAL_UART_IRQ_RX_ERROR) == CYHAL_UART_IRQ_RX_ERROR)
    {
    	(void)cyhal_uart_clear(&arduino_uart_obj);
    }
}
/* [] END OF FILE */
