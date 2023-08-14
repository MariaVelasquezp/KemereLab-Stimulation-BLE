/*****************************************************************************
* File Name: main.c
*
*****************************************************************************/
#include <project.h>
#include <ble.h> 
#include <ble_eventHandler.h>

#define MAX_DAC_VALUE (127u)  /* Maximum value for a 7-bit DAC */

#define DAC_VALUE 127

#define WDT_DELAY 2000

#define PC_A_FLAG ((uint32)(0x06) << (3*Electrode_0_SHIFT)) // defined in Electrode_aliases.h
#define PC_B_FLAG ((uint32)(0x06) << (3*Electrode_1_SHIFT))

#define HSIOM_A_FLAG ((uint32)(0x06) << (4*Electrode_0_SHIFT))
#define HSIOM_B_FLAG ((uint32)(0x06) << (4*Electrode_1_SHIFT))

inline void mux_ground() {
    CY_SET_REG32((void *)(CYREG_HSIOM_PORT_SEL1), 0x00000000u); // Disconnect analog mux from all pins
    CY_SET_REG32((void *)(CYREG_GPIO_PRT1_PC), PC_A_FLAG + PC_B_FLAG);    // Set Port1_0 and Port1_1 to Hi-Z output
}

inline void mux_forward() {
    CY_SET_REG32((void *)(CYREG_HSIOM_PORT_SEL1),HSIOM_B_FLAG); // Analog mux to IDAC to PinB
	CY_SET_REG32((void *)(CYREG_GPIO_PRT1_PC),  PC_A_FLAG); // PinA output (PinB analog)
}

inline void mux_reverse() {
    CY_SET_REG32((void *)(CYREG_HSIOM_PORT_SEL1),HSIOM_A_FLAG); // Analog mux to IDAC to PinA
	CY_SET_REG32((void *)(CYREG_GPIO_PRT1_PC),  PC_B_FLAG); // PinA output (PinB analog)
}



/******************************************************************************
* Function Name: WDT_ISR_Handler
*******************************************************************************
*
* Summary:
* Interrupt Service Routine for the watchdog timer interrupt. The periodicity
* of the interrupt is depended on the match value written into the counter
* register using API - CySysWdtWriteMatch().
*
******************************************************************************/
void WDT_ISR ()
{
      
    LED_Write(1);

    mux_forward();
    IDAC_1_SetValue(DAC_VALUE);
    CyDelayUs(57u);   // 60 us
    IDAC_1_SetValue(0);
    
    mux_ground();
    CyDelayUs(1u);   
    
    mux_reverse();
    IDAC_1_SetValue(DAC_VALUE); 
    CyDelayUs(57u);   
    IDAC_1_SetValue(0);
    mux_ground();
    
    LED_Write(0);
}

/***************************************************************
 * Function to handle the BLE stack
 **************************************************************/
void BleCallBack(uint32 event)
{
    CYBLE_GATTS_WRITE_REQ_PARAM_T *wrReqParam;

    switch(event)
    {
        /* if there is a disconnect or the stack just turned on from a reset then start the advertising and turn on the LED blinking */
        case CYBLE_EVT_STACK_ON:
        case CYBLE_EVT_GAP_DEVICE_DISCONNECTED:
            /* Start advertising after disconnection */
            CyBle_GappStartAdvertisement(CYBLE_ADVERTISING_FAST);
            break;
        
        /* when a connection is made, update the LED and Capsense states in the GATT database and stop blinking the LED */    
        case CYBLE_EVT_GATT_CONNECT_IND:
            /* Update the GATT database with your service and characteristics here */
            /* For example, you can call a function to configure and register the GATT service */
            /* configureAndRegisterGattService(); */
            break;
        
        default:
            break;
    }
} 

int main()
{
    uint32 dacValue;

    IDAC_1_Start();     /* Initialize the IDAC */
    
    CY_SET_REG32((void *)(CYREG_GPIO_PRT1_PC2),  
            ((uint32)(0x01)<<Electrode_0_SHIFT) + 
            ((uint32)(0x01)<<Electrode_1_SHIFT)); // Force input off for both pins of electrode
    CY_SET_REG32((void *)(CYREG_GPIO_PRT1_DR), 0x00000000u); // Set digital value of P1_0 and P1_1 to 0
    
    IDAC_1_SetValue(0);
    mux_ground();
   
    /* Enable Global Interrupt */
    CyGlobalIntEnable;

    /* Start BLE stack and register the callback function */
    CyBle_Start(BleCallBack);
    
    /* Update the match register for generating a periodic WDT ISR.
    Note: In order to do a periodic ISR using WDT, Match value needs to be
    updated every WDT ISR with the desired time value added on top of the
    existing match value */
    //CySysWdtWriteMatch(WDT_DELAY);

    
    isr_1_StartEx(WDT_ISR);
    
    CySysWdtEnable(CY_SYS_WDT_COUNTER0);
    
    for(;;)   /* Loop forever */
    {
        /* Call CyBle_ProcessEvents() to process BLE events */
        CyBle_ProcessEvents();
        /* Enter low power mode */
        CyBle_EnterLPM(CYBLE_BLESS_DEEPSLEEP);
    }
}

/***************************************************************
 * Function to configure and register the GATT service
 **************************************************************/
void configureAndRegisterGattService()
{
    /* Define your custom GATT service and characteristics here */
    CYBLE_GATTS_HANDLE_VALUE_NTF_T charValue;
    CYBLE_GATT_HANDLE_VALUE_PAIR_T myHandle;

    /* Initialize and register the GATT service */
    /* Assuming you have a custom service UUID "0x2800" */
    CYBLE_GATT_HANDLE_VALUE_PAIR_T myServiceHandle;
    myServiceHandle.attrHandle = 0x0001; /* Specify a handle for your service */
    myServiceHandle.value.len = 6; /* Length of the service UUID */
    myServiceHandle.value.val = (uint8 *)"\x00\x28\x00\x00\x00\x00"; /* Replace with your service UUID */
    CyBle_GattsWriteAttributeValue(&myServiceHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
    CyBle_GattsWriteAttributeValue(&myServiceHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_PEER_INITIATED);

    /* Initialize and register the characteristic */
    /* Assuming you have a custom characteristic UUID "0x2803" */
    myHandle.attrHandle = 0x0002; /* Specify a handle for your characteristic */
    myHandle.value.val = (uint8 *)"\x00\x00"; /* Initial value of your characteristic */
    myHandle.value.len = 2; /* Length of the characteristic value */
    CyBle_GattsWriteAttributeValue(&myHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_LOCALLY_INITIATED);
    CyBle_GattsWriteAttributeValue(&myHandle, 0, &cyBle_connHandle, CYBLE_GATT_DB_PEER_INITIATED);

    /* Set the permissions for the characteristic (e.g., read, write, notify) */
    //CYBLE_GATT_DB_ATTR_SET_GEN_VALUE(myHandle.attrHandle, 0x0F, 0x04, 0, 0, 0);
    
    /* Register the GATT service with the BLE stack */
    CyBle_GattsEnableAttribute(myHandle.attrHandle);
}