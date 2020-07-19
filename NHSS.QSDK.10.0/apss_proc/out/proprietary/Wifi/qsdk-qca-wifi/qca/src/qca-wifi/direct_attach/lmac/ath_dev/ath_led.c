/*****************************************************************************/
/*! \file ath_led.c
**  \brief ATH LED control
**    
**  This file contains the implementation of the LED control module.
**
** Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
**
*/

#include "ath_internal.h"
#include "ath_timer.h"
#include "ath_led.h"

#if ATH_SUPPORT_LED

// LED not blinking
#define LED_STEADY_TIME                  0

// GPIO pins to which power and network LEDs are mapped to by default (MAC 5416)
#ifndef ATH_WINHTC
#define DEFAULT_NETWORK_LED_PIN          1
#define DEFAULT_POWER_LED_PIN            2
#else
#define DEFAULT_NETWORK_LED_PIN          14   // Dr.Wang: GPIO-14 will reserve for another LED, define as NetworkLED
#define DEFAULT_POWER_LED_PIN            15   // Now, K2 have only one LED using GPIO-15, define as PowerLED
#endif

// run the heartbeat timer every 50ms.
// faster rates will unnecessarily increase the CPU utilization.
#define LED_HEARTBEAT_TIMER_PERIOD      50
#define LED_RATE_UPDATE_INTERVAL       400   // update tx/rx rates every 400ms (multiple of the heartbeat period) 

#define DLINK_SCAN_PATTERN_DURATION   4000   // Show the scan blinking pattern for 6 seconds

#define LEDTIMER_MINCOUNT               20   // Sets the minimum threshold for doing the led rates

// The number of shift to convert the byte rate in 400ms (LED_RATE_UPDATE_INTERVAL) 
// into a pseudo rate (in Mbs).
// The full expression is
//     Rate(Mbs) = (tx bytes + rx bytes) * 8 bits/byte * (1000ms/400ms) / 1000000 bits.
//     Rate(Mbs) = (tx bytes + rx bytes) / 50000
// which is roughly converted to (tx bytes + rx bytes) / 2^15
//
// However, not all bandwidth is available for data. The highest rates observed 
// during unit tests were in the order of 21 Mbs, so we chose to multiply the 
// throughput by 2.6 to cover all the blinking rates
// The final expression is 
//     Rate(Mbs) = (tx bytes + rx bytes) / 19230
// which is roughly converted to (tx bytes + rx bytes) / 2^14
#define LEDTIMER_PSEUDORATESHIFT        14

// Convert the number of bytes in LED_RATE_UPDATE_INTERVAL into a rate in Mbits/second.
// For increased performance, we use a right shift instead of division.
#define ath_calculate_pseudo_rate(_n)   ((_n) >> LEDTIMER_PSEUDORATESHIFT)

// macros to control gpio functions 0 (power/link LED) and 1 (network/activity LED)
#define ath_gpio_func0(_pLedControl, _state)         \
    ath_gpio_func((_pLedControl), &((_pLedControl)->gpioFunc[0]), (_state));
#define ath_gpio_func1(_pLedControl, _state)         \
    ath_gpio_func((_pLedControl), &((_pLedControl)->gpioFunc[1]), (_state));
#define ath_gpio_func2(_pLedControl, _state)         \
    ath_gpio_func((_pLedControl), &((_pLedControl)->gpioFunc[2]), (_state));

// macro to swap two values
#define ath_variable_swap(_x, _y)    \
    { u_int32_t _temp; _temp = (_x); (_x) = (_y); (_y) = _temp; }

// Forward definitions
static void ath_set_link_led         (struct ath_led_control *pLedControl, LED_STATE state);
static void ath_set_activity_led     (struct ath_led_control *pLedControl, LED_STATE state);
static void ath_set_connection_led   (struct ath_led_control *pLedControl, LED_STATE state);

static void ath_led_state_transition (struct ath_led_control *pLedControl, HAL_LED_STATE newHalState);

#if ATH_DEBUG
static const char* HalLedStateName[] = 
{
    "RESET",
    "INIT",
    "READY",
    "SCAN",
    "AUTH",
    "ASSOC",
    "RUN"
};
#endif

/******************************************************************************/
/*!
**  \brief Convert LED Information
**
** Convert LED information contained in registry entries linkLedFunc, 
** activityLedFunc and connectionLedFunc into their gpioPinFuncs[x] and
** gpioActHiFuncs[x], storing the result in a GPIO_FUNC_INFO structure.
**
**  \param pGpioFunc Pointer to GPIO function object.
**  \param ledFunc Function number to assign
**  \param numGpioPins Total number of GPIO pins available.
**
**  \return N/A
*/

u_int32_t ath_LEDinfo(struct ath_led_control *pLedControl, u_int32_t bOn)
{
    if(bOn==0)
    {
        pLedControl->gpioLedCustom = CUSTOMER_DEFAULT;
        ath_hal_gpioCfgOutput_LEDoff(pLedControl->ah, pLedControl->gpioFunc[0].pin, HAL_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED);

    }
    else 
    {
        pLedControl->gpioLedCustom = CUSTOMER_ATHEROS;
        ath_hal_gpioCfgOutput(pLedControl->ah, pLedControl->gpioFunc[0].pin, HAL_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED);
    
    }
        

    return pLedControl->gpioLedCustom;

}

static void
convert_led_func(GPIO_FUNC_INFO *pGpioFunc, u_int16_t ledFunc, u_int32_t numGpioPins)
{
    if (IS_LED_ENABLE(ledFunc) && (LED_PIN(ledFunc) < numGpioPins))
    {
        u_int32_t    onValue = (LED_POLARITY(ledFunc) ? 1 : 0);

        pGpioFunc->enabled        = 1;
        pGpioFunc->pin            = LED_PIN(ledFunc);
        pGpioFunc->value[LED_OFF] = !onValue;
        pGpioFunc->value[LED_ON]  = onValue;
    }
}

#ifdef ATH_GPIO_OE_REG
#include <asm/addrspace.h>
static void ath_led_gpio_set(int pin, LED_STATE led_state)
{
    volatile unsigned int *oe_reg = (unsigned int *)(KSEG1ADDR(ATH_GPIO_OE_REG));
    volatile unsigned int *out_reg = (unsigned int *)(KSEG1ADDR(ATH_GPIO_OUT_REG));
    volatile unsigned int *func_reg = (unsigned int *)(KSEG1ADDR(ATH_GPIO_FUNC_REG));

    if (pin == 4)
        *func_reg = *func_reg & 0xffffff00;

    *oe_reg = *oe_reg & (~(1 << pin));

    if (led_state)
        *out_reg = *out_reg | (1 << pin);
    else
        *out_reg = *out_reg & (~(1 << pin));
}

static void ath_led_channel_poll(struct ath_led_control *pLedControl,
        unsigned int new_channel_flags)
{
    if (!(new_channel_flags & pLedControl->channelFlags & (CHANNEL_2GHZ | CHANNEL_5GHZ))) {
        if (new_channel_flags & CHANNEL_2GHZ) {
            pLedControl->gpioFunc[0].pin = ATH_GPIO_PIN_2G;
            pLedControl->gpioFunc[1].pin = ATH_GPIO_PIN_2G;
            pLedControl->gpioFunc[2].pin = ATH_GPIO_PIN_2G;
            ath_led_gpio_set(ATH_GPIO_PIN_5G, pLedControl->gpioFunc[0].value[LED_OFF]);
        } else {
            pLedControl->gpioFunc[0].pin = ATH_GPIO_PIN_5G;
            pLedControl->gpioFunc[1].pin = ATH_GPIO_PIN_5G;
            pLedControl->gpioFunc[2].pin = ATH_GPIO_PIN_5G;
            ath_led_gpio_set(ATH_GPIO_PIN_2G, pLedControl->gpioFunc[0].value[LED_OFF]);
        }
        pLedControl->channelFlags = new_channel_flags;
    }
}
#endif


/******************************************************************************/
/*!
**  \brief Initialize GPIO Functions
**
** Initialize data for GPIO functions based on registry entries.
** For each function we store the GPIO pin used and the values used to 
** turn the LED on or off (depends on whether the pin is active high or low).
**
** The registry provides 2 ways to specify gpio pins and polarity for each 
** LED functionality:
**
** Func[0] controls the link (power) LED.
**     registry entries: gpioPinFunc0/gpioFunc0ActHi or linkLedFunc
** Func[1] controls the network (activity) LED.
**     registry entries: gpioPinFunc1/gpioFunc1ActHi or activityLedFunc
** Func[2] controls the connection LED.
**     registry entries: gpioPinFunc2/gpioFunc2ActHi or connectionLedFunc
**
** Conflicts and precedence:
**     Entries XxxLedFunc take precedence over gpioPinFuncX.
**     Entry softLedEnable preempts gpioPinFuncX, but not XxxLedFunc, and can
**     be combined with the latter.
**
**  \param pLedControl Pointer to LED control structure
**  \param pRegParam Pointer to registry parameter used to specify control.
**
**  \return HAL_OK always
*/

static HAL_STATUS
ath_init_gpio (struct ath_led_control *pLedControl,
               struct ath_reg_parm    *pRegParam)
{
    int    i;

    for (i = 0; i < NUM_GPIO_FUNCS; i++)
    {
        pLedControl->gpioFunc[i].enabled = 0;
        pLedControl->sharedGpioFunc[i] = pRegParam->sharedGpioFunc[i];

        // Setting the LEDs via the hardware's registers (softLEDEnable) takes 
        // precedence over usage of GPIO pins.
        if (! pRegParam->softLEDEnable)
        {
            if (pRegParam->gpioPinFuncs[i] < pLedControl->numGpioPins)
            {
                // initialize function information
                pLedControl->gpioFunc[i].enabled        = 1;
                pLedControl->gpioFunc[i].pin            = pRegParam->gpioPinFuncs[i];
                pLedControl->gpioFunc[i].shared         = pRegParam->sharedGpioFunc[i];
                pLedControl->gpioFunc[i].value[LED_OFF] = pRegParam->gpioActHiFuncs[i] ? 0 : 1;
                pLedControl->gpioFunc[i].value[LED_ON]  = pRegParam->gpioActHiFuncs[i] ? 1 : 0;
            }
        }
    }

#ifdef ATH_GPIO_OE_REG
    if(pLedControl->sc->sc_osdev->bc.bc_bustype == QDF_BUS_TYPE_AHB){
        pLedControl->channelFlags = 0;
        ath_led_channel_poll(pLedControl, pLedControl->sc->sc_curchan.channel_flags);
    }
#endif

