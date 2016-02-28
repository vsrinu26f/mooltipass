/* CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at src/license_cddl-1.0.txt
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at src/license_cddl-1.0.txt
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*! \file   mooltipass.c
 *  \brief  main file
 *  Copyright [2014] [Mathieu Stephan]
 */
#include <util/atomic.h>
#include <avr/eeprom.h>
#include <avr/boot.h>
#include <stdlib.h>
#include <avr/io.h>
#include <string.h>
#include <stdio.h>
#include "smart_card_higher_level_functions.h"
#include "touch_higher_level_functions.h"
#include "gui_smartcard_functions.h"
#include "gui_screen_functions.h"
#include "gui_basic_functions.h"
#include "logic_aes_and_comms.h"
#include "eeprom_addresses.h"
#include "define_printouts.h"
#include "watchdog_driver.h"
#include "logic_smartcard.h"
#include "usb_cmd_parser.h"
#include "timer_manager.h"
#include "bitstreammini.h"
#include "oled_wrapper.h"
#include "logic_eeprom.h"
#include "mini_inputs.h"
#include "mooltipass.h"
#include "interrupts.h"
#include "smartcard.h"
#include "flash_mem.h"
#include "defines.h"
#include "delays.h"
#include "utils.h"
#include "tests.h"
#include "touch.h"
#include "anim.h"
#include "spi.h"
#include "pwm.h"
#include "usb.h"
#include "rng.h"

// Tutorial led masks and touch filtering
#if !defined(FLASH_CHIP_1M)
static const uint8_t tutorial_masks[] __attribute__((__progmem__)) =
{
    0,                              TOUCH_PRESS_MASK,       // Welcome screen
    LED_MASK_WHEEL,                 RETURN_RIGHT_PRESSED,   // Show you around...
    LED_MASK_WHEEL,                 RETURN_RIGHT_PRESSED,   // Display hints
    LED_MASK_LEFT|LED_MASK_RIGHT,   RETURN_WHEEL_PRESSED,   // Circular segments
    LED_MASK_LEFT|LED_MASK_RIGHT,   RETURN_WHEEL_PRESSED,   // Wheel interface
    0,                              TOUCH_PRESS_MASK,       // That's all!
};
#endif
// Define the bootloader function
bootloader_f_ptr_type start_bootloader = (bootloader_f_ptr_type)0x3800;
// Flag to inform if the caps lock timer is armed
volatile uint8_t wasCapsLockTimerArmed = FALSE;
// Boolean to know if user timeout is enabled
uint8_t mp_timeout_enabled = FALSE;
// Flag set by anything to signal activity
uint8_t act_detected_flag = FALSE;


/*! \fn     disableJTAG(void)
*   \brief  Disable the JTAG module
*/
static inline void disableJTAG(void)
{
    unsigned char temp;

    temp = MCUCR;
    temp |= (1<<JTD);
    MCUCR = temp;
    MCUCR = temp;
}

/*! \fn     smallForLoopBasedDelay(void)
*   \brief  Small delay used at the mooltipass start
*/
void smallForLoopBasedDelay(void)
{
    for (uint16_t i = 0; i < 20000; i++) asm volatile ("NOP");
}

