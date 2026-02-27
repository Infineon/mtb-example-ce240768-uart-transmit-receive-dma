/******************************************************************************
* File Name:   main.c
*
* Description: This example demonstrates the UART transmit and receive
*              operation using DMA.
*
* Related Document: See README.md
*
*
*******************************************************************************
 * (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 * This software, associated documentation and materials ("Software") is
 * owned by Infineon Technologies AG or one of its affiliates ("Infineon")
 * and is protected by and subject to worldwide patent protection, worldwide
 * copyright laws, and international treaty provisions. Therefore, you may use
 * this Software only as provided in the license agreement accompanying the
 * software package from which you obtained this Software. If no license
 * agreement applies, then any use, reproduction, modification, translation, or
 * compilation of this Software is prohibited without the express written
 * permission of Infineon.
 *
 * Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
 * IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
 * THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
 * SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
 * Infineon reserves the right to make changes to the Software without notice.
 * You are responsible for properly designing, programming, and testing the
 * functionality and safety of your intended application of the Software, as
 * well as complying with any legal requirements related to its use. Infineon
 * does not guarantee that the Software will be free from intrusion, data theft
 * or loss, or other breaches ("Security Breaches"), and Infineon shall have
 * no liability arising out of any Security Breaches. Unless otherwise
 * explicitly approved by Infineon, the Software may not be used in any
 * application where a failure of the Product or any consequences of the use
 * thereof can reasonably be expected to result in personal injury.
*******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "UartDma.h"

/*******************************************************************************
*            Constants
*******************************************************************************/

/*******************************************************************************
*            Global variables
*******************************************************************************/

uint8_t rx_dma_error;   /* RxDma error flag */
uint8_t tx_dma_error;   /* TxDma error flag */
uint8_t uart_error;     /* UART error flag */
uint8_t rx_dma_done;    /* RxDma done flag */

CY_SECTION_SHAREDMEM uint8_t rx_dma_uart_buffer_a[BUFFER_SIZE];
CY_SECTION_SHAREDMEM uint8_t rx_dma_uart_buffer_b[BUFFER_SIZE];

/*******************************************************************************
*            Forward declaration
*******************************************************************************/

void handle_error(void);
void Isr_UART(void);

/*******************************************************************************
* Function Name: main
********************************************************************************
*
* Summary:
* The main function performs the following actions:
*  1. Configures RX and TX DMAs to handle UART RX+TX direction.
*  2. Configures UART component.
*  3. Initial UART component.
*  4. Sends text header to the UART serial terminal.
*  5. Waits in an infinite loop (for DMA or UART error interrupt)
*
* Parameters:
*  None
*
* Return:
*  int
*
*******************************************************************************/