    // Setting the LEDs using registry entries linkLedFunc, activityLedFunc and
    // connectionLedFunc takes precedence over gpioFuncX, and can be combined 
    // with entry softLedEnable.
    convert_led_func(&(pLedControl->gpioFunc[0]), pRegParam->linkLedFunc,       pLedControl->numGpioPins);
    convert_led_func(&(pLedControl->gpioFunc[1]), pRegParam->activityLedFunc,   pLedControl->numGpioPins);
    convert_led_func(&(pLedControl->gpioFunc[2]), pRegParam->connectionLedFunc, pLedControl->numGpioPins);

    return HAL_OK;
}

/******************************************************************************/
/*!
**  \brief Generai GPIO Function Interface
**
**  Sets/clears the GPIO pin associated with this function according to the
**  input parm.  This generic function replaces NDIS 5.1's functions
**  gpioFunc0 and gpioFunc1.
**
**  \param ah Pointer to ATH HAL object that controls the chip
**  \param pGpioFunc Pointer to the specific GPIO function to use
**  \param ledState Indicates the state to put the LED into
**
**  \return HAL_OK on success
**  \return HAL_EINVAL if function is not enabled
*/

static HAL_STATUS
ath_gpio_func (struct ath_led_control *pLedControl, GPIO_FUNC_INFO *pGpioFunc, LED_STATE ledState)
{
    HAL_STATUS    status = HAL_OK;

    if (pGpioFunc->enabled)
    {
#ifdef __linux__
        if(pLedControl->sc->sc_osdev->bc.bc_bustype == QDF_BUS_TYPE_AHB)
#endif
        {
#ifdef ATH_GPIO_OE_REG
            ath_led_gpio_set(pGpioFunc->pin, pGpioFunc->value[ledState]);
#endif
#ifdef __linux__
        }else{
#endif
            ath_hal_gpioset(pLedControl->ah, pGpioFunc->pin, pGpioFunc->value[ledState]);
        }
    }
    else
    {
        status = HAL_EINVAL;
    }

    return status;
}

/******************************************************************************/
/*!
**  \brief Configure GPIO pin direction/drive
**
**  Configures the Hw gpio pins. All pins used for LED control must be
**  configured as output, meaning they will drive a device (the LED).
**
**  \param pLedControl Pointer to LED Control Object
**
**  \return HAL_OK - if one of the Led Function is supported
**  \return HAL_ENOTSUPP - if all Led Functions failed
**
*/

static HAL_STATUS
ath_configure_gpio_pins (struct ath_led_control *pLedControl)
{
    int    i;

    ASSERT(pLedControl);

    if (!pLedControl->ledControlDisabled)
    {
        return HAL_OK;
    }

    // Some customers (Buffalo) control LED using both GPIO pins and HW pins, 
    // so we have to allow for both approaches to coexist.
    // Conflicts are possible for CB72, but we'll cross that bridge when we 
    // get there. 
    // No conflicts happen for chipsets that do not use a multiplexer to 
    // select the output signal. Buffalo uses XB63, so we're OK.

    if (pLedControl->softLEDEnable                      || 
        pLedControl->cardBusLEDControl                  ||
        (pLedControl->gpioLedCustom == CUSTOMER_ATHEROS))
    {
        // Map Network and Power signals to GPIO pins 1 and 2.
        // Not all MACs support this configuration.
        ath_hal_gpioCfgOutput(pLedControl->ah, DEFAULT_NETWORK_LED_PIN, HAL_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED);
        ath_hal_gpioCfgOutput(pLedControl->ah, DEFAULT_POWER_LED_PIN,   HAL_GPIO_OUTPUT_MUX_AS_MAC_POWER_LED);
    }

    if (pLedControl->gpioLedCustom == CUSTOMER_ATHEROS_WASP)
    {
        // On Atheros's WASP platform, the on-board Osprey's GPIO-0 mapped to the WLAN LED
        ath_hal_gpioCfgOutput(pLedControl->ah, 0, HAL_GPIO_OUTPUT_MUX_AS_WLAN_ACTIVE);
    }

    if (pLedControl->gpioLedCustom == CUSTOMER_ATHEROS_SCORPION)
    {
        // On Atheros's Scorpion platform, the on-board Osprey's GPIO-7 mapped to the WLAN LED
        ath_hal_gpioCfgOutput(pLedControl->ah, 7, HAL_GPIO_OUTPUT_MUX_AS_WLAN_ACTIVE);
    }
	

    if (pLedControl->customLedControl)
    {
        // Map user-specified GPIO pins.
        ath_set_link_led(pLedControl, LED_OFF);
        ath_set_activity_led(pLedControl, LED_OFF);
        ath_set_connection_led(pLedControl, LED_OFF);

        for (i = 0; i < NUM_GPIO_FUNCS; i++)
        {
            if (pLedControl->gpioFunc[i].enabled && (pLedControl->sharedGpioFunc[i] == 0))
            {
                // Initialize the hardware: the GPIO pins used to control
                // the LEDs must be put in output mode.
                ath_hal_gpioCfgOutput(pLedControl->ah, pLedControl->gpioFunc[i].pin, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);
            }
        }
    }

    return HAL_OK;
}

/******************************************************************************/
/*!
**  \brief Tristate unused GPIO pins
**
** XXX: How do we find out exactly which gpio pins whould be tristated?
** Normally pins 1 and 2 are used for LEDs. However, if they are not being
** used for that purpose, maybe customer is using them for something else.
** Pin 0 is normally used for RF Kill, so we do not touch it.
**
** Until we develop our ESP module, let's assume customers use pins 1 and 2 
** for LEDs and nothing else.
**
**  \param pLedControl Pointer to LED Control Object
**
**  \return N/A
*/

static void
ath_tristate_gpio_pins (struct ath_led_control *pLedControl)
{
    int    i;
    int    tristateNetworkLedPin;
    int    tristatePowerLedPin;

    if (pLedControl->ah == NULL)
    {
        return;
    }

    /*
     * Tristate pins only if LED control is disabled, so it matches the
     * behavior of ath_configure_gpio_pins. 
     * If these 2 functions are not in synch, problems will arise when
     * switching from Infrastructure to IBSS mode.
     */
    if (!pLedControl->ledControlDisabled)
    {
        return;
    }

    // We must tristate the Power and Network pins iff we do not them in use 
    // in the loop below.
    tristateNetworkLedPin = 1;
    tristatePowerLedPin   = 1;

    // Parse array specifying which gpio pins are in use
    for (i = 0; i < NUM_GPIO_FUNCS; i++)
    {
        if (pLedControl->gpioFunc[i].enabled)
        {
            // If pin 1 or 2 in use, indicate we should not tristate it.
            // Leave other pins alone unless specifically requested.
            if (pLedControl->gpioFunc[i].pin == DEFAULT_NETWORK_LED_PIN)
            {
                tristateNetworkLedPin = 0;
            }
            else if (pLedControl->gpioFunc[i].pin == DEFAULT_POWER_LED_PIN)
            {
                tristatePowerLedPin = 0;
            }
        }
    }

    if (tristateNetworkLedPin)
    {
        ath_hal_gpioCfgInput(pLedControl->ah, DEFAULT_NETWORK_LED_PIN);
    }

    if (tristatePowerLedPin)
    {
        ath_hal_gpioCfgInput(pLedControl->ah, DEFAULT_POWER_LED_PIN);
    }
}

/******************************************************************************/
/*!
**  \brief Set Link LED State
**
**  Sets the state of the Link LED using the method specified by the registry
**  settings (software mimicking hardware patterns, or GPIO pins).
**
**  If Link Led data (pin and polarity) was provided through registry entry
**  linkLedFunc, we set the GPIO pin even if softLedEnable is also set.
**
**  If Link Led data (pin and polarity) was provided through registry entries
**  gpioPinFunc0 and gpioFunc0ActHi, we do not set the GPIO pin if softLedEnable
**  is set.
**
**  \param pLedControl Pointer to LED Control Object
**  \param state Enumerated value of LED state to set (as in ON or OFF)
**
**  \return N/A
*/

static void
ath_set_link_led (struct ath_led_control *pLedControl, LED_STATE state)
{
    if (pLedControl->softLEDEnable)
    {
        // set the link (power) LED 
        ath_hal_setpowerledstate(pLedControl->ah, state);
    }

    if (pLedControl->sharedGpioFunc[0])
    {
        if (pLedControl->gpioFunc[0].enabled && (state == LED_ON))
        {
            // set to output first then set value
            ath_gpio_func0(pLedControl, state);
            ath_hal_gpioCfgOutput(pLedControl->ah, pLedControl->gpioFunc[0].pin, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);
        }
        else
        {
            // Tristate gpio_func0
            ath_hal_gpioCfgInput(pLedControl->ah, pLedControl->gpioFunc[0].pin);
        }
    }
    else
    {
        //! GPIO Function 0 controls the link (power) LED
        //! Function ath_init_gpio already took all registry settings into 
        //! consideration when setting information for Func0, so it is safe
        //! to call ath_gpio_func0 regardless of the value of softLEDEnable
        ath_gpio_func0(pLedControl, state);
    }
}

