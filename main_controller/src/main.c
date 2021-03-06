#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "configBits.h"
#include "defines.h"
#include "macros.h"
#include "RTCC.h"
#include "UART.h"
#include "interruptRoutines.h"
#include "startup_setup.h"
#include "timers.h"
#include "combine.h"
#include "globals.h"
#include "PWM.h"
#include "EEPROM.h"


int main(void) 
{
    startup_setup();
    
    //initialize variables
    seconds = 0;
    minutes = 0;
    hours = 0;
    secondsCopy = seconds;
    minutesCopy = minutes;
    hoursCopy = hours;

    mode_current = CLOCK_MODE;
    clock_init();

    digit_ones = 0;
    digit_tens = 0;
    digit_hundreds = 0;
    digit_thousands = 0;
    
    controlColon = ':';
    controlPoint = '.';

    clearBit(LATC, DEBUG_LED_PIN);
    
    uint8_t digit_place;
    digit_place = 1;
    
    uint8_t BCDtoDecimal_high;
    uint8_t BCDtoDecimal_low;
    
    uint8_t rotateCounter = 0;
    
    uint8_t encoderRotationCopy = 0;
    
    uint8_t setClockFlag = SET_ALARM;
    uint8_t ok_counter = 0;
    
    uint8_t exceed_24hr = FALSE;
   
    uint8_t hour_format = FORMAT_24;
    
    uint8_t am_pm = AM_TIME;
    
    uint8_t duty_cycle = 60;
    uint8_t duty_cycle_temp = 60;
    
    bool previous_sleepFlag = FALSE;
    
    uint8_t save_recall = SAVE;
    
    uint8_t saved_counter = 0;
    
    
    
    while(1) {
            
        if(updateDisplay == TRUE) {
            updateDigits();
            updateDisplay = FALSE;
        }
        
        
        if(checkBit(PORTC, HOUR_FORMAT_PIN) == FORMAT_24)
        {
            hour_format = FORMAT_24;
        }
        else if(checkBit(PORTC, HOUR_FORMAT_PIN) == FORMAT_12)
        {
            hour_format = FORMAT_12;
        }
        
        
        //get modes
        if(isPressed(&PORTA, MODE_PIN)) {
            ++mode_current;
            if((mode_current == MODE_ROLLBACK)  || 
               (mode_current == CLOCK_MODE)     || 
               (mode_current == ADJUST_MODE)    || 
               (mode_current == (ADJUST_MODE + 1))) {
                
                mode_current = CLOCK_MODE;
            }
        }
        if(isPressed(&PORTC, ADJUST_SOUND_PIN))
        {
            duty_cycle_temp = duty_cycle;
            __delay_ms(MODE_DISPLAY_DELAY_MS);
            mode_current = ADJUST_MODE;
        }
        if(!(mode_current == ADJUST_MODE))
        {
            PWM_off(BUZZER_PIN);
            PWM_dutyCycle(BUZZER_PIN, duty_cycle);
        }
        
        
        //get submodes
        if(isPressed(&PORTA, SET_RUN_PIN)) {
            ++subMode;
            if(subMode == SUBMODE_ROLLBACK) {
                subMode = SET_MODE;
            }
            
            if(subMode_previous == RUN_MODE)
            {
                subMode = SET_MODE;
            }
        }
        
        // sleep function
        if(sleepFlag == TRUE)
        {
            disableInterrupt();
            TX_char('x');
            TX_char('x');
            TX_char('x');
            TX_char('x');
            TX_char('x');
            TX_char('x');
            TX_char('x');
            enableInterrupt();
            SLEEP();
            previous_sleepFlag = sleepFlag;
        }
        else if((previous_sleepFlag == TRUE) && (sleepFlag == FALSE ))
        {
            // transmit the wakeup characters
            previous_sleepFlag = sleepFlag;
            TX_word("1234");
        }
        
        
        if(isPressed(&PORTA, STOP_RESUME_PIN)) {
            ++stopResumeFlag;
            if(stopResumeFlag == STOP_RESUME_ROLLBACK) {
                stopResumeFlag = STOP_MODE;
            }
        }
        
        
        
        if(mode_current == CLOCK_MODE){
            if(mode_previous != mode_current) { // initiate when entering a mode
                clock_init();
            }
            mode_previous = mode_current;
            
            if(subMode == SET_MODE || subMode == RESET_MODE) {
                if(subMode_previous != subMode) {
                    TMR0H = TIMER_REG_HIGH_BYTE;
                    TMR0L = TIMER_REG_LOW_BYTE;
                    clearBit(PIR0, 5);  // clear flag
                    setBit(T0CON0, 7);
                    setBit(PIE0, 5); // enable timer0 interrupt
                    
                    stopResumeFlag = RESUME_MODE;
                    
                    digit_place = 1;
                    
                    ok_counter = 1;
                }
                subMode_previous = subMode;
                
                while((ok_counter == 1) && (isPressed(&PORTB, OK_PIN) == 0))
                {
                    if(isPressed(&PORTB, SET_CLOCK_ALARM_PIN))
                    {
                        ++setClockFlag;
                        if(setClockFlag == SET_ALARM_CLOCK_ROLLBACK)
                        {
                           setClockFlag = SET_ALARM;
                        }
                    }
                        
                    if(setClockFlag == SET_ALARM)
                    {
                        TX_word("aLar"); 
                    }
                    else if(setClockFlag == SET_CLOCK)
                    {
                        TX_word("cloc"); 
                    }     
                }
                ok_counter = 2;
               
               // if set mode, just display the set time
               // if reset mode, display 00.00
               if(subMode == RESET_MODE)
               {
                   setMinutesClock = 0;
                   setHoursClock = 0;
                   subMode = SET_MODE;  // Rollback to set mode. If not, the display will always be 00.00
               }
               else if(subMode == SET_MODE)
               {
                   ;    // do nothing because the default is just display the set time
               }
                
                
                // check run/ok button
                if(isPressed(&PORTB, OK_PIN))
                {
                    if(setClockFlag == SET_ALARM)
                    {
                        if(exceed_24hr == TRUE)    // disable alarm if set hour is greater than 25
                        {
                            exceed_24hr = FALSE;
                            clearBit(ALRMCON, 7);   
                        }
                        else if(exceed_24hr == FALSE)
                        {
                            // if format is 24 hr
                            if(hour_format == FORMAT_24)
                            {
                                RTCC_setAlarm(decimalToBCD(setHoursClock), decimalToBCD(setMinutesClock));
                                subMode = RUN_MODE;
                                ALARM_DONE = FALSE;
                                // run
                            }
                            // else if format is 12 hr
                            else if(hour_format == FORMAT_12)
                            {
                                
                                while(isPressed(&PORTB, OK_PIN) == 0)
                                {
                                // ask if AM or PM
                                    if(isPressed(&PORTA, LEFT_MOVEMENT_PIN) || isPressed(&PORTB, RIGHT_MOVEMENT_PIN))
                                    {
                                        ++am_pm;
                                        if(am_pm == HOUR_FORMAT_ROLLBACK)
                                        {
                                            am_pm = AM_TIME;
                                        }
                                    }
                                    
                                    if(am_pm == AM_TIME)
                                    {
                                        TX_word("amm-");
                                    }
                                    else if(am_pm == PM_TIME)
                                    {
                                         TX_word("pmm-");
                                    }
                                // run mode
                                }
                                
                                if(am_pm == PM_TIME)
                                {
                                    setHoursClock = setHoursClock + 12;
                                }
                                RTCC_setAlarm(decimalToBCD(setHoursClock), decimalToBCD(setMinutesClock));
                                subMode = RUN_MODE;
                                ALARM_DONE = FALSE;
                            }  
                        }
                    }
                    else if(setClockFlag == SET_CLOCK)
                    {   
                        if(hour_format == FORMAT_12)
                            {
                                
                                while(isPressed(&PORTB, OK_PIN) == 0)
                                {
                                // ask if AM or PM
                                    if(isPressed(&PORTA, LEFT_MOVEMENT_PIN) || isPressed(&PORTB, RIGHT_MOVEMENT_PIN))
                                    {
                                        ++am_pm;
                                        if(am_pm == HOUR_FORMAT_ROLLBACK)
                                        {
                                            am_pm = AM_TIME;
                                        }
                                    }
                                    
                                    if(am_pm == AM_TIME)
                                    {
                                        TX_word("amm-");
                                    }
                                    else if(am_pm == PM_TIME)
                                    {
                                         TX_word("pmm-");
                                    }
                                // run mode
                                }
                                
                                if(am_pm == PM_TIME)
                                {
                                    setHoursClock = setHoursClock + 12;
                                } 
                            }
                        
                        RTCC_write(&HOURS, decimalToBCD(setHoursClock));
                        RTCC_write(&MINUTES, decimalToBCD(setMinutesClock));
                        RTCC_write(&SECONDS, 0x00);
                        subMode = RUN_MODE;
                    }
                }
                
                //get the digit place from the switch
                if(isPressed(&PORTA, LEFT_MOVEMENT_PIN)) {
                    ++digit_place;
                    if(digit_place == DIGIT_ROLLBACK) {
                        digit_place = 1;
                    }
                }
             
                if(isPressed(&PORTB, RIGHT_MOVEMENT_PIN)) {
                    --digit_place;
                    if(digit_place == 0 || digit_place > 4) {   // if passed the 1st digit or underflow, make 4th digit
                        digit_place = 4;
                    }
                }
                
                
                if(encoderCounter_ClockWise >= 2) {
                    encoderCounter_ClockWise = 0;
                    if(digit_place == 1) {
                        ++setMinutesClock;
                    }
                    else if(digit_place == 2) {
                        setMinutesClock =  setMinutesClock + 10;
                    }
                    else if(digit_place == 3) {
                        ++setHoursClock;
                    }
                    else if(digit_place == 4) {
                        setHoursClock =  setHoursClock + 10;
                    } 
                }
                
                else if(encoderCounter_counterClockWise >= 2) {
                    encoderCounter_counterClockWise = 0;
                    
                    if(digit_place == 1) {
                        --setMinutesClock;
                            if(setMinutesClock == 255) { // for event of underflow (0 - 1 = 255)
                                setMinutesClock = 0;
                            }   
                    }
                    else if(digit_place == 2) {
                        if(setMinutesClock > 10) {   // don't subtract when less than 10 because it will result in underflow
                            setMinutesClock =  setMinutesClock - 10;
                        }
                    }
                    else if(digit_place == 3) {
                        --setHoursClock;
                        if(setHoursClock == 255) { // for event of underflow (0 - 1 = 255)
                            setHoursClock = 0;
                        } 
                    }
                    else if(digit_place == 4) {
                        if(setHoursClock >= 10) {   // don't subtract when less than 10 because it will result in underflow
                            setHoursClock =  setHoursClock - 10;
                        }
                    } 
                }
                
                  
                if(setMinutesClock == 60) {
                    setMinutesClock = 0;
                    ++setHoursClock;
                }
                else if(setMinutesClock > 60) {      
                    setMinutesClock = extractDigitOnes(setMinutesClock);
                }
                
                if(hour_format == FORMAT_24)
                {
                    if(setHoursClock > 24){
                        setHoursClock = 24;
                        exceed_24hr = TRUE;
                    }
                }
                else if(hour_format == FORMAT_12)
                {
                    if(setHoursClock > 12)
                    {
                        setHoursClock = 12;
                    }
                }
            
                extractDigits(setMinutesClock, setHoursClock);
            }        
            
            else if(subMode == RUN_MODE) {
                if(subMode_previous != subMode) {
                    TMR0H = TIMER_REG_HIGH_BYTE;
                    TMR0L = TIMER_REG_LOW_BYTE;
                    setBit(T0CON0, 7);   // enable timer
                    clearBit(PIR0, 5);  // clear flag
                    setBit(PIE0, 5); // enable timer0 interrupt
                    stopResumeFlag = RESUME_MODE;
                    seconds = 0;
                    minutes = 0;
                    stopFlag = FALSE;
                
                }
                subMode_previous = subMode;
                
                if(updateClock == TRUE) {
                    minutes = BCDtoDecimal(RTCC_read(&MINUTES));
                    hours = BCDtoDecimal(RTCC_read(&HOURS));
                    if((hour_format == FORMAT_12) && (hours > 12))
                    {
                        hours = hours - 12;
                    }
                    updateClock = FALSE;
                }
                extractDigits(minutes, hours);
                
                if(stopResumeFlag == STOP_MODE) {
                    stopFlag = TRUE;
                    clearBit(PIR0, 5);  // clear flag
                }
                else if(stopResumeFlag == RESUME_MODE) {
                    stopFlag = FALSE;
                    clearBit(PIR0, 5);  // clear flag
                }
                
                
                if(alarmFlag_1 == TRUE) {
                    
                    ledState = TRUE;
                    alarmFlag_1 = FALSE;
                    while(stopResumeFlag == RESUME_MODE) {
                        if(ledState == TRUE)
                        {
                            PWM_on(BUZZER_PIN);
                        }
                        else
                        {
                            PWM_off(BUZZER_PIN);
                        }
                        
                        minutes = BCDtoDecimal(RTCC_read(&MINUTES));
                        hours = BCDtoDecimal(RTCC_read(&HOURS));
                        
                        extractDigits(minutes, hours);
                        
                        if(updateDisplay == TRUE)
                        {
                            updateDigits();
                            updateDisplay = FALSE;
                        }
                        
                        if(isPressed(&PORTA, STOP_RESUME_PIN)) {
                            ++stopResumeFlag;
                            if(stopResumeFlag == STOP_RESUME_ROLLBACK) {
                                stopResumeFlag = STOP_MODE;
                            }
                        }
                    }
                    PWM_off(BUZZER_PIN);
                    stopFlag = FALSE;
                    stopResumeFlag = RESUME_MODE;  
                }    
            }
            else {
                TX_char("----");
            }
        }
        
        else if(mode_current == COUNT_UP_MODE){
            
            if(mode_previous != mode_current) {
                countUp_init();
            }
            mode_previous = mode_current;
            
            if(subMode == SET_MODE || subMode == RESET_MODE) {
                if(subMode_previous != subMode) {
                    TMR0H = TIMER_REG_HIGH_BYTE;
                    TMR0L = TIMER_REG_LOW_BYTE;
                    clearBit(PIR0, 5);  // clear flag
                    setBit(T0CON0, 7);
                    setBit(PIE0, 5); // enable timer0 interrupt
                    
                    stopResumeFlag = RESUME_MODE;
                    
                    digit_place = 1;
                }
                subMode_previous = subMode;
                
               extractDigits(setSecondsCtUp, setMinutesCtUp);
               
               
               
               
               // if set mode, just display the set time
               // if reset mode, display 00.00
                if(subMode == RESET_MODE)
                {
                   setSecondsCtUp = 0;
                   setMinutesCtUp = 0;
                   subMode = SET_MODE;  // Rollback to set mode. If not, the display will always be 00.00
                }
                else if(subMode == SET_MODE)
                {
                   ;    // do nothing because the default is just display the set time
                }
                
               
               
                // check run/ok button
                if(isPressed(&PORTB, OK_PIN))
                {
                    subMode = RUN_MODE;
                }
                
                //get the digit place from the switch
                if(isPressed(&PORTA, LEFT_MOVEMENT_PIN)) {
                    ++digit_place;
                    if(digit_place == DIGIT_ROLLBACK) {
                        digit_place = 1;
                    }
                }
             
                if(isPressed(&PORTB, RIGHT_MOVEMENT_PIN)) {
                    --digit_place;
                    if(digit_place == 0 || digit_place > 4) {   // if passed the 1st digit or underflow, make 4th digit
                        digit_place = 4;
                    }
                }
                
                
                if(encoderCounter_ClockWise >= 2) {
                    encoderCounter_ClockWise = 0;
                    if(digit_place == 1) {
                        ++setSecondsCtUp;
                    }
                    else if(digit_place == 2) {
                        setSecondsCtUp =  setSecondsCtUp + 10;
                    }
                    else if(digit_place == 3) {
                        ++setMinutesCtUp;
                    }
                    else if(digit_place == 4) {
                        setMinutesCtUp =  setMinutesCtUp + 10;
                    } 
                }
                
                else if(encoderCounter_counterClockWise >= 2) {
                    encoderCounter_counterClockWise = 0;
                    
                    if(digit_place == 1) {
                        --setSecondsCtUp;
                            if(setSecondsCtUp == 255) { // for event of underflow (0 - 1 = 255)
                                setSecondsCtUp = 0;
                            }   
                    }
                    else if(digit_place == 2) {
                        if(setSecondsCtUp > 10) {   // don't subtract when less than 10 because it will result in underflow
                            setSecondsCtUp =  setSecondsCtUp - 10;
                        }
                    }
                    else if(digit_place == 3) {
                        --setMinutesCtUp;
                        if(setMinutesCtUp == 255) { // for event of underflow (0 - 1 = 255)
                            setMinutesCtUp = 0;
                        } 
                    }
                    else if(digit_place == 4) {
                        if(setMinutesCtUp >= 10) {   // don't subtract when less than 10 because it will result in underflow
                            setMinutesCtUp =  setMinutesCtUp - 10;
                        }
                    } 
                }
                
                  
                if(setSecondsCtUp == 60) {
                    setSecondsCtUp = 0;
                    ++setMinutesCtUp;
                }
                else if(setMinutesCtUp > 99){
                    setMinutesCtUp = 99;
                }
                else if(setSecondsCtUp > 60) {      
                    setSecondsCtUp = extractDigitOnes(setSecondsCtUp);
                }
               
               
            }
            
            else if(subMode == RUN_MODE) {
                if(subMode_previous != subMode) {
                      
                    TMR0H = TIMER_REG_HIGH_BYTE;
                    TMR0L = TIMER_REG_LOW_BYTE;
                    
                    setBit(T0CON0, 7);   // enable timer
                    clearBit(PIR0, 5);  // clear flag
                    setBit(PIE0, 5); // enable timer0 interrupt
                    stopResumeFlag = RESUME_MODE;
                    seconds = 0;
                    minutes = 0;
                    stopFlag = FALSE; 
                }
                subMode_previous = subMode;
                
                secondsCopy = seconds;
                minutesCopy = minutes;
                extractDigits(secondsCopy, minutesCopy);
                
                if(stopResumeFlag == STOP_MODE) {
                    stopFlag = TRUE;
                  
                    clearBit(PIR0, 5);  // clear flag
                }
                else if(stopResumeFlag == RESUME_MODE) {
                    stopFlag = FALSE;
                    
                    clearBit(PIR0, 5);  // clear flag
                }
                
                if((secondsCopy == setSecondsCtUp) && (minutesCopy == setMinutesCtUp)) {
                    stopFlag = TRUE;
                   
                    while(stopResumeFlag == RESUME_MODE) {
                        
                        
                        if(ledState == TRUE)
                        {
                            PWM_on(BUZZER_PIN);
                        }
                        else
                        {
                            PWM_off(BUZZER_PIN);
                        }
                        
                        LATBbits.LATB1 = ledState;
                        
                        updateDigits();     // When stop happened first, it will not display the last number.
                        if(isPressed(&PORTA, STOP_RESUME_PIN)) {
                            ++stopResumeFlag;
                            if(stopResumeFlag == STOP_RESUME_ROLLBACK) {
                                stopResumeFlag = STOP_MODE;
                            }
                        }
                    }
                    PWM_off(BUZZER_PIN);
                    clearBit(PIR0, 5);  // clear flag
                    clearBit(PIE0, 5); // disable timer0 interrupt
                    stopFlag = FALSE;
                    subMode = SET_MODE;
                    stopResumeFlag = RESUME_MODE;
                }
            }  
        }
        
        
        else if(mode_current == COUNT_DOWN_MODE) {
            if(mode_previous != mode_current) {
                countDown_init();
            }
            mode_previous = mode_current;
            
            if(subMode == SET_MODE || subMode == RESET_MODE) {
                if(subMode_previous != subMode) { 
                    TMR0H = TIMER_REG_HIGH_BYTE;
                    TMR0L = TIMER_REG_LOW_BYTE;
                    
                    clearBit(PIR0, 5);  // clear flag
                    setBit(PIE0, 5); // enable timer0 interrupt
                    
                    stopResumeFlag = RESUME_MODE;
                    seconds = 0;
                    minutes = 0;
                    stopFlag = FALSE;
                }
                subMode_previous = subMode;
                
                extractDigits(setSecondsCtDown, setMinutesCtDown);
                
                // if set mode, just display the set time
               // if reset mode, display 00.00
                if(subMode == RESET_MODE)
                {
                   setSecondsCtDown = 0;
                   setMinutesCtDown = 0;
                   subMode = SET_MODE;  // Rollback to set mode. If not, the display will always be 00.00
                }
                else if(subMode == SET_MODE)
                {
                   ;    // do nothing because the default is just display the set time
                }
                
                // check run/ok button
                if(isPressed(&PORTB, OK_PIN))
                {
                    subMode = RUN_MODE;
                }
                
                //get the digit place from the switch
                if(isPressed(&PORTA, LEFT_MOVEMENT_PIN)) {
                    ++digit_place;
                    if(digit_place == DIGIT_ROLLBACK) {
                        digit_place = 1;
                    }
                }
             
                if(isPressed(&PORTB, RIGHT_MOVEMENT_PIN)) {
                    --digit_place;
                    if(digit_place == 0 || digit_place > 4) {   // if passed the 1st digit or underflow, make 4th digit
                        digit_place = 4;
                    }
                }
                
                
                if(encoderCounter_ClockWise >= 2) {
                    encoderCounter_ClockWise = 0;
                    if(digit_place == 1) {
                        ++setSecondsCtDown;
                    }
                    else if(digit_place == 2) {
                        setSecondsCtDown =  setSecondsCtDown + 10;
                    }
                    else if(digit_place == 3) {
                        ++setMinutesCtDown;
                    }
                    else if(digit_place == 4) {
                        setMinutesCtDown =  setMinutesCtDown + 10;
                    } 
                }
                
                else if(encoderCounter_counterClockWise >= 2) {
                    encoderCounter_counterClockWise = 0;
                    
                    if(digit_place == 1) {
                        --setSecondsCtDown;
                            if(setSecondsCtDown == 255) { // for event of underflow (0 - 1 = 255)
                                setSecondsCtDown = 0;
                            }   
                    }
                    else if(digit_place == 2) {
                        if(setSecondsCtDown > 10) {   // don't subtract when less than 10 because it will result in underflow
                            setSecondsCtDown =  setSecondsCtDown - 10;
                        }
                    }
                    else if(digit_place == 3) {
                        --setMinutesCtDown;
                        if(setMinutesCtDown == 255) { // for event of underflow (0 - 1 = 255)
                            setMinutesCtDown = 0;
                        } 
                    }
                    else if(digit_place == 4) {
                        if(setMinutesCtDown >= 10) {   // don't subtract when less than 10 because it will result in underflow
                            setMinutesCtDown =  setMinutesCtDown - 10;
                        }
                    } 
                }
                
                if(setSecondsCtDown == 60) {
                    setSecondsCtDown = 0;
                    ++setMinutesCtDown;
                }
                else if(setMinutesCtDown > 99){
                    setMinutesCtDown = 99;
                }
                else if(setSecondsCtDown > 60) {      
                    setSecondsCtDown = extractDigitOnes(setSecondsCtDown);
                }
            }
            else if(subMode == RUN_MODE) {
                if(subMode_previous != subMode) {
                    
                    TMR0H = TIMER_REG_HIGH_BYTE;
                    TMR0L = TIMER_REG_LOW_BYTE;
                    
                    clearBit(PIR0, 5);  // clear flag
                    setBit(PIE0, 5); // enable timer0 interrupt
                    
                    stopResumeFlag = RESUME_MODE;
                    seconds = setSecondsCtDown;
                    minutes = setMinutesCtDown;
                }
                subMode_previous = subMode;
               
                secondsCopy = seconds;
                minutesCopy = minutes;
                extractDigits(secondsCopy, minutesCopy);
                
                
                if(stopResumeFlag == STOP_MODE) {
                    stopFlag = TRUE;
                    
                    clearBit(PIR0, 5);  // clear flag
                }
                else if(stopResumeFlag == RESUME_MODE) {
                    stopFlag = FALSE;
                    
                    clearBit(PIR0, 5);  // clear flag
                }
                
                
                
                if((secondsCopy == 0) && (minutesCopy == 0)) {
                    while(stopResumeFlag == RESUME_MODE) {
                        
                        if(ledState == TRUE)
                        {
                            PWM_on(BUZZER_PIN);
                        }
                        else
                        {
                            PWM_off(BUZZER_PIN);
                        }
                        
                        updateDigits();
                        if(isPressed(&PORTA, STOP_RESUME_PIN)) {
                            ++stopResumeFlag;
                            if(stopResumeFlag == STOP_RESUME_ROLLBACK) {
                                stopResumeFlag = STOP_MODE;
                            }
                        }
                    }
                    clearBit(PIR0, 5);  // clear flag
                    clearBit(PIE0, 5);  // 
                   
                    PWM_off(BUZZER_PIN); 
                    stopFlag = FALSE;
                    subMode = SET_MODE;
                    stopResumeFlag = RESUME_MODE;    
                }
            }
        }
        else if(mode_current == ADJUST_MODE)
        {
            mode_previous = ADJUST_MODE;
           
            
            if(ledState == TRUE)
            {
                PWM_on(BUZZER_PIN);
            }
            else
            {
               PWM_off(BUZZER_PIN);
            }
            
            if(isPressed(&PORTB, OK_PIN))
            {
                duty_cycle = duty_cycle_temp;
                PWM_off(BUZZER_PIN);
                mode_current = CLOCK_MODE;
                PWM_dutyCycle(BUZZER_PIN, duty_cycle);
            }
            
            if(encoderCounter_counterClockWise >= 1)
            {
                encoderCounter_counterClockWise = 0;
                duty_cycle_temp += 2;
                if(duty_cycle_temp > 123)
                {
                    duty_cycle_temp = 123;
                }
            }
            else if(encoderCounter_ClockWise >= 1)
            {
                encoderCounter_ClockWise = 0;
                if(duty_cycle_temp > 63)
                {
                    duty_cycle_temp -= 2;
                }
            }
            
        PWM_dutyCycle(BUZZER_PIN, duty_cycle_temp);    
            
        }
    }
    
    return 0;
}