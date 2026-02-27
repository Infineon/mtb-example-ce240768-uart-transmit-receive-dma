/******************************************************************************
* File Name: UartDma.c
*
* Description: This file contains all the functions and variables required for
*              proper operation of UART/DMA for this CE
*
* Related Document: See README.md
*
*******************************************************************************
 * (c) 2022-2026, Infineon Technologies AG, or an affiliate of Infineon
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


#include "UartDma.h"

/*******************************************************************************
*            Forward declaration
*******************************************************************************/
void handle_error(void);

/*******************************************************************************
*            Global variables
*******************************************************************************/
extern uint8_t rx_dma_error;   /* RxDma error flag */
extern uint8_t tx_dma_error;   /* TxDma error flag */
extern uint8_t rx_dma_done;    /* RxDma done flag */

/*******************************************************************************
* Function Name: configure_rx_dma
********************************************************************************
*
* Summary:
* Configures DMA Rx channel for operation.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void configure_rx_dma(uint8_t* buffer_a, uint8_t* buffer_b, cy_stc_sysint_t* int_config)
{
    cy_en_dma_status_t dma_init_status;

    /* Initialize descriptor 1 */
    dma_init_status = Cy_DMA_Descriptor_Init(&RxDma_Descriptor_0, &RxDma_Descriptor_0_config);
    if (dma_init_status!=CY_DMA_SUCCESS)
    {
        handle_error();
    }

    /* Initialize descriptor 2 */
    dma_init_status = Cy_DMA_Descriptor_Init(&RxDma_Descriptor_1, &RxDma_Descriptor_1_config);
    if (dma_init_status!=CY_DMA_SUCCESS)
    {
        handle_error();
    }

    dma_init_status = Cy_DMA_Channel_Init(RxDma_HW, RxDma_CHANNEL, &RxDma_channelConfig);
    if (dma_init_status!=CY_DMA_SUCCESS)
    {
        handle_error();
    }

    /* Set source and destination address for descriptor 1 */
    Cy_DMA_Descriptor_SetSrcAddress(&RxDma_Descriptor_0, (uint32_t *) &KIT_UART_HW->RX_FIFO_RD);
    Cy_DMA_Descriptor_SetDstAddress(&RxDma_Descriptor_0, (uint32_t *) buffer_a);

    /* Set source and destination address for descriptor 2 */
    Cy_DMA_Descriptor_SetSrcAddress(&RxDma_Descriptor_1, (uint32_t *) &KIT_UART_HW->RX_FIFO_RD);
    Cy_DMA_Descriptor_SetDstAddress(&RxDma_Descriptor_1, (uint32_t *) buffer_b);

    Cy_DMA_Channel_SetDescriptor(RxDma_HW, RxDma_CHANNEL, &RxDma_Descriptor_0);

    /* Initialize and enable interrupt from RxDma */
    Cy_SysInt_Init  (int_config, &rx_dma_complete);
    NVIC_EnableIRQ((IRQn_Type) NvicMux3_IRQn);

    /* Enable DMA interrupt source. */
    Cy_DMA_Channel_SetInterruptMask(RxDma_HW, RxDma_CHANNEL, CY_DMA_INTR_MASK);

    /* Enable channel and DMA block to start descriptor execution process */
    Cy_DMA_Channel_Enable(RxDma_HW, RxDma_CHANNEL);
    Cy_DMA_Enable(RxDma_HW);
}

/*******************************************************************************
* Function Name: configure_tx_dma
********************************************************************************
*
* Summary:
* Configures DMA Tx channel for operation.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void configure_tx_dma(uint8_t* buffer_a, cy_stc_sysint_t* int_config)
{
    cy_en_dma_status_t dma_init_status;

    /* Init descriptor */
    dma_init_status = Cy_DMA_Descriptor_Init(&TxDma_Descriptor_0, &TxDma_Descriptor_0_config);
    if (dma_init_status!=CY_DMA_SUCCESS)
    {
        handle_error();
    }
    dma_init_status = Cy_DMA_Channel_Init(TxDma_HW, TxDma_CHANNEL, &TxDma_channelConfig);
    if (dma_init_status!=CY_DMA_SUCCESS)
    {
        handle_error();
    }

    /* Set source and destination for descriptor 1 */
    Cy_DMA_Descriptor_SetSrcAddress(&TxDma_Descriptor_0, (uint32_t *) buffer_a);
    Cy_DMA_Descriptor_SetDstAddress(&TxDma_Descriptor_0, (uint32_t *) &KIT_UART_HW->TX_FIFO_WR);

    /* Set next descriptor to NULL to stop the chain execution after descriptor 1
    *  is completed.
    */
    Cy_DMA_Descriptor_SetNextDescriptor(Cy_DMA_Channel_GetCurrentDescriptor(TxDma_HW, TxDma_CHANNEL), NULL);

    /* Initialize and enable the interrupt from TxDma */
    Cy_SysInt_Init  (int_config, &tx_dma_complete);
    NVIC_EnableIRQ((IRQn_Type) NvicMux2_IRQn);

    /* Enable DMA interrupt source */
    Cy_DMA_Channel_SetInterruptMask(TxDma_HW, TxDma_CHANNEL, CY_DMA_INTR_MASK);

    /* Enable Data Write block but keep channel disabled to not trigger
    *  descriptor execution because TX FIFO is empty and SCB keeps active level
    *  for DMA.
    */
    Cy_DMA_Enable(TxDma_HW);
}


/*******************************************************************************
* Function Name: rx_dma_complete
********************************************************************************
*
* Summary:
*  Handles Rx Dma descriptor completion interrupt source: triggers Tx Dma to
*  transfer back data received by the Rx Dma descriptor.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void rx_dma_complete(void)
{
    Cy_DMA_Channel_ClearInterrupt(RxDma_HW, RxDma_CHANNEL);

    /* Check interrupt cause to capture errors. */
    if (CY_DMA_INTR_CAUSE_COMPLETION == Cy_DMA_Channel_GetStatus(RxDma_HW, RxDma_CHANNEL))
    {
        rx_dma_done = 1;
    }
    else
    {
        /* DMA error occurred while RX operations */
        rx_dma_error = 1;
    }
}


/*******************************************************************************
* Function Name: tx_dma_complete
********************************************************************************
*
* Summary:
*  Handles Tx Dma descriptor completion interrupt source: only used for
*  indication.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void tx_dma_complete(void)
{
    /* Check interrupt cause to capture errors.
    *  Note that next descriptor is NULL to stop descriptor execution */
    if ((CY_DMA_INTR_CAUSE_COMPLETION    != Cy_DMA_Channel_GetStatus(TxDma_HW, TxDma_CHANNEL)) &&
        (CY_DMA_INTR_CAUSE_CURR_PTR_NULL != Cy_DMA_Channel_GetStatus(TxDma_HW, TxDma_CHANNEL)))
    {
        /* DMA error occurred while TX operations */
        tx_dma_error = 1;
    }
    Cy_DMA_Channel_ClearInterrupt(TxDma_HW, TxDma_CHANNEL);
}

/* [] END OF FILE */