/******************************************************************************/
/*!
**  \brief Set Activity LED State
**
** Sets the state of the Network LED using the method specified by the 
** registry settings (software mimicking hardware patterns, or GPIO pins)
**
** If Activity Led data (pin and polarity) was provided through registry 
** entry activityLedFunc, we set the GPIO pin even if softLedEnable is 
** also set.
**
** If Activity Led data (pin and polarity) was provided through registry
** entries gpioPinFunc1 and gpioFunc1ActHi, we do not set the GPIO pin if 
** softLedEnable is set.
**
** \param pLedControl Pointer to LED Control object
** \param state Enumerated state to set LED to
**
** \return N/A
*/

static void
ath_set_activity_led (struct ath_led_control *pLedControl, LED_STATE state)
{
    if (pLedControl->softLEDEnable)
    {
        // set the network (activity) LED 
        ath_hal_setnetworkledstate(pLedControl->ah, state);
    }

    //! GPIO Function 1 controls the network (activity) LED
    //! Function ath_init_gpio already took all registry settings into 
    //! consideration when setting information for Func1, so it is safe
    //! to call ath_gpio_func1 regardless of the value of softLEDEnable
    ath_gpio_func1(pLedControl, state);
}

/******************************************************************************/
/*!
**  \brief Set Connection LED State
**
** Sets the state of the Connection LED using the method specified by the 
** registry settings (software mimicking hardware patterns, or GPIO pins)
**
** If Connection Led data (pin and polarity) was provided through registry 
** entry connectionLedFunc, we set the GPIO pin even if softLedEnable is 
** also set.
**
** If Connection Led data (pin and polarity) was provided through registry 
** entries gpioPinFunc2 and gpioFunc2ActHi, we do not set the GPIO pin if 
** softLedEnable is set.
**
** \param pLedControl Pointer to LED control object
** \param state Enumerated state to set LED to
**
** \return N/A
*/

static void
ath_set_connection_led (struct ath_led_control *pLedControl, LED_STATE state)
{
    if (pLedControl->softLEDEnable)
    {
        // No equivalent LED pin in the cardbus
    }

    //! GPIO Function 2 controls the connection LED
    //! Function ath_init_gpio already took all registry settings into 
    //! consideration when setting information for Func2, so it is safe
    //! to call ath_gpio_func2 regardless of the value of softLEDEnable
    ath_gpio_func2(pLedControl, state);
}

/***************************************************************************
** LED Control Tables
** These are customer specific settings for LED blink pattern duty cycles
**
** GENERIC
*/

static const
LED_BLINK_RATES genBlinkRateTable[] = {
    { 108,  40,  10 },
    {  96,  40,  10 },
    {  72,  40,  10 },
    {  54,  40,  10 },
    {  48,  44,  11 },
    {  36,  50,  13 },
    {  24,  57,  14 },
    {  18,  67,  16 },
    {  12,  80,  20 },
    {  11, 100,  25 },
    {   9, 133,  34 },
    {   6, 160,  40 },
    {   5, 200,  50 },
    {   3, 240,  58 },
    {   2, 267,  66 },
    {   1, 400, 100 },
    {   0, 500, 130 }
};

/*
** LENOVO
*/

static const
LED_BLINK_RATES lenovoBlinkRateTable[] = {
    { 108,  40,  10 },
    {  96,  40,  10 },
    {  72,  40,  10 },
    {  54,  40,  10 },
    {  48,  44,  11 },
    {  36,  50,  13 },
    {  24,  57,  14 },
    {  18,  67,  16 },
    {  12,  80,  20 },
    {  11, 100,  25 },
    {   9, 133,  34 },
    {   6, 160,  40 },
    {   5, 200,  50 },
    {   3, 240,  58 },
    {   2, 267,  66 },
    {   1, 400, 100 },
    {   0, 400, 100 },
};

// Blinking rates used by NEC
// Duty Cycle = 50%
// Lowest rate = 3Hz (333.3ms total cycle; 166.6ms ON, 166.6ms OFF)
// Highest rate = 20Hz (50ms total cycle; 25ms ON, 25ms OFF)
//
// Keep in mind that the granularity of Windows timers is 10ms, so values
// will be rounded to the next multiple of 10ms. As a result, our fastest
// blinking rate is actually 30ms ON + 30ms OFF.
// For the slowest rate we use 150ms ON + 150ms OFF so that the total cycle
// will follow the specs after system latency is added.

static const
LED_BLINK_RATES necBlinkRateTable[] = {
    { 108,  25,  25 },
    {  96,  30,  30 },
    {  72,  30,  30 },
    {  54,  30,  30 },
    {  48,  30,  30 },
    {  36,  30,  30 },
    {  24,  35,  35 },
    {  18,  40,  40 },
    {  12,  45,  45 },
    {  11,  50,  50 },
    {   9,  60,  60 },
    {   6,  70,  70 },
    {   5,  80,  80 },
    {   3,  90,  90 },
    {   2, 100, 100 },
    {   1, 130, 130 },
    {   0, 150, 150 }
};

/*
** ASUS
*/

static const
LED_BLINK_RATES asusBlinkRateTable[] = {
    { 108,  25,  25 },
    {  96,  30,  30 },
    {  72,  30,  30 },
    {  54,  30,  30 },
    {  48,  30,  30 },
    {  36,  30,  30 },
    {  24,  40,  40 },
    {  18,  45,  45 },
    {  12,  50,  50 },
    {  11,  62,  62 },
    {   9,  83,  83 },
    {   6, 100, 100 },
    {   5, 125, 125 },
    {   3, 150, 150 },
    {   2, 163, 163 },
    {   1, 250, 250 },
    {   0, 350, 300 }
};

/******************************************************************************/
/*!
**  \brief Get Slow Rate
**
** Returns the slowest blinking rate for the specified table into the
** Cadence array
**
** \param pBlinkingCadence Pointer to 2 element array that gets the on
**        and off times
** \param pBlinking Pointer to table of Blink Rates
** \param BlinkingTableSize Integer indicating the size of the table
**
** \return N/A
*/

static void
led_get_slow_blinking_rate (u_int32_t       *pBlinkingCadence, 
    const LED_BLINK_RATES *pBlinkingTable, 
    int             BlinkingTableSize)
{
    pBlinkingCadence[LED_OFF] = pBlinkingTable[BlinkingTableSize-1].timeOff;
    pBlinkingCadence[LED_ON]  = pBlinkingTable[BlinkingTableSize-1].timeOn;
}

/******************************************************************************/
/*!
**  \brief Get Lenovo Custom Pattern
**
** This function sets the blinking pattern for the Lenovo case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
**
** \return N/A
*/

static void
led_get_pattern_custom_lenovo (struct led_state_info *pIdlePattern, 
                               struct led_state_info *pScanPattern, 
                               struct led_state_info *pConnectedPattern)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_ON,  LED_ON,  LED_OFF,   { 200,             5000            } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_ON,  LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    // LED steady ON if connected and no rx/tx traffic

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}

/******************************************************************************/
/*!
**  \brief Get Atheros Custom Pattern
**
** This function sets the blinking pattern for the Atheros case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
**
** \return N/A
*/

static void
led_get_pattern_custom_atheros (struct led_state_info *pIdlePattern, 
                                struct led_state_info *pScanPattern, 
                                struct led_state_info *pConnectedPattern)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_ON,  LED_ON,  LED_OFF,   { 200,             5000            } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_OFF, LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    // Blink LED at the slowest rate if connected and no rx/tx traffic.
    led_get_slow_blinking_rate(ConnectedPattern.blinking_cadence, 
        genBlinkRateTable, 
        ARRAY_LENGTH(genBlinkRateTable));

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}

/******************************************************************************/
/*!
**  \brief Get D-Link Custom Pattern
**
** This function sets the blinking pattern for the D-Link case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
**
** \return N/A
*/

static void
led_get_pattern_custom_dlink (struct led_state_info *pIdlePattern, 
                              struct led_state_info *pScanPattern, 
                              struct led_state_info *pConnectedPattern)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_OFF, LED_ON,  LED_OFF,   { 100,             1000            } };

    struct led_state_info    ScanPattern = 
        { LED_OFF, LED_ON,  LED_OFF,   { 250,             100             } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_OFF, LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    // Blink LED at the slowest rate if there is no rx/tx traffic
    led_get_slow_blinking_rate(ConnectedPattern.blinking_cadence, 
        genBlinkRateTable, 
        ARRAY_LENGTH(genBlinkRateTable));

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = ScanPattern;
    *pConnectedPattern = ConnectedPattern;
}

/******************************************************************************/
/*!
**  \brief Get Linksys Custom Pattern
**
** This function sets the blinking pattern for the Linksys case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
**
** \return N/A
*/

static void
led_get_pattern_custom_linksys (struct led_state_info *pIdlePattern, 
                                struct led_state_info *pScanPattern, 
                                struct led_state_info *pConnectedPattern)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_ON,  LED_OFF, LED_OFF,   { LED_STEADY_TIME, LED_STEADY_TIME } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_OFF, LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    // Blink LED at the slowest rate if there is no rx/tx traffic
    led_get_slow_blinking_rate(ConnectedPattern.blinking_cadence, 
        genBlinkRateTable, 
        ARRAY_LENGTH(genBlinkRateTable));

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}

/******************************************************************************/
/*!
**  \brief Get NEC Custom Pattern
**
** This function sets the blinking pattern for the NEC case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
**
** \return N/A
*/

static void
led_get_pattern_custom_nec (struct led_state_info *pIdlePattern, 
                            struct led_state_info *pScanPattern, 
                            struct led_state_info *pConnectedPattern)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_ON,  LED_ON,  LED_OFF,   { 4800,            200             } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_ON,  LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    //! LED steady ON if connected and no rx/tx traffic

    //! XXX: Correct cadence should be 4750ms OFF, 250ms ON.
    //!      However, since our slow timer has a 100ms granularity, the ON time
    //!      is rounded up to 300ms, and the system's latency time causes it to
    //!      go above NEC's specification (max=312ms).
    //!      The correct fix would be to use our higher-granularity timer (used
    //!      for fast blinking while connected) for the low-frequency blinking 
    //!      as well.
    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}

