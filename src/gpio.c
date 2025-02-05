/*
  gpio.c
 
   Created on: Dec 12, 2018
       Author: Dan Walkes
   Updated by Dave Sluiter Dec 31, 2020. Minor edits with #defines.

   March 17
   Dave Sluiter: Use this file to define functions that set up or control GPIOs.
   
   Jan 24, 2023
   Dave Sluiter: Cleaned up gpioInit() to make it less confusing for students regarding
                 drive strength setting. 

 * @resources
 *
 *
 * Student edit: Add your name and email address here:
 * @student    Hyounjun Chang, hyounjun.chang@colorado.edu
 *
   Editor: Jan 20, 2025 Hyounjun Chang
 * Change: updated GPIO assignment for LED0/LED1
 */


// *****************************************************************************
// Students:
// We will be creating additional functions that configure and manipulate GPIOs.
// For any new GPIO function you create, place that function in this file.
// *****************************************************************************

#include <stdbool.h>
#include "em_gpio.h"
#include <string.h>

#include "gpio.h"


// Student Edit: Define these, 0's are placeholder values.
//
// See the radio board user guide at https://www.silabs.com/documents/login/user-guides/ug279-brd4104a-user-guide.pdf
// and GPIO documentation at https://siliconlabs.github.io/Gecko_SDK_Doc/efm32g/html/group__GPIO.html
// to determine the correct values for these.
// If these links have gone bad, consult the reference manual and/or the datasheet for the MCU.
// Change to correct port and pins:

// MCU for Blue Gekko EFR32BG13P632F512GM48-D: QFN48 chip, 2.4GHz
#define LED_port   gpioPortF
#define LED0_pin   4 // PF4
#define LED1_pin   5 // PF5

// For Si7021 Temperature Sensor
#define Si7021_port gpioPortC
#define Si7021_SCL_pin 10 // I2C0_SCL_14
#define Si7021_SDA_pin 11 // I2C0_SDA_16
#define Si7021_ENABLE_pin 15


// Set GPIO drive strengths and modes of operation
void gpioInit()
{
	// Initialize
  gpioInit_LED();
  gpioInit_Si7021();


} // gpioInit()

void gpioInit_LED(){
  // Set the port's drive strength. In this MCU implementation, all GPIO cells
  // in a "Port" share the same drive strength setting.
  //GPIO_DriveStrengthSet(LED_port, gpioDriveStrengthStrongAlternateStrong); // Strong, 10mA
  GPIO_DriveStrengthSet(LED_port, gpioDriveStrengthWeakAlternateWeak); // Weak, 1mA

  // Set the 2 GPIOs mode of operation
  GPIO_PinModeSet(LED_port, LED0_pin, gpioModePushPull, false);
  GPIO_PinModeSet(LED_port, LED1_pin, gpioModePushPull, false);

  // clear LED0 and LED1 for initialization
  GPIO_PinOutClear(LED_port, LED0_pin);
  GPIO_PinOutClear(LED_port, LED1_pin);

}

void gpioInit_Si7021(){
  GPIO_DriveStrengthSet(LED_port, gpioDriveStrengthWeakAlternateWeak); // Weak, 1mA
  // Set the 2 GPIOs mode of operation
  GPIO_PinModeSet(Si7021_port, Si7021_SCL_pin, gpioModeWiredAndPullUp, 1); // open drain with pull-up, default to 1
  GPIO_PinModeSet(Si7021_port, Si7021_SDA_pin, gpioModeWiredAndPullUp, 1);
  GPIO_PinModeSet(Si7021_port, Si7021_ENABLE_pin, gpioModePushPull, false); // push-pull, initialize with 0
}


void gpioLed0SetOn()
{
	GPIO_PinOutSet(LED_port, LED0_pin);
}


void gpioLed0SetOff()
{
	GPIO_PinOutClear(LED_port, LED0_pin);
}


void gpioLed1SetOn()
{
	GPIO_PinOutSet(LED_port, LED1_pin);
}


void gpioLed1SetOff()
{
	GPIO_PinOutClear(LED_port, LED1_pin);
}