int main(void)
{
    cy_rslt_t result;
    cy_en_scb_uart_status_t init_status;
    cy_stc_scb_uart_context_t KIT_UART_context;
    uint32_t active_descr = DMA_DESCR0; /* flag to control which descriptor to use */

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    cy_stc_sysint_t KIT_UART_INT_cfg =
    {
        .intrSrc = ( (NvicMux3_IRQn << 16) | KIT_UART_IRQ ),
        .intrPriority = 7u,
    };

    cy_stc_sysint_t RX_DMA_INT_cfg =
    {
        .intrSrc      = ( (NvicMux3_IRQn << 16) | (IRQn_Type)RxDma_IRQ ),
        .intrPriority = 6u,
    };

    cy_stc_sysint_t TX_DMA_INT_cfg =
    {
        .intrSrc      = ( (NvicMux2_IRQn << 16) | (IRQn_Type)TxDma_IRQ ),
        .intrPriority = 6u,
    };

    /* Configure DMA Rx and Tx channels for operation */
    configure_rx_dma(rx_dma_uart_buffer_a, rx_dma_uart_buffer_b, &RX_DMA_INT_cfg);
    configure_tx_dma(rx_dma_uart_buffer_a, &TX_DMA_INT_cfg);

    /* Initialize and enable interrupt from UART. The UART interrupt sources
    *  are enabled in the Component GUI */
    Cy_SysInt_Init(&KIT_UART_INT_cfg, &Isr_UART);
    NVIC_EnableIRQ((IRQn_Type) NvicMux3_IRQn);

    /* Start UART operation */
    init_status = Cy_SCB_UART_Init(KIT_UART_HW, &KIT_UART_config, &KIT_UART_context);
    if (init_status!=CY_SCB_UART_SUCCESS)
    {
        handle_error();
    }
    Cy_SCB_UART_Enable(KIT_UART_HW);

    /* Transmit header to the terminal */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    Cy_SCB_UART_PutString(KIT_UART_HW, "\x1b[2J\x1b[;H");

    Cy_SCB_UART_PutString(KIT_UART_HW, "************************************************************\r\n");
    Cy_SCB_UART_PutString(KIT_UART_HW, "UART Transmit and Receive using DMA\r\n");
    Cy_SCB_UART_PutString(KIT_UART_HW, "************************************************************\r\n\n");
    Cy_SCB_UART_PutString(KIT_UART_HW, ">> Start typing to see the echo on the screen \r\n\n");

    /* Initialize flags */
    rx_dma_error = 0;
    tx_dma_error = 0;
    uart_error = 0;
    rx_dma_done = 0;

    /* Disable D cache because DMA also reads descriptor in the SRAM */
#if defined (CY_DEVICE_TVIIC2D6M) || defined (CY_DEVICE_TVIIC2D4M)
#else
        SCB_DisableDCache();
#endif

    __enable_irq();
 
    while (1)
    {
        /* Indicate status if RxDma error or TxDma error or UART error occurs */
        if ((rx_dma_error==1) | (tx_dma_error==1) | (uart_error==1))
        {
            handle_error();
        }

        /* Handle RxDma complete */
        if (rx_dma_done==1)
        {
            /* Ping Pong between rx_dma_uart_buffer_a and rx_dma_uart_buffer_b */
            /* Ping Pong buffers give firmware time to pull the data out of one or the other buffer */
            if (DMA_DESCR0 == active_descr)
            {
                /* Set source RX Buffer A as source for TxDMA */
                Cy_DMA_Descriptor_SetSrcAddress(&TxDma_Descriptor_0, (uint32_t *) rx_dma_uart_buffer_a);
                active_descr = DMA_DESCR1;
            }
            else
            {
                /* Set source RX Buffer B as source for TxDMA */
                Cy_DMA_Descriptor_SetSrcAddress(&TxDma_Descriptor_0, (uint32_t *) rx_dma_uart_buffer_b);
                active_descr = DMA_DESCR0;
            }

            SCB_CleanDCache_by_Addr((uint32_t*)&TxDma_Descriptor_0, (int32_t)sizeof(TxDma_Descriptor_0));

            Cy_DMA_Channel_SetDescriptor(TxDma_HW, TxDma_CHANNEL, &TxDma_Descriptor_0);
            Cy_DMA_Channel_Enable(TxDma_HW, TxDma_CHANNEL);
            rx_dma_done = 0;
        }
    }
}

/*******************************************************************************
* Function Name: handle_error
********************************************************************************
* Summary:
* User defined error handling function
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void handle_error(void)
{
     /* Disable all interrupts. */
    __disable_irq();

    CY_ASSERT(0);
}

/*******************************************************************************
* Function Name: Isr_UART
********************************************************************************
*
* Summary:
* Handles UART Rx underflow and overflow conditions. This conditions must never
* occur.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void Isr_UART(void)
{
    uint32_t intrSrcRx;
    uint32_t intrSrcTx;

    /* Get RX interrupt sources */
    intrSrcRx = Cy_SCB_UART_GetRxFifoStatus(KIT_UART_HW);
    Cy_SCB_UART_ClearRxFifoStatus(KIT_UART_HW, intrSrcRx);

    /* Get TX interrupt sources */
    intrSrcTx = Cy_SCB_UART_GetTxFifoStatus(KIT_UART_HW);
    Cy_SCB_UART_ClearTxFifoStatus(KIT_UART_HW, intrSrcTx);

    /* RX overflow or RX underflow or RX overflow occured */
    uart_error = 1;
}