/******************************************************************************/
/*!
**  \brief Get BUFFALO Custom Pattern
**
** This function sets the blinking pattern for the BUFFALO case.  
** Three LEDs are used in this configuration: Two are driven by GPIO pins, 
** and their states depend exclusively on 11a and 11g support. The third LED
** is driven by the Activity pin provided by the HW.
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
** \param pRegParam    Pointer to the registry parameter structure.
**
** \return N/A
*/

static void
led_get_pattern_custom_buffalo(struct led_state_info *pIdlePattern, 
                               struct led_state_info *pScanPattern, 
                               struct led_state_info *pConnectedPattern,
                               struct ath_reg_parm   *pRegParam)
{
    struct led_state_info    IdlePattern = 
    //    Link     Activity Connection { LED_OFF          LED_ON          }
        { LED_OFF, LED_OFF, LED_OFF,   { LED_STEADY_TIME, LED_STEADY_TIME } };

    // LED controlled by GPIO 0 ON if 11a is supported, OFF otherwise
    if (pRegParam->NetBand & HAL_MODE_11A)
    {
        IdlePattern.link_led_state     = LED_ON;
    }

    // LED controlled by GPIO 1 ON if 11a is supported, OFF otherwise
    if (pRegParam->NetBand & HAL_MODE_11G)
    {
        IdlePattern.activity_led_state = LED_ON;
	} 

    // GPIO-controlled LEDs depend only on supported PHYs
    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = IdlePattern;
}

/******************************************************************************/
/*!
**  \brief Get ASUS Custom Pattern
**
** This function sets the blinking pattern for the ASUS case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
**
** \return N/A
*/

static void
led_get_pattern_custom_asus (struct led_state_info *pIdlePattern, 
                             struct led_state_info *pScanPattern, 
                             struct led_state_info *pConnectedPattern)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_ON,  LED_ON,  LED_OFF,   { 4800,            200             } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_OFF, LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    // LED steady ON if connected and no rx/tx traffic

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}

/******************************************************************************/
/*!
**  \brief Get Enhanced Custom Pattern
**
** This function sets the blinking pattern for the "Enhanced" case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table.  Additionally, the registry
** parameters are used to fine tune the parameters
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
** \param pRegParam    Pointer to the registry parameter structure.
**
** \return N/A
*/

static void
led_get_pattern_enhanced (struct led_state_info *pIdlePattern, 
                          struct led_state_info *pScanPattern, 
                          struct led_state_info *pConnectedPattern,
                          struct ath_reg_parm   *pRegParam)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_ON,  LED_OFF, LED_OFF,   { 3000,            500             } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_OFF, LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    //! Do not blink LEDs while not connected if either one of registry
    //! entries softLEDEnable and WiFiLEDEnable is set.
    if (pRegParam->softLEDEnable || pRegParam->WiFiLEDEnable)
    {
        IdlePattern.blinking_cadence[LED_OFF] = LED_STEADY_TIME;
        IdlePattern.blinking_cadence[LED_ON]  = LED_STEADY_TIME;
    }

    // Blink LED at the slowest rate if there is no rx/tx traffic
    led_get_slow_blinking_rate(ConnectedPattern.blinking_cadence, 
        genBlinkRateTable, 
        ARRAY_LENGTH(genBlinkRateTable));

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}


/******************************************************************************/
/*!
**  \brief Get "Soft LED" Custom Pattern
**
** This function sets the blinking pattern for the "Soft LED" case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table.  Additionally, the registry
** parameters are used to fine tune the parameters
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
** \param pRegParam    Pointer to the registry parameter structure.
**
** \return N/A
*/

static void
led_get_pattern_softled (struct led_state_info *pIdlePattern, 
                         struct led_state_info *pScanPattern, 
                         struct led_state_info *pConnectedPattern,
                         struct ath_reg_parm   *pRegParam)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_OFF, LED_OFF, LED_OFF,   { LED_STEADY_TIME, LED_STEADY_TIME } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_OFF, LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    // Initial state of link LED is determined by the user (default=LED_OFF)
    if (pRegParam->linkLEDDefaultOn)
    {
        IdlePattern.link_led_state     = LED_ON;
    }

    // Registry variable swapDefaultLED indicates the behaviour of the
    // link and activity LEDs must be reversed.
    if (pRegParam->swapDefaultLED)
    {
        ath_variable_swap(IdlePattern.link_led_state,      IdlePattern.activity_led_state);
        ath_variable_swap(ConnectedPattern.link_led_state, ConnectedPattern.activity_led_state);
    }

    // Blink LED at the slowest rate if there is no rx/tx traffic
    led_get_slow_blinking_rate(ConnectedPattern.blinking_cadence, 
        genBlinkRateTable, 
        ARRAY_LENGTH(genBlinkRateTable));

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}


/******************************************************************************/
/*!
**  \brief Get WiFi Custom Pattern
**
** This function sets the blinking pattern for the WiFi Allicnce standard.
** The three states defined are Idle (not connected), scanning, and connected.
** These states are populated in the led_info table.  Additionally, the
** registry parameters are used to fine tune the parameters.
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
** \param pRegParam    Pointer to the registry parameter structure.
**
** \return N/A
*/

static void
led_get_pattern_wifi (struct led_state_info *pIdlePattern, 
                      struct led_state_info *pScanPattern, 
                      struct led_state_info *pConnectedPattern,
                      struct ath_reg_parm   *pRegParam)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_ON,  LED_ON,  LED_OFF,   { LED_STEADY_TIME, LED_STEADY_TIME } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_ON,  LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    // LEDs should not blink if no data traffic present.

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}


/******************************************************************************/
/*!
**  \brief Get Default Pattern
**
** This function sets the blinking pattern for the Default case.  The three
** states defined are Idle (not connected), scanning, and connected.  These
** states are populated in the led_info table.
**
** \param pIdlePattern Pointer to led_state_info element that contains the
**                     idle pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     scan pattern in use.
** \param pScanPattern Pointer to led_state_info element that contains the
**                     connected pattern in use.
**
** \return N/A
*/

static void
led_get_pattern_default (struct led_state_info *pIdlePattern, 
                         struct led_state_info *pScanPattern, 
                         struct led_state_info *pConnectedPattern)
{
    //    Link     Activity Connection { LED_OFF          LED_ON          }
    struct led_state_info    IdlePattern =
        { LED_ON , LED_ON,  LED_OFF,   { LED_STEADY_TIME, LED_STEADY_TIME } };

    struct led_state_info    ConnectedPattern = 
        { LED_ON,  LED_OFF, LED_ON,    { LED_STEADY_TIME, LED_STEADY_TIME } };

    // Blink LED at the slowest rate if there is no rx/tx traffic
    led_get_slow_blinking_rate(ConnectedPattern.blinking_cadence, 
        genBlinkRateTable, 
        ARRAY_LENGTH(genBlinkRateTable));

    *pIdlePattern      = IdlePattern;
    *pScanPattern      = IdlePattern;
    *pConnectedPattern = ConnectedPattern;
}

/******************************************************************************/
/*!
**  \brief Updates the LED frequency and duty cycle
**
** Description: Updates the onTime and offTime based on the current activity
** of the rx and tx. Coded to behave as the Wifi mode, led toggles based on the
** total number of tx and rx bytes when connected.
**
** Uses the blinkRateTable and derives a pseudo rate that will correspond to
** the current rx and tx activity.
**
** \param pLedControl Pointer to LED Control Object
** \param applyThreshold Byte flag indicating that traffic threshold is to be
**                       used
** \param resetTrafficStats Byte flag indicating that the traffic statistics
**                          are to be reset.
** \param pOnTime Pointer to 32 bit value that gets the "on" time.
** \param pOffTime Pointer to 32 bit value that gets the "off" time.
**
** \return 1 if both ON and OFF times indicate LED is blinking
** \return 0 otherwise
*/

static int
ath_update_led_rate (struct ath_led_control *pLedControl, 
                     u_int8_t               applyThreshold,
                     u_int8_t               resetTrafficStats,
                     u_int32_t              *pOnTime, 
                     u_int32_t              *pOffTime)
{
    int32_t    byteCount;

    //! Retrieve number of data bytes transmitted/received in the last period
    //! and reset the counter.
    byteCount = atomic_read(&pLedControl->dataBytes);
    if (resetTrafficStats)
    {
        atomic_set(&pLedControl->dataBytes, 0);
    }

    // Use customer-specific blinking rate if no traffic.
    *pOnTime  = pLedControl->led_info[HAL_LED_RUN].blinking_cadence[LED_ON];
    *pOffTime = pLedControl->led_info[HAL_LED_RUN].blinking_cadence[LED_OFF];

    //! If total traffic below a certain threshold (LED_MINCOUNT), consider
    //! link to be "idle" and use default blinking rate. If traffic above
    //! the threshold, or threshold is disabled, calculate data rate.
    if ((byteCount > LEDTIMER_MINCOUNT) || (!applyThreshold))
    {
        const LED_BLINK_RATES *pRateEntry;
        int32_t            rateIndex;
        u_int32_t          pseudoRate;

        // Maps the data activity in the last update period into a pseudo connect rate.
        pseudoRate = (u_int32_t)ath_calculate_pseudo_rate(byteCount);

        pRateEntry = pLedControl->blinkRateTable;
        // Now search the table rate for the closest match to the calculated pseudoRate.
        for (rateIndex = 0; rateIndex < pLedControl->blinkRateTableLength; rateIndex++ )
        {
            if (pRateEntry->rate <= pseudoRate)
            {
                *pOnTime  = pRateEntry->timeOn;
                *pOffTime = pRateEntry->timeOff;
                break;
            }

            pRateEntry++;
        }
    }

    // Return 1 only if both LED state periods indicate blinking.
    return ((*pOnTime != LED_STEADY_TIME) && (*pOffTime != LED_STEADY_TIME));
}

/******************************************************************************/
/*!
**  \brief Set new state into LED control
**
** Higher level function to set the state of an LED control object.  This
** sets the state of the higher level object, then calls the control function
** with the new state to set the hardware.
**
** \param pLedControl Pointer to LED control object.
** \param pBlinkingControl Pointer to specific blink control object
** \param newState Enumerated state to put Blink Control into
**
** \return N/A
*/

static void
ath_initialize_blinking_led_state (struct ath_led_control  *pLedControl, 
                                   struct blinking_control *pBlinkingControl,
                                   LED_STATE               newState)
{
    // Switch led states
    pBlinkingControl->ledState = newState;

    // Blink the LED indirectly specified by the registry entries.
    pBlinkingControl->ledControlFunction(pLedControl, pBlinkingControl->ledState);
}

/******************************************************************************/
/*!
**  \brief "Invert" Led State
**
** This function "inverts" the state of the blinking control object by negating
** the current state.  The negated state is set back into the blinking control
** object, and the control fucntion is called to set the hardware.
**
** \warning Note that this assumes only two states.  If additional LED states
**          are added, this function may break.
**
** \param pLedControl Pointer to LED Control object
** \param pBlinkingControl Pointer to Blinking Control Object
**
** \return N/A
*/

static void
ath_flip_blinking_led_state (struct ath_led_control  *pLedControl, 
                             struct blinking_control *pBlinkingControl)
{
    // Switch led states
    pBlinkingControl->ledState = !pBlinkingControl->ledState;

    // Blink the LED indirectly specified by the registry entries.
    pBlinkingControl->ledControlFunction(pLedControl, pBlinkingControl->ledState);
}

/******************************************************************************/
/*!
**  \brief LED Heartbeat Task
**
** heartbeat timer: this timer fires at fixed intervals (unlike the 
** faster ath_led_fast_blinking). We rely on this fact and use accumulators 
** to run housekeeping functions at slower rates.  This function must be
** scheduled by an OS timer function.
**
** This uses the spinlock to arbitrate between two different timers.
**
** \param pLedControl Pointer to LED Control object.
**
** \return 0 if timer must be rearmed (periodic timers)
** \return !0 otherwise.
*/

static int
ath_led_heartbeat (struct ath_led_control *pLedControl)
{
    systime_t           currentTime = OS_GET_TIMESTAMP();

    // Acquire access to Hw. Synchronize with ath_led_fast_blinking.
    ATH_LED_LOCK(pLedControl);

#ifdef ATH_GPIO_OE_REG
    if(pLedControl->sc->sc_osdev->bc.bc_bustype == QDF_BUS_TYPE_AHB){
        ath_led_channel_poll(pLedControl, pLedControl->sc->sc_curchan.channel_flags);
    }
#endif

    if (pLedControl->halState == HAL_LED_RUN)
    {
        // Need to control LED blinking
        if (pLedControl->swFastBlinkingControl)
        {
            // If associated we calculate the LED blinking rate 
            // at intervals specified by LED_RATE_UPDATE_INTERVAL.
            // The actual blinking is performed by ath_led_fast_blinking.
            if (CONVERT_SYSTEM_TIME_TO_MS(currentTime - pLedControl->trafficStatisticsTimeStamp) >= 
                    LED_RATE_UPDATE_INTERVAL)
            {
                pLedControl->trafficStatisticsTimeStamp = currentTime;
        
                // Update blinking rates, applying minimum traffic threshold.
                // Do not interfere with the blinking controls.
                ath_update_led_rate(pLedControl, 
                                    1,
                                    1,
                                    &pLedControl->stateDuration[LED_ON], 
                                    &pLedControl->stateDuration[LED_OFF]);
            }
        }
    }
    else
    {
        // LED blinking happens at a lower rate if not associated
        if (pLedControl->stateDuration[pLedControl->blinkingControlNotConnected.ledState] != LED_STEADY_TIME)
        {
            // SCAN LED pattern can be shown only for a specified period of time.
            if (pLedControl->halState == HAL_LED_SCAN)
            {
                // If we reached the duration for which to show the scan 
                // pattern, return to the previous blinking state.
                if (CONVERT_SYSTEM_TIME_TO_MS(currentTime - pLedControl->scanStartTimestamp) >= 
                        pLedControl->scanStateDuration)
                {
                    ath_led_state_transition(pLedControl, pLedControl->saveState);
                }
            }

            // If duration for the current LED state has elapsed, we flip the 
            // LED state and save the time stamp for the new state.
            if (CONVERT_SYSTEM_TIME_TO_MS(currentTime - pLedControl->ledStateTimeStamp) >= 
                    pLedControl->stateDuration[pLedControl->blinkingControlNotConnected.ledState])
            {
                ath_flip_blinking_led_state(pLedControl, &pLedControl->blinkingControlNotConnected);

                pLedControl->ledStateTimeStamp = currentTime;
            }
        }
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);

    return (0);
}

/******************************************************************************/
/*!
**  \brief Fast Blinking task
**
** Due to the short blinking periods, we continuously adjust the timer period
** to the duration of the new LED state, and then change the LED state 
** every time the timer fires.
**
** For example, if the LED is to blink at a cadence of 200ms ON and 50ms OFF,
** we alternate setting the timer period to 200ms and 50ms, and change the 
** LED state every time the timer fires.
**
** Uses spinlock to control access between two timers
**
** \param pLedControl Pointer to LED Control object
**
** \return 0 if timer must be rearmed (periodic timers)
** \return !0 otherwise.
*/

static int
ath_led_fast_blinking (struct ath_led_control *pLedControl)
{
    int    rc = 0;  // 0 to continue blinking; !0 to stop

    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat
    ATH_LED_LOCK(pLedControl);

    // set the timer period to the duration of new LED state. 
    // The timer will be rearmed to this new duration.
    if (pLedControl->stateDuration[pLedControl->blinkingControlConnected.ledState] != LED_STEADY_TIME)
    {
        // duration for the current LED state has elapsed, so we flip the LED state
        ath_flip_blinking_led_state(pLedControl, &pLedControl->blinkingControlConnected);

        // set timer period only if blinking continues
        ath_set_timer_period(&pLedControl->ledFastTimer, 
            pLedControl->stateDuration[pLedControl->blinkingControlConnected.ledState]);
    } 
    else
    {
        // Activity LED steady; return it to original state
        ath_initialize_blinking_led_state(pLedControl, 
                                          &pLedControl->blinkingControlConnected,
                                          pLedControl->led_info[HAL_LED_RUN].activity_led_state);

        rc = 1;    // No more blinking if LED state is steady
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);

    return (rc);
}

/******************************************************************************/
/*!
**  \brief Process HAL Transition
**
** Upon change of HAL state, uses the new HAL state to set the LED state and
** blinking rate based on information contained in table led_info.
**
** \param pLedControl Pointer to LED Control object
** \param newHalState Enumerated value of the new HAL state
**
** \return N/A
*/

static void
ath_led_state_transition (struct ath_led_control *pLedControl, HAL_LED_STATE newHalState)
{
    HAL_BOOL    stateChange;
    systime_t   currentTime = OS_GET_TIMESTAMP();

    // No changes of state if LEDs are disabled
    if (pLedControl->ledControlDisabled)
    {
        return;
    }

    /*
     * When it's in HAL_LED_SCAN state, make sure it stays in that state for
     * scanStateDuration ms. If the new state is HAL_LED_RUN, we'll change blinking
     * pattern right away.
     */
    if ((pLedControl->halState == HAL_LED_SCAN) && (newHalState != HAL_LED_RUN))
    {
        if (CONVERT_SYSTEM_TIME_TO_MS(currentTime - pLedControl->scanStartTimestamp) < 
            pLedControl->scanStateDuration)
        {
            pLedControl->saveState = newHalState;
            return;
        }
    }

    DPRINTF(pLedControl->sc, ATH_DEBUG_LED, "%s: HalState %s->%s\n", 
        __func__, 
        HalLedStateName[pLedControl->halState], 
        HalLedStateName[newHalState]);

    stateChange = (pLedControl->halState != newHalState);
    pLedControl->halState = newHalState;

    // Set LED pattern in the hardware
    if (pLedControl->cardBusLEDControl)
    {
        // LED_0 or LED_1 in PCICFG register could be used for other purposes.
        // For example, HB63 is using LED_1 to control wow power switch.
        // In this case, do not change the led state.
        if (! pLedControl->DisableLED01)
            ath_hal_setledstate(pLedControl->ah, newHalState);

        // Return if no GPIO-controlled LEDs are present
        if (! pLedControl->customLedControl)
        {
            return;
        }
    }

    // If we got here, LED states/blinking are software (i.e. driver) controlled.
    switch (newHalState)
    {
    case HAL_LED_RESET:     // power off
    case HAL_LED_INIT:
    case HAL_LED_READY:
    case HAL_LED_SCAN:
    case HAL_LED_AUTH:
    case HAL_LED_ASSOC:
        // Nothing to do if state has not changed
        if (! stateChange)
        {
            break;
        }

        // Just in case the fast timer was running...
        if (ath_timer_is_initialized(&pLedControl->ledFastTimer))
        {
            ath_cancel_timer(&pLedControl->ledFastTimer, CANCEL_NO_SLEEP);
        }

        // Set initial state for the LEDs
        // Link and Activity LEDs are set only if they are steady - i.e. 
        // NOT being controlled by the blinking routine.
        if ((pLedControl->blinkingControlNotConnected.ledControlFunction != ath_set_link_led) ||
            (pLedControl->led_info[newHalState].blinking_cadence[LED_ON] == LED_STEADY_TIME))
        {
            ath_set_link_led(pLedControl, pLedControl->led_info[newHalState].link_led_state);
        }

        if ((pLedControl->blinkingControlNotConnected.ledControlFunction != ath_set_activity_led) ||
            (pLedControl->led_info[newHalState].blinking_cadence[LED_ON] == LED_STEADY_TIME))
        {
            ath_set_activity_led(pLedControl, pLedControl->led_info[newHalState].activity_led_state);
        }
        ath_set_connection_led(pLedControl, pLedControl->led_info[newHalState].connection_led_state);

        // Specify the blinking cadence. 
        // If not associated, only the link (power) LED may blink.
        pLedControl->stateDuration[LED_OFF] = pLedControl->led_info[newHalState].blinking_cadence[LED_OFF];
        pLedControl->stateDuration[LED_ON]  = pLedControl->led_info[newHalState].blinking_cadence[LED_ON];

        // Save scan start time.
        if (newHalState == HAL_LED_SCAN)
        {
            pLedControl->scanStartTimestamp = OS_GET_TIMESTAMP();
        }
        break;

    case HAL_LED_RUN:
        // If connected, reset data count
        if (stateChange)
        {
            // reset the timer used by ath_led_heartbeat for blinking LEDs
            pLedControl->ledStateTimeStamp = 0;
            atomic_set(&pLedControl->dataBytes, 0);
        }

        // Set blinking pattern. Start with slowest rate
        // If associated, only the network (activity) LED may blink.
        if (ath_timer_is_initialized(&pLedControl->ledFastTimer))
        {
            if (! ath_timer_is_active(&pLedControl->ledFastTimer))
            {
                // Set initial state for the LEDs
                ath_set_link_led(pLedControl, pLedControl->led_info[newHalState].link_led_state);
                ath_set_activity_led(pLedControl, pLedControl->led_info[newHalState].activity_led_state);
                ath_set_connection_led(pLedControl, pLedControl->led_info[newHalState].connection_led_state);

                // Update fast timer period. 
                // Start timer only if LEDs will indeed blink. Some customers
                // specify that LEDs should not blink if no traffic is present.
                // Apply minimum traffic threshold.
                if (ath_update_led_rate(pLedControl, 
                                        1,
                                        0,
                                        &pLedControl->stateDuration[LED_ON], 
                                        &pLedControl->stateDuration[LED_OFF]))
                {
                    // Data started flowing again; change LED state and start blinking
                    ath_flip_blinking_led_state(pLedControl, &pLedControl->blinkingControlConnected);

                    // Blinking rates when associated are much faster, 
                    // so we control the LEDs from a separate timer. 
                    // The slower heartbeat timer still runs, updating the 
                    // blinking rate at pre-determined intervals.
                    ath_set_timer_period(&pLedControl->ledFastTimer, 
                        pLedControl->stateDuration[pLedControl->led_info[newHalState].activity_led_state]);
                    ath_start_timer(&pLedControl->ledFastTimer);
                }
            }
        }
        break;
    }
}