/*! \fn     main(void)
*   \brief  Main function
*/
int main(void)
{
    uint16_t current_bootkey_val = eeprom_read_word((uint16_t*)EEP_BOOTKEY_ADDR);
    #if !defined(MINI_VERSION)
        RET_TYPE touch_init_result;
    #endif
    RET_TYPE flash_init_result;
    RET_TYPE card_detect_ret;
    uint8_t fuse_ok = TRUE;
    
    // Disable JTAG to gain access to pins, set prescaler to 1 (fuses not set)
    #if !defined(PRODUCTION_KICKSTARTER_SETUP)
        disableJTAG();
        CPU_PRESCALE(0);
    #endif

    #if defined(MINI_CLICK_BETATESTERS_SETUP)
        // We don't check fuses in beta testers units
    #elif defined(PREPRODUCTION_KICKSTARTER_SETUP)
        // Check fuse settings: boot reset vector, 2k words, SPIEN, BOD 4.3V, programming & ver disabled >> http://www.engbedded.com/fusecalc/
        if ((boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) != 0xFF) || (boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS) != 0xD9) || (boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS) != 0xF8) || (boot_lock_fuse_bits_get(GET_LOCK_BITS) != 0xFC))
        {
            fuse_ok = FALSE;
        }
    #elif defined(PRODUCTION_TEST_SETUP)
        // Check fuse settings: 2k words, SPIEN, BOD 4.3V, programming & ver disabled >> http://www.engbedded.com/fusecalc/
        if ((boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) != 0xFF) || (boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS) != 0xD9) || (boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS) != 0xF8))
        {
            fuse_ok = FALSE;
        }
    #else
        // Check fuse settings: 2k words, SPIEN, BOD 4.3V, programming & ver disabled >> http://www.engbedded.com/fusecalc/
        if ((boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS) != 0xFF) || (boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS) != 0xD8) || (boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS) != 0xF8) || (boot_lock_fuse_bits_get(GET_LOCK_BITS) != 0xFC))
        {
            fuse_ok = FALSE;
        }
    #endif
    
    #if defined(HARDWARE_OLIVIER_V1)
        // Check if PB5 is low to start electrical test
        DDRB &= ~(1 << 5); PORTB |= (1 << 5);
        smallForLoopBasedDelay();
        if (!(PINB & (1 << 5)))
        {
            // Test result, true by default
            uint8_t test_result = TRUE;
            // Leave flash nS off
            DDR_FLASH_nS |= (1 << PORTID_FLASH_nS);
            PORT_FLASH_nS |= (1 << PORTID_FLASH_nS);
            // Set PORTD as output, leave PORTID_OLED_SS high
            DDRD |= 0xFF; PORTD |= 0xFF;
            // All other pins are input by default, run our test
            for (uint8_t i = 0; i < 4; i++)
            {
                PORTD |= 0xFF;
                smallForLoopBasedDelay();
                if (!(PINF & (0xC3)) || !(PINC & (1 << 6)) || !(PINE & (1 << 6)) || !(PINB & (1 << 4)))
                {
                    test_result = FALSE;
                }
                PORTD &= (1 << PORTID_OLED_SS);
                smallForLoopBasedDelay();
                if ((PINF & (0xC3)) || (PINC & (1 << 6)) || (PINE & (1 << 6)) || (PINB & (1 << 4)))
                {
                    test_result = FALSE;
                }
            }               
            // PB6 as test result output
            DDRB |= (1 << 6);
            // If test successful, light green LED
            if ((test_result == TRUE) && (fuse_ok == TRUE))
            {
                PORTB |= (1 << 6);
            } 
            else
            {
                PORTB &= ~(1 << 6);
            }
            while(1);
        }
    #elif defined(MINI_VERSION)
        // Check if PD0 is low to start electrical test
        DDRD &= ~(1 << 0); PORTD |= (1 << 0);
        smallForLoopBasedDelay();
        if (!(PIND & (1 << 0)))
        {
        }
    #endif    
    
    // This code will only be used for developers and beta testers
    #if !defined(PRODUCTION_SETUP) && !defined(PRODUCTION_KICKSTARTER_SETUP)
        // Check if we were reset and want to go to the bootloader
        if (current_bootkey_val == BOOTLOADER_BOOTKEY)
        {
            // Disable WDT
            wdt_reset();
            wdt_clear_flag();
            wdt_change_enable();
            wdt_stop();
            // Store correct bootkey
            eeprom_write_word((uint16_t*)EEP_BOOTKEY_ADDR, CORRECT_BOOTKEY);
            // Jump to bootloader
            start_bootloader();
        }
        
        // Check if there was a change in the mooltipass setting storage to reset the parameters to their correct values
        if (getMooltipassParameterInEeprom(USER_PARAM_INIT_KEY_PARAM) != USER_PARAM_CORRECT_INIT_KEY)
        {
            mooltipassParametersInit();
            setMooltipassParameterInEeprom(USER_PARAM_INIT_KEY_PARAM, USER_PARAM_CORRECT_INIT_KEY);
        }
    #endif    

    // First time initializations for Eeprom (first boot at production or flash layout changes for beta testers)
    if (current_bootkey_val != CORRECT_BOOTKEY)
    {
        // Erase Mooltipass parameters
        mooltipassParametersInit();
        // Set bootloader password bool to FALSE
        eeprom_write_byte((uint8_t*)EEP_BOOT_PWD_SET, FALSE);
    }

    /* Check if a card is inserted in the Mooltipass to go to the bootloader */
    #ifdef AVR_BOOTLOADER_PROGRAMMING
        #ifndef MINI_VERSION
            /* Disable JTAG to get access to the pins */
            disableJTAG();
            /* Init SMC port */
            initPortSMC();
            /* Delay for detection */
            smallForLoopBasedDelay();
            #if defined(HARDWARE_V1)
            if (PIN_SC_DET & (1 << PORTID_SC_DET))
            #elif defined(HARDWARE_OLIVIER_V1) || defined (MINI_VERSION)
            if (!(PIN_SC_DET & (1 << PORTID_SC_DET)))
            #endif
            {
                uint16_t tempuint16;
                /* What follows is a copy from firstDetectFunctionSMC() */
                /* Enable power to the card */
                PORT_SC_POW &= ~(1 << PORTID_SC_POW);
                /* Default state: PGM to 0 and RST to 1 */
                PORT_SC_PGM &= ~(1 << PORTID_SC_PGM);
                DDR_SC_PGM |= (1 << PORTID_SC_PGM);
                PORT_SC_RST |= (1 << PORTID_SC_RST);
                DDR_SC_RST |= (1 << PORTID_SC_RST);
                /* Activate SPI port */
                PORT_SPI_NATIVE &= ~((1 << SCK_SPI_NATIVE) | (1 << MOSI_SPI_NATIVE));
                DDRB |= (1 << SCK_SPI_NATIVE) | (1 << MOSI_SPI_NATIVE);
                setSPIModeSMC();
                /* Let the card come online */
                smallForLoopBasedDelay();
                /* Check smart card FZ */
                readFabricationZone((uint8_t*)&tempuint16);
                if ((swap16(tempuint16)) != SMARTCARD_FABRICATION_ZONE)
                {
                    removeFunctionSMC();
                    start_bootloader();
                }
                else
                {
                    removeFunctionSMC();
                }
            }
        #else
            /* Disable JTAG to get access to the pins */
            disableJTAG();
            /* Pressing center joystick starts the bootloader */
            DDR_JOYSTICK &= ~(1 << PORTID_JOY_CENTER);
            PORT_JOYSTICK |= (1 << PORTID_JOY_CENTER);
            /* Small delay for detection */
            smallForLoopBasedDelay();
            /* Check if low */
            if (!(PIN_JOYSTICK & (1 << PORTID_JOY_CENTER)))
            {
                start_bootloader();
            }  
        #endif          
    #endif

    initPortSMC();                      // Initialize smart card port
    initPwm();                          // Initialize PWM controller
    initIRQ();                          // Initialize interrupts
    powerSettlingDelay();               // Let the power settle   
    initUsb();                          // Initialize USB controller
    powerSettlingDelay();               // Let the USB 3.3V LDO rise
    initI2cPort();                      // Initialize I2C interface
    rngInit();                          // Initialize avrentropy library
    oledInitIOs();                      // Initialize OLED input/outputs
    spiUsartBegin();                    // Start USART SPI at 8MHz
    #if defined(MINI_VERSION)           // Only executed for the mini
        initMiniInputs();               // Init Mini Inputs
    #endif

    // If offline mode isn't enabled, wait for device to be enumerated
    if (getMooltipassParameterInEeprom(OFFLINE_MODE_PARAM) == FALSE)
    {
        while(!isUsbConfigured());      // Wait for host to set configuration
    }    
    
    // Set correct timeout_enabled val
    mp_timeout_enabled = getMooltipassParameterInEeprom(LOCK_TIMEOUT_ENABLE_PARAM);

    // Launch the before flash initialization tests
    #ifdef TESTS_ENABLED
        beforeFlashInitTests();
    #endif
    
    // Check if we can initialize the Flash memory
    flash_init_result = initFlash();
    
    // Launch the after flash initialization tests
    #ifdef TESTS_ENABLED
        afterFlashInitTests();
    #endif
    
    // Set up OLED now that USB is receiving full 500mA.
    oledBegin(FONT_DEFAULT);
    
    // First time initializations for Flash (first time power up at production)
    if (current_bootkey_val != CORRECT_BOOTKEY)
    {
        // Erase everything in flash
        chipErase();
        // Erase # of cards and # of users
        firstTimeUserHandlingInit();
    }
    
    // Check if we can initialize the touch sensing element
    #if !defined(MINI_VERSION)
        touch_init_result = initTouchSensing();
    #endif

    // Enable proximity detection
    #if !defined(HARDWARE_V1) && !defined(V2_DEVELOPERS_BOTPCB_BOOTLOADER_SETUP) && !defined(MINI_VERSION)
        activateProxDetection();
    #endif
    
    // Launch the after touch initialization tests
    #ifdef TESTS_ENABLED
        afterTouchInitTests();
    #endif
    
    // Test procedure to check that all HW is working
    //#define FORCE_PROD_TEST
    #if defined(PRODUCTION_SETUP) || defined(PRODUCTION_KICKSTARTER_SETUP) || defined(FORCE_PROD_TEST)
        if (current_bootkey_val != CORRECT_BOOTKEY)
        {
            uint8_t test_result_ok = TRUE;
            RET_TYPE temp_rettype;     
            // Wait for USB host to upload bundle, which then sets USER_PARAM_INIT_KEY_PARAM
            //#ifdef PRODUCTION_KICKSTARTER_SETUP
            while(getMooltipassParameterInEeprom(USER_PARAM_INIT_KEY_PARAM) != 0x94)
            {
                usbProcessIncoming(USB_CALLER_MAIN);
            }
            //#endif
            // Bundle uploaded, start the screen
            stockOledBegin(FONT_DEFAULT);
            oledWriteActiveBuffer();
            oledSetXY(0,0);
            // LEDs ON, to check
            setPwmDc(MAX_PWM_VAL);
            touchDetectionRoutine(0);
            guiDisplayRawString(ID_STRING_TEST_LEDS_CH);
            // Check flash init
            if (flash_init_result != RETURN_OK)
            {
                 guiDisplayRawString(ID_STRING_TEST_FLASH_PB);
                 test_result_ok = FALSE;
            }
            // Check touch init
            if (touch_init_result != RETURN_OK)
            {
                guiDisplayRawString(ID_STRING_TEST_TOUCH_PB);
                test_result_ok = FALSE;
            }
            // Check fuse setting
            if (fuse_ok != TRUE)
            {
                test_result_ok = FALSE;
                guiDisplayRawString(ID_STRING_FUSE_PB);
            }
            // Touch instructions
            guiDisplayRawString(ID_STRING_TEST_INST_TCH);
            // Check prox
            while(!(touchDetectionRoutine(0) & RETURN_PROX_DETECTION));
            guiDisplayRawString(ID_STRING_TEST_DET);
            activateGuardKey();
            // Check left
            while(!(touchDetectionRoutine(0) & RETURN_LEFT_PRESSED));
            guiDisplayRawString(ID_STRING_TEST_LEFT);
            // Check wheel
            while(!(touchDetectionRoutine(0) & RETURN_WHEEL_PRESSED));
            guiDisplayRawString(ID_STRING_TEST_WHEEL);
            // Check right
            while(!(touchDetectionRoutine(0) & RETURN_RIGHT_PRESSED));
            guiDisplayRawString(ID_STRING_TEST_RIGHT);
            // Insert card
            guiDisplayRawString(ID_STRING_TEST_CARD_INS);
            while(isCardPlugged() != RETURN_JDETECT);
            temp_rettype = cardDetectedRoutine();
            // Check card
            if (!((temp_rettype == RETURN_MOOLTIPASS_BLANK) || (temp_rettype == RETURN_MOOLTIPASS_USER)))
            {
                guiDisplayRawString(ID_STRING_TEST_CARD_PB);
                test_result_ok = FALSE;
            }
            // Display result
            uint8_t script_return = RETURN_OK;
            if (test_result_ok == TRUE)
            {
                // Inform script of success
                usbSendMessage(CMD_FUNCTIONAL_TEST_RES, 1, &script_return);
                #if !defined(PREPRODUCTION_KICKSTARTER_SETUP)
                    // Wait for password to be set
                    while(eeprom_read_byte((uint8_t*)EEP_BOOT_PWD_SET) != BOOTLOADER_PWDOK_KEY)
                    {
                        usbProcessIncoming(USB_CALLER_MAIN);
                    }
                #endif
            }
            else
            {
                // Set correct bool
                script_return = RETURN_NOK;
                // Display test result
                guiDisplayRawString(ID_STRING_TEST_NOK);
                // Inform script of failure
                usbSendMessage(CMD_FUNCTIONAL_TEST_RES, 1, &script_return);
                while(1);
            }
        }
    #endif
    
    // Stop the Mooltipass if we can't communicate with the flash or the touch interface
    #if defined(HARDWARE_OLIVIER_V1)
        #if defined(PRODUCTION_KICKSTARTER_SETUP) || defined(PREPRODUCTION_KICKSTARTER_SETUP)
            while ((flash_init_result != RETURN_OK) || (touch_init_result != RETURN_OK) || (fuse_ok != TRUE));
        #elif defined(V2_DEVELOPERS_BOTPCB_BOOTLOADER_SETUP)
            while ((flash_init_result != RETURN_OK) || (touch_init_result != RETURN_NOK));
        #else
            while ((flash_init_result != RETURN_OK) || (touch_init_result != RETURN_OK));
        #endif
    #elif defined(MINI_VERSION)
        while ((flash_init_result != RETURN_OK) || (fuse_ok != TRUE));
    #endif
    
    // First time initializations done.... write correct value in eeprom
    if (current_bootkey_val != CORRECT_BOOTKEY)
    {
        // Store correct bootkey
        eeprom_write_word((uint16_t*)EEP_BOOTKEY_ADDR, CORRECT_BOOTKEY);
    }
    
    // Write inactive buffer by default
    oledWriteInactiveBuffer();    
    
    // First boot tutorial, only on big flash versions
    #ifndef FLASH_CHIP_1M
    if (getMooltipassParameterInEeprom(TUTORIAL_BOOL_PARAM) != FALSE)
    {
        #ifndef MINI_VERSION
            uint8_t tut_led_mask, press_filter;
            activateGuardKey();
            activityDetectedRoutine();
            for (uint8_t i = 0; i < sizeof(tutorial_masks)/2; i++)
            {
                tut_led_mask = pgm_read_byte(&tutorial_masks[i*2]);
                press_filter = pgm_read_byte(&tutorial_masks[i*2+1]);
                oledBitmapDrawFlash(0, 0, i + BITMAP_TUTORIAL_1, OLED_SCROLL_UP);
                while(!(touchDetectionRoutine(tut_led_mask) & press_filter));
                touchInhibitUntilRelease();
            }
        #endif
        setMooltipassParameterInEeprom(TUTORIAL_BOOL_PARAM, FALSE);
    }
    #endif

    // Go to startup screen
    guiSetCurrentScreen(SCREEN_DEFAULT_NINSERTED);
    guiGetBackToCurrentScreen();
        
    // Launch the after HaD logo display tests
    #ifdef TESTS_ENABLED
        afterHadLogoDisplayTests();  
    #endif
    
    #if defined(HARDWARE_OLIVIER_V1)
        // Let's fade in the LEDs
        touchDetectionRoutine(0);
        for (uint16_t i = 0; i < MAX_PWM_VAL; i++)
        {
            setPwmDc(i);
            timerBasedDelayMs(0);
        }
        activityDetectedRoutine();
        launchCalibrationCycle();
        touchClearCurrentDetections();
    #endif

    #if defined(MINI_VERSION)
        while(1)
        {
            usbProcessIncoming(USB_CALLER_MAIN);
//             for(uint8_t i = 0; i < 128-16; i++)
//             {
//                 if(isMiniDirectionPressed(PORTID_JOY_UP) == RETURN_DET)
//                     miniOledDrawRectangle(40,0,5,5,TRUE);
//                 else
//                     miniOledDrawRectangle(40,0,5,5,FALSE);  
//                 if(isMiniDirectionPressed(PORTID_JOY_DOWN) == RETURN_DET)
//                     miniOledDrawRectangle(45,0,5,5,TRUE);
//                 else
//                     miniOledDrawRectangle(45,0,5,5,FALSE);  
//                 if(isMiniDirectionPressed(PORTID_JOY_LEFT) == RETURN_DET)
//                     miniOledDrawRectangle(50,0,5,5,TRUE);
//                 else
//                     miniOledDrawRectangle(50,0,5,5,FALSE);  
//                 if(isMiniDirectionPressed(PORTID_JOY_RIGHT) == RETURN_DET)
//                     miniOledDrawRectangle(55,0,5,5,TRUE);
//                 else
//                     miniOledDrawRectangle(55,0,5,5,FALSE);  
//                 if(isMiniDirectionPressed(PORTID_JOY_CENTER) == RETURN_DET)
//                     miniOledDrawRectangle(60,0,5,5,TRUE);
//                 else
//                     miniOledDrawRectangle(60,0,5,5,FALSE);    
//                 if(isWheelClicked() == RETURN_DET)
//                     miniOledDrawRectangle(65,0,5,5,TRUE);
//                 else
//                     miniOledDrawRectangle(65,0,5,5,FALSE);  
//                     
//                 if (!(PIN_WHEEL_A & (1 << PORTID_WHEEL_A)))    
//                     miniOledDrawRectangle(100,0,5,5,TRUE);
//                 else
//                     miniOledDrawRectangle(100,0,5,5,FALSE);   
//                 if (!(PIN_WHEEL_B & (1 << PORTID_WHEEL_B)))    
//                     miniOledDrawRectangle(105,0,5,5,TRUE);
//                 else
//                     miniOledDrawRectangle(105,0,5,5,FALSE);            
//                   
//                 //bitstream_mini_t tata;
//                 //miniOledBitmapDrawRaw(i, i, &tata, 0);
//                 //miniOledDrawRectangle(i,i,1,1,TRUE);
//                 //miniOledBitmapDrawFlash(i, 16, 0, 0);
//                 miniOledFlushEntireBufferToDisplay();
//                 //timerBasedDelayMs(100);
//                 miniOledDrawRectangle(i,16,16,16,FALSE);    
//             }
        }
    #endif
    
    // Inhibit touch inputs for the first 2 seconds
    activateTimer(TIMER_TOUCH_INHIBIT, 2000);
    while (1)
    {
        // Process possible incoming USB packets
        usbProcessIncoming(USB_CALLER_MAIN);
        
        // Launch activity detected routine if flag is set
        if (act_detected_flag != FALSE)
        {
            if (isScreenSaverOn() == TRUE)
            {
                guiGetBackToCurrentScreen();
            }
            activityDetectedRoutine();
            act_detected_flag = FALSE;
        }
        
        // Call GUI routine once the touch input inhibit timer is finished
        if (hasTimerExpired(TIMER_TOUCH_INHIBIT, FALSE) == TIMER_EXPIRED)
        {
            guiMainLoop();
        }
        
        // If we are running the screen saver
        if (isScreenSaverOn() == TRUE)
        {
            animScreenSaver();
        }
        
        // If the USB bus is in suspend (computer went to sleep), lock device
        if ((hasTimerExpired(TIMER_USB_SUSPEND, TRUE) == TIMER_EXPIRED) && ((getCurrentScreen() == SCREEN_DEFAULT_INSERTED_NLCK) || (getCurrentScreen() == SCREEN_MEMORY_MGMT)))
        {
            handleSmartcardRemoved();
            guiDisplayInformationOnScreenAndWait(ID_STRING_PC_SLEEP);
            guiSetCurrentScreen(SCREEN_DEFAULT_INSERTED_LCK);
            // If the screen saver is on, clear screen contents
            if(isScreenSaverOn() == TRUE)
            {
                oledClear();
                oledDisplayOtherBuffer();
                oledClear();
            }
            else
            {
                guiGetBackToCurrentScreen();                
            }
        }
        
        // Check if a card just got inserted / removed
        card_detect_ret = isCardPlugged();
        
        // Do appropriate actions on smartcard insertion / removal
        if (card_detect_ret == RETURN_JDETECT)
        {
            // Light up the Mooltipass and call the dedicated function
            activityDetectedRoutine();
            handleSmartcardInserted();
        }
        else if (card_detect_ret == RETURN_JRELEASED)
        {
            // Light up the Mooltipass and call the dedicated function
            activityDetectedRoutine();
            handleSmartcardRemoved();
            
            // Set correct screen
            guiDisplayInformationOnScreenAndWait(ID_STRING_CARD_REMOVED);
            guiSetCurrentScreen(SCREEN_DEFAULT_NINSERTED);
            guiGetBackToCurrentScreen();
        }
        
        #define TWO_CAPS_TRICK
        #ifdef TWO_CAPS_TRICK
        // Two quick caps lock presses wakes up the device        
        if ((hasTimerExpired(TIMER_CAPS, FALSE) == TIMER_EXPIRED) && (getKeyboardLeds() & HID_CAPS_MASK) && (wasCapsLockTimerArmed == FALSE))
        {
            wasCapsLockTimerArmed = TRUE;
            activateTimer(TIMER_CAPS, CAPS_LOCK_DEL);
        }
        else if ((hasTimerExpired(TIMER_CAPS, FALSE) == TIMER_RUNNING) && !(getKeyboardLeds() & HID_CAPS_MASK))
        {
            if (isScreenSaverOn() == TRUE)
            {
                guiGetBackToCurrentScreen();
            }
            activityDetectedRoutine();
        }
        else if ((hasTimerExpired(TIMER_CAPS, FALSE) == TIMER_EXPIRED) && !(getKeyboardLeds() & HID_CAPS_MASK))
        {
            wasCapsLockTimerArmed = FALSE;            
        }
        #endif
        
        // If we have a timeout lock
        if ((mp_timeout_enabled == TRUE) && (hasTimerExpired(SLOW_TIMER_LOCKOUT, TRUE) == TIMER_EXPIRED))
        {
            guiSetCurrentScreen(SCREEN_DEFAULT_INSERTED_LCK);
            guiGetBackToCurrentScreen();
            handleSmartcardRemoved();
        }
    }
}