/******************************************************************************/
/*!
**  \brief Synchronous set LED State
**
** Synchronizes access to LED state variables using the LED Spinlock. Calls the
** function responsible for the changes to the LED states.
**
** \param pLedControl Pointer to LED Control state
** \param newHalState Enumerated value of new HAL state
**
** \return N/A
*/

void
ath_led_set_state (struct ath_led_control *pLedControl, HAL_LED_STATE newHalState)
{
    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    ath_led_state_transition(pLedControl, newHalState);

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);
}

/******************************************************************************/
/*!
**  \brief Update LED cadence table
**
** This function will initialize the LED control object with the proper data
** specific for the indicated customer.  The settings to use are based on
** registry values.
**
** \param pLedControl Pointer to LED Control object to update
** \param pRegParam Pointer to registry data structure
**
** \return N/A
*/

static void
ath_initialize_led_cadence_table (struct ath_led_control *pLedControl,
    struct ath_reg_parm    *pRegParam)
{
    struct led_state_info    idle_pattern, scan_pattern, connected_pattern;

    // When not connected, blink Link LED if registry entries linkLEDFunc,
    // activityLEDFunc or connectionLEDFunc are defined, otherwise blink 
    // Activity LED.
    // When connected, blink Activity LED
    // Then swap Activity LED for Link LED if swapDefaultLED is set.
    if (pRegParam->swapDefaultLED)
    {
        pLedControl->blinkingControlNotConnected.ledControlFunction =
            pLedControl->enhancedLedControl ? ath_set_activity_led : ath_set_link_led;

        pLedControl->blinkingControlConnected.ledControlFunction    = ath_set_link_led;
    }
    else
    {
        pLedControl->blinkingControlNotConnected.ledControlFunction =
            pLedControl->enhancedLedControl ? ath_set_link_led : ath_set_activity_led;

        pLedControl->blinkingControlConnected.ledControlFunction    = ath_set_activity_led;
    }

    // Set initial LED state.
    pLedControl->blinkingControlNotConnected.ledState = LED_OFF;
    pLedControl->blinkingControlConnected.ledState    = LED_OFF;

    // Set default blinking table to be used in connected state.
    // May be overwritten to account for customer-specific requirements.
    pLedControl->blinkRateTable         = genBlinkRateTable;
    pLedControl->blinkRateTableLength   = ARRAY_LENGTH(genBlinkRateTable);
    pLedControl->swFastBlinkingControl  = (pRegParam->softLEDEnable || pLedControl->customLedControl);
    

    // Most customers do not require a special blinking pattern during scan.
    pLedControl->scanStateDuration = 0;

    // This convoluted logic, ported from NDIS 5.1, means that the LED must 
    // blink while not connected only if softLEDEnable and WiFiLEDEnable 
    // are 0 AND gpio usage has been specified.
    if (pLedControl->enhancedLedControl)
    {
        led_get_pattern_enhanced(&idle_pattern, &scan_pattern, &connected_pattern, pRegParam);
    }
    else if ((!pRegParam->softLEDEnable) &&
             (!pRegParam->WiFiLEDEnable) &&
             (pLedControl->customLedControl))
    {
        switch (pRegParam->gpioLedCustom) 
        {
        case CUSTOMER_DLINK:      // D-Link
            led_get_pattern_custom_dlink(&idle_pattern, &scan_pattern, &connected_pattern);

            // D-Link requires special blinking during scan (foreground and background)
            pLedControl->scanStateDuration = DLINK_SCAN_PATTERN_DURATION;
            break;

        case CUSTOMER_LINKSYS:    // LinkSys
            led_get_pattern_custom_linksys(&idle_pattern, &scan_pattern, &connected_pattern);
            break;

        case CUSTOMER_LENOVO:     // Foxconn/Lenovo
            led_get_pattern_custom_lenovo(&idle_pattern, &scan_pattern, &connected_pattern);

            // custom blinking rate for Lenovo
            pLedControl->blinkRateTable       = lenovoBlinkRateTable;
            pLedControl->blinkRateTableLength = ARRAY_LENGTH(lenovoBlinkRateTable);
            break;

        case CUSTOMER_NEC:        // NEC
            led_get_pattern_custom_nec(&idle_pattern, &scan_pattern, &connected_pattern);

            // Custom blinking rate for NEC
            pLedControl->blinkRateTable       = necBlinkRateTable;
            pLedControl->blinkRateTableLength = ARRAY_LENGTH(necBlinkRateTable);
            break;

        case CUSTOMER_BUFFALO:      // Buffalo
            led_get_pattern_custom_buffalo(&idle_pattern, &scan_pattern, &connected_pattern, pRegParam);

            // GPIO-controlled LEDs do not blink when connected.
            pLedControl->blinkingControlConnected.ledControlFunction = ath_set_connection_led;

            // Buffalo uses gpio pins in addition to chip-controlled LED pins.
            pLedControl->cardBusLEDControl     = 1;

            // LED blinking when connected is controlled by HW.
            pLedControl->swFastBlinkingControl = 0;
            pLedControl->blinkRateTable        = NULL;
            pLedControl->blinkRateTableLength  = 0;
            break;

        case CUSTOMER_ASUS:       // ASUS
            led_get_pattern_custom_asus(&idle_pattern, &scan_pattern, &connected_pattern);

            // Custom blinking rate for ASUS
            pLedControl->blinkRateTable       = asusBlinkRateTable;
            pLedControl->blinkRateTableLength = ARRAY_LENGTH(asusBlinkRateTable);
            break;

        case CUSTOMER_ATHEROS:    // Atheros
        case CUSTOMER_ATHEROS_WASP:     //Atheros WASP platform // notice fallthrough
        case CUSTOMER_ATHEROS_SCORPION:
        default:
            led_get_pattern_custom_atheros(&idle_pattern, &scan_pattern, &connected_pattern);
            break;
        }
    }
    else
    {
        if (pRegParam->softLEDEnable)
        {
            led_get_pattern_softled(&idle_pattern, &scan_pattern, &connected_pattern, pRegParam);
        }
        else if (pRegParam->WiFiLEDEnable)
        {
            led_get_pattern_wifi(&idle_pattern, &scan_pattern, &connected_pattern, pRegParam);
        }
        else
        {
            led_get_pattern_default(&idle_pattern, &scan_pattern, &connected_pattern);
        }
    }

    pLedControl->led_info[HAL_LED_INIT]  = idle_pattern;
    pLedControl->led_info[HAL_LED_READY] = idle_pattern;
    pLedControl->led_info[HAL_LED_SCAN]  = scan_pattern;
    pLedControl->led_info[HAL_LED_AUTH]  = idle_pattern;
    pLedControl->led_info[HAL_LED_ASSOC] = idle_pattern;
    pLedControl->led_info[HAL_LED_RUN]   = connected_pattern;

    // Led states for HAL_LED_RESET and HAL_LED_RUN do not change from default.
    // Blinking rates for HAL_LED_RUN depend on traffic and 
    // cannot be calculated at this time.
}

void ath_led_reinit(struct ath_softc *sc)
{
    sc->sc_led_control.customLedControl = 1;
    sc->sc_led_control.gpioLedCustom = sc->sc_reg_parm.gpioLedCustom;
    ath_initialize_led_cadence_table(&sc->sc_led_control, &sc->sc_reg_parm);
}

/******************************************************************************/
/*!
**  \brief LED Module InitializerConvert LED Information
**
** Initialize the LED module as indicated by the registry parameters
**
** \param sc Pointer to ATH object instance
** \param sc_osdev Reference to OS specific "handle" for the driver
** \param ah Pointer to HAL object instance
** \param pLedControl Pointer to LED control object
** \param pRegParam Pointer to Registry parameter structure
**
** \return N/A
*/

void
ath_led_initialize_control (struct ath_softc       *sc,
                            osdev_t                sc_osdev,
                            struct ath_hal         *ah,
                            struct ath_led_control *pLedControl,
                            struct ath_reg_parm    *pRegParam,
                            HAL_BOOL               bAllowCardBusLedControl)
{
    // Initial values for table containing LED states and ON/OFF times for each HAL state.
    static const struct led_state_info led_info_template[NUMBER_HAL_LED_STATES] = {
        /* HAL_LED_RESET */ { LED_OFF, LED_OFF, LED_OFF, { LED_STEADY_TIME, LED_STEADY_TIME } },
        /* HAL_LED_INIT  */ { LED_OFF, LED_OFF, LED_OFF, { LED_STEADY_TIME, LED_STEADY_TIME } },
        /* HAL_LED_READY */ { LED_OFF, LED_OFF, LED_OFF, { LED_STEADY_TIME, LED_STEADY_TIME } },
        /* HAL_LED_SCAN  */ { LED_OFF, LED_OFF, LED_OFF, { LED_STEADY_TIME, LED_STEADY_TIME } },
        /* HAL_LED_AUTH  */ { LED_OFF, LED_OFF, LED_OFF, { LED_STEADY_TIME, LED_STEADY_TIME } },
        /* HAL_LED_ASSOC */ { LED_OFF, LED_OFF, LED_OFF, { LED_STEADY_TIME, LED_STEADY_TIME } },
        /* HAL_LED_RUN   */ { LED_ON,  LED_ON,  LED_ON,  { LED_STEADY_TIME, LED_STEADY_TIME } },
    };

    // Pointers to OS, ath, and HAL structures needed for
    // communicating with those layers.
    pLedControl->osdev                  = sc_osdev;
    pLedControl->sc                     = sc;
    pLedControl->ah                     = ah;

    // Initialize LED state info table (per device) using global LED state info table
    OS_MEMCPY(pLedControl->led_info, led_info_template, sizeof(pLedControl->led_info));

    // Initial state
    pLedControl->halState               = HAL_LED_RESET;
    pLedControl->ledControlDisabled     = 1;

    // Set default blinking cadence = not blinking
    pLedControl->stateDuration[LED_OFF] = LED_STEADY_TIME;
    pLedControl->stateDuration[LED_ON]  = LED_STEADY_TIME;
    pLedControl->ledStateTimeStamp      = 0;

    // Find out number of GPIO pins actually supported by the hardware
    ath_hal_getcapability(pLedControl->ah, HAL_CAP_NUM_GPIO_PINS, 0, &(pLedControl->numGpioPins));

    // Setting the LEDs via the hardware's registers (softLEDEnable) takes 
    // precedence over usage of GPIO pins.
    ath_init_gpio(pLedControl, pRegParam);

    // Usage of registry entries linkLEDFunc, activityLEDFunc or
    // connectionLEDFunc imply blinking rates different from those used 
    // if gpioPinFuncX entries are populated. 
    pLedControl->enhancedLedControl     = IS_LED_ENABLE(pRegParam->linkLedFunc)       ||
                                          IS_LED_ENABLE(pRegParam->activityLedFunc)   ||
                                          IS_LED_ENABLE(pRegParam->connectionLedFunc);

    // If any of the GPIO-controlling entries are used, we have custom 
    // LED patterns. Notice that gpioFunc[x] combines the values of
    // XxxLedFunc and gpioPinFuncX.
    pLedControl->customLedControl       = pLedControl->gpioFunc[0].enabled ||
                                          pLedControl->gpioFunc[1].enabled ||
                                          pLedControl->gpioFunc[2].enabled;

    // Save registry settings. 
    // GPIO settings in pLedControl must be set by now.
    pLedControl->softLEDEnable          = pRegParam->softLEDEnable;
    pLedControl->gpioLedCustom          = pRegParam->gpioLedCustom;

    // LED_0 or LED_1 might be used for other purposes.
    pLedControl->DisableLED01           = pRegParam->DisableLED01;

    /*
     * gpioFunc0 disabled and sharedGpioFunc0 enabled is an invalid registry 
     * configuration.
     */
    ASSERT(pLedControl->gpioFunc[0].enabled || !pRegParam->sharedGpioFunc[0]);

    if (pLedControl->gpioFunc[0].enabled)
    {
        // On some systems, link LED (gpioFunc0) might be shared with other devices. 
        // To avoid different devices drive the LED with different values at the same time, 
        // only drives the GPIO when LED turns on. Tristate the GPIO when LED turns off. 
        pLedControl->sharedGpioFunc[0]  = pRegParam->sharedGpioFunc[0];
    }

    // Specify whether chip must control LEDs.
    // Function ath_initialize_led_cadence_table may change this flag to
    // account for customer requirements.
    pLedControl->cardBusLEDControl      = (! pRegParam->softLEDEnable) &&
                                          (! pLedControl->customLedControl) &&
                                          (  bAllowCardBusLedControl);

    // Initialize LED conditions (ON/OFF) and blinking cadence for each HAL state
    ath_initialize_led_cadence_table(pLedControl, pRegParam);

    // Initialize synchronization object used to ensure ath_led_heartbeat and
    // ath_led_fast_blinking do not run concurrently on multiple-processor machines
    //
    ATH_LED_LOCK_INIT(pLedControl);

    // Create a timer if the LED blinking must be controlled via software
    // rather than via hardware.
    //
    if ((pRegParam->softLEDEnable) || (pLedControl->customLedControl))
    {
        ath_initialize_timer(sc->sc_osdev, &pLedControl->ledHeartbeatTimer, 
                             LED_HEARTBEAT_TIMER_PERIOD, 
                             (timer_handler_function) ath_led_heartbeat,     
                             pLedControl);

        // Initialize fast timer only if blinking will be controlled by SW.
        if (pLedControl->swFastBlinkingControl)
        {
            ath_initialize_timer(sc->sc_osdev, &pLedControl->ledFastTimer, 
                                 0, 
                                 (timer_handler_function) ath_led_fast_blinking, 
                                 pLedControl);
        }
    }
}

/******************************************************************************/
/*!
 **  \brief Stop LED tasks, free resources
 **
 ** This function stops the timers and frees OS dependent timer resources.
 **
 ** \param pLedControl Pointer to LED control object.
 **
 ** \return N/A
 */

void
ath_led_free_control (struct ath_led_control *pLedControl)
{
    ath_led_halt_control(pLedControl);
    ath_free_timer(&pLedControl->ledHeartbeatTimer);
    ath_free_timer(&pLedControl->ledFastTimer);
}

/******************************************************************************/
/*!
**  \brief Start LED control function
**
** Start controlling LEDs using the timer functions and GPIO functions. Internal
** data structures must have been previously initialized through a call to
** ath_initialize_led_control. Initial LED state depends on radio state.
**
** This function will initialize the pin functions and set the states of the
** pins in use.  Finally, it will start the timer controlling the heartbeat
** and Fast blink tasks.  This function takes control of the spin lock to
** synchronize access.
**
** \param pLedControl Pointer to LED Control object.
** \param radioOn Integer flag indicating state of radio (0=off, 1=on)
**
** \return N/A
*/

void
ath_led_start_control (struct ath_led_control *pLedControl, int radioOn)
{
    DPRINTF(pLedControl->sc, ATH_DEBUG_LED, "%s: radioOn=%d suspended=%d disabled=%d\n", 
        __func__, 
        radioOn,
        pLedControl->ledSuspended, 
        pLedControl->ledControlDisabled);

    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    // Resume from S3/S4
    pLedControl->ledSuspended = 0;

    // Tristate gpio pins not in use
    ath_tristate_gpio_pins(pLedControl);

    // Configure gpio pins used for LED control.
    // This must be done in ath_start_led_control instead of 
    // ath_initialize_led_control to make sure the pins are properly
    // initialized after Sleep or Hibernate.
    ath_configure_gpio_pins(pLedControl);

    // Activate LEDs
    if (radioOn)
    {
        pLedControl->ledControlDisabled = 0;
        ath_led_state_transition(pLedControl, HAL_LED_INIT);
    }

    // Start LED timer if device has LEDs and we haven't started timer yet
    if (ath_timer_is_initialized(&pLedControl->ledHeartbeatTimer) && (!ath_timer_is_active(&pLedControl->ledHeartbeatTimer)))
    {
        ath_start_timer(&pLedControl->ledHeartbeatTimer);
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);
}

/******************************************************************************/
/*!
**  \brief Stop LED tasks
**
** This function stops the timers that drive the LED control tasks, effectively
** halting control of the LED functions.  Note that no hardware access is made
** in this routine, but the spin lock is used to synchronize access to the
** timers.
**
** \param pLedControl Pointer to LED control object.
**
** \return N/A
*/

void
ath_led_halt_control (struct ath_led_control *pLedControl)
{
    DPRINTF(pLedControl->sc, ATH_DEBUG_LED, "%s: suspended=%d disabled=%d\n", 
        __func__, 
        pLedControl->ledSuspended, 
        pLedControl->ledControlDisabled);

    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    // Disable LEDs
    pLedControl->ledControlDisabled = 1;

    // Cancel timer. Use busy wait so that function can be called from any IRQL.
    if (ath_timer_is_initialized(&pLedControl->ledHeartbeatTimer))
    {
        ath_cancel_timer(&pLedControl->ledHeartbeatTimer, CANCEL_NO_SLEEP);
    }

    // Cancel timer. Use busy wait so that function can be called from any IRQL.
    if (ath_timer_is_initialized(&pLedControl->ledFastTimer))
    {
        ath_cancel_timer(&pLedControl->ledFastTimer, CANCEL_NO_SLEEP);
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);
}

/******************************************************************************/
/*!
**  \brief LED Special Scan Process Start
**
** Notifies the LED module that the driver is performing a scan.  Called by
** the scan process.
**
** Some customers require a different blinking pattern during external scans.
** However, unless customers specifically require it, do not change the 
** blinking pattern if connected (background scan), or it might give the
** impression that connection was lost.
**
** \param pLedControl Pointer to LED Control object.
**
** \return N/A
*/

void
ath_led_scan_start (struct ath_led_control *pLedControl)
{
    DPRINTF(pLedControl->sc, ATH_DEBUG_LED, "%s: suspended=%d disabled=%d scanStateDuration=%d state=%s\n", 
        __func__, 
        pLedControl->ledSuspended, 
        pLedControl->ledControlDisabled,
        pLedControl->scanStateDuration,
        HalLedStateName[pLedControl->halState]);

    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    // Ignore changes to the LED state if the driver is suspended (S3/S4 state)
    if (pLedControl->ledSuspended)
    {
        // Release access to Hw
        ATH_LED_UNLOCK(pLedControl);

        return;
    }

    if (! pLedControl->ledControlDisabled)
    {
        // If customer requires unique blinking pattern during external scan...
        if (pLedControl->scanStateDuration != 0)
        {
            // Save current LED state
            pLedControl->saveState = pLedControl->halState;

            // Set new LED state. 
            ath_led_state_transition(pLedControl, HAL_LED_SCAN);
        }
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);
}

/******************************************************************************/
/*!
**  \brief LED Special Scan Process End
**
** Called by the scan process, notifies the LED processing that the scan has
** ended.  This will internally transition the HAL state and cause any hardware
** updates requred. This routine uses the spin lock to synchronize access to the
** hardware and timers.
**
** \param pLedControl Pointer to LED Control object.
**
** \return N/A
*/

void
ath_led_scan_end (struct ath_led_control *pLedControl)
{
    DPRINTF(pLedControl->sc, ATH_DEBUG_LED, "%s: suspended=%d disabled=%d state=%s saved=%s\n", 
        __func__, 
        pLedControl->ledSuspended, 
        pLedControl->ledControlDisabled,
        HalLedStateName[pLedControl->halState],
        HalLedStateName[pLedControl->saveState]);

    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    // Ignore changes to the LED state if the driver is suspended (S3/S4 state)
    if (pLedControl->ledSuspended)
    {
        // Release access to Hw
        ATH_LED_UNLOCK(pLedControl);

        return;
    }

    if (! pLedControl->ledControlDisabled)
    {
        // Need to restore LED state only if the amount of time for which the 
        // scan blinking pattern should be shown is not fixed.
        if ((pLedControl->halState == HAL_LED_SCAN) &&
            (pLedControl->scanStateDuration == 0))
        {
            // Restore the blinking pattern to that active before the scan started
            if (pLedControl->saveState == HAL_LED_INIT)
            {
                // If scan started from initialization state (HAL_LED_INIT),
                // system is now "ready" (HAL_LED_READY).
                ath_led_state_transition(pLedControl, HAL_LED_READY);
            }
            else
            {
                ath_led_state_transition(pLedControl, pLedControl->saveState);
            }
        }
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);
}

/******************************************************************************/
/*!
**  \brief Enable LED processing
**
** Called by power processing, lets the LED task know that the chip is powered
** up and LED processing should commence.  Sets the disabled state to 0,
** initializes the GPIO pins, and calls the HAL state transition processing.
**
** \param pLedControl Pointer to LED Control object
**
** \return N/A
*/

void
ath_led_enable (struct ath_led_control *pLedControl)
{
    DPRINTF(pLedControl->sc, ATH_DEBUG_LED, "%s: suspended=%d disabled=%d saved state=%s\n", 
        __func__, 
        pLedControl->ledSuspended, 
        pLedControl->ledControlDisabled,
        HalLedStateName[pLedControl->saveState]);

    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    // Ignore changes to the LED state if the driver is suspended (S3/S4 state)
    if (pLedControl->ledSuspended)
    {
        // Release access to Hw
        ATH_LED_UNLOCK(pLedControl);

        return;
    }

    if (pLedControl->ledControlDisabled)
    {
        // Reconfigure gpio pins before clearing flag ledControlDisabled
        ath_configure_gpio_pins(pLedControl);

        pLedControl->ledControlDisabled = 0;

        // Restore LED state to what it was before radio was turned off
        ath_led_state_transition(pLedControl, pLedControl->saveState);
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);
}

/******************************************************************************/
/*!
**  \brief Disable LED Processing
**
** This function disables LED processing by setting the HAL state to 
** reset.  This is called from power off and ath termination processing.
** This routine uses the spin lock to control access to the hardware and
** timer functions.
**
** \param pLedControl Pointer to LED Control object
**
** \return N/A
*/

void
ath_led_disable (struct ath_led_control *pLedControl)
{
    DPRINTF(pLedControl->sc, ATH_DEBUG_LED, "%s: suspended=%d disabled=%d state=%s\n", 
        __func__, 
        pLedControl->ledSuspended, 
        pLedControl->ledControlDisabled,
        HalLedStateName[pLedControl->halState]);

    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    // Ignore changes to the LED state if the driver is suspended (S3/S4 state)
    if (pLedControl->ledSuspended)
    {
        // Release access to Hw
        ATH_LED_UNLOCK(pLedControl);

        return;
    }

    if (! pLedControl->ledControlDisabled)
    {
        // Save current LED state so it can be restored when radio is turned on.
        // If we are currently scanning we keep the state saved when scanning
        // started, as there's no guarantee we will still be scanning when the
        // LED is re-enabled.
        if (pLedControl->halState != HAL_LED_SCAN)
        {
            pLedControl->saveState = pLedControl->halState;
        }

        ath_led_state_transition(pLedControl, HAL_LED_RESET);

        pLedControl->ledControlDisabled = 1;

        // Tristate gpio pins not in use
        ath_tristate_gpio_pins(pLedControl);
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);
}

/******************************************************************************/
/*!
**  \brief Process Data Flow Statistics
**
** This routine is called from the transmit and receive chains.  It will
** pass in the number of bytes received/transmitted, and the routine will
** change the state of the LED as specified by the internal rate tables.
** This routine uses the spin lock to control access to the timers/hardware
**
** \param pLedControl Pointer to LED Control object.
** \param byteCount Number of bytes being transmitted/received
**
** \return N/A
*/

void
ath_led_report_data_flow (struct ath_led_control *pLedControl, int32_t byteCount)
{
    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    // Ignore changes to the LED state if the driver is suspended (S3/S4 state)
    if (pLedControl->ledSuspended)
    {
        // Release access to Hw
        ATH_LED_UNLOCK(pLedControl);

        return;
    }

    // If connected, check whether we must blink activity LED
    if ((pLedControl->halState == HAL_LED_RUN) && (! pLedControl->sc->sc_scanning))
    {
        // Increment number of bytes transmitted/received in an update period
        atomic_add(byteCount, &pLedControl->dataBytes);

        if (pLedControl->swFastBlinkingControl)
        {
            // If activity LED is not blinking, calculate new blinking rate
            // (not subject to a minimum threshold) and start blinking it.
            if (! ath_timer_is_active(&pLedControl->ledFastTimer))
            {
                // Update fast timer period. 
                // Start timer only if LEDs will indeed blink. Some customers
                // specify that LEDs should not blink if no traffic is present.
                // Do not apply minimum traffic threshold.
                if (ath_update_led_rate(pLedControl, 
                                        0,
                                        0,
                                        &pLedControl->stateDuration[LED_ON], 
                                        &pLedControl->stateDuration[LED_OFF]))
                {
                    // Data started flowing again; change LED state and start blinking
                    ath_flip_blinking_led_state(pLedControl, &pLedControl->blinkingControlConnected);

                    // Set the LED blinking timer.
                    ath_set_timer_period(&pLedControl->ledFastTimer, 
                        pLedControl->stateDuration[pLedControl->blinkingControlConnected.ledState]);
                    ath_start_timer(&pLedControl->ledFastTimer);
                }
            }
        }
    }

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);
}

/**************************************************************************
 * ath_led_suspend
 *
 * Notifies the LED module of a transition to S3/S4.
 * This notification needed so that this module can ignore a call to 
 * ath_led_enable (caused by ath_radio_enable) made immediately before the 
 * transition to S3/S4 actually occurs. The call to ath_radio_enable is made
 * only to ensure the chip is awake thus avoid write operations from accessing
 * the MAC while the chip is asleep (leading to an NMI), and not to change the 
 * LED state.
 */
void
ath_led_suspend (ath_dev_t dev)
{
	struct ath_softc          *sc = ATH_DEV_TO_SC(dev);
    struct ath_led_control    *pLedControl = &sc->sc_led_control;

    DPRINTF(pLedControl->sc, ATH_DEBUG_LED, "%s: suspended=%d disabled=%d state=%s\n", 
        __func__, 
        pLedControl->ledSuspended, 
        pLedControl->ledControlDisabled,
        HalLedStateName[pLedControl->halState]);

    // Acquire access to Hw. Synchronize with ath_led_led_heartbeat and ath_led_fast_blinking
    ATH_LED_LOCK(pLedControl);

    pLedControl->ledSuspended       = 1;
    pLedControl->ledControlDisabled = 1;

    // Release access to Hw
    ATH_LED_UNLOCK(pLedControl);

    // Halt LED module. 
    // This will stop any LED-related timers that might be running.
    ath_led_halt_control(pLedControl);
}

#endif
