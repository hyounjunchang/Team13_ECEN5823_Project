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
#include "timer.h"


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

// For SI7021 Temperature Sensor
#define SI7021_I2C_PORT gpioPortC
#define SI7021_SCL_PIN 10 // I2C0_SCL_14
#define SI7021_SDA_PIN 11 // I2C0_SDA_16

#define SI7021_ENABLE_PORT gpioPortD
#define SI7021_ENABLE_PIN 15


// called during boot_up in app.c
void gpioInit()
{
  gpioInit_SI7021();
  initialize_I2C();
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

// Initializes SI7021, must be called on initialization to power on the port
void gpioInit_SI7021(){
  // Max current 4mA for I2C/Power-Up for SI7021
  // Per I2C spec, 3mA for 100kHz/400kHz
  GPIO_DriveStrengthSet(SI7021_I2C_PORT, gpioDriveStrengthStrongAlternateStrong); // Strong, 10mA
  GPIO_DriveStrengthSet(SI7021_ENABLE_PORT, gpioDriveStrengthStrongAlternateStrong); // Strong, 10mA

  // Set GPIOs mode of operation
  GPIO_PinModeSet(SI7021_ENABLE_PORT, SI7021_ENABLE_PIN, gpioModePushPull, 0); // push-pull, initialize with 0 (off)
}

// Referenced from Lecture 6
void gpioPowerOn_SI7021(){
  // Basic setup steps for GPIO control include the following:
  // 1- Setup the GPIO for Push-pull configuration and enable the GPIO pin to enable power to the device, i.e. set to 1 (SENSOR_ENABLE)
  GPIO_PinModeSet(SI7021_ENABLE_PORT, SI7021_ENABLE_PIN, gpioModePushPull, 1); // push-pull
  GPIO_PinOutSet(SI7021_ENABLE_PORT, SI7021_ENABLE_PIN);

  // 2- Wait for external device to complete its Power On Reset (POR) sequence
  // 80ms max power-up time
  timerWaitUs_irq(80000);

  // 3- Setup/enable GPIOs used for communication (I2C GPIOs: SCLK,SDA) with the device
  // this does not depend on step 2, so it's fine to call
  GPIO_PinModeSet(SI7021_I2C_PORT, SI7021_SCL_PIN, gpioModeWiredAndPullUp, 1); // open drain with pull-up, default to 1
  GPIO_PinModeSet(SI7021_I2C_PORT, SI7021_SDA_PIN, gpioModeWiredAndPullUp, 1);

  // 4- Initialize the device, if required, including peripheral interrupt requests
  // 5- If using interrupts, enable NVIC interrupts for the device
}

// Referenced from Lecture 6
void gpioPowerOff_SI7021(){
  // 5- If using interrupts, disable NVIC interrupts for the device

  // 3- Disable GPIOs used for communication (I2C GPIOs: SCLK,SDA) with the device
  GPIO_PinModeSet(SI7021_I2C_PORT, SI7021_SCL_PIN, gpioModeDisabled, 1);
  GPIO_PinModeSet(SI7021_I2C_PORT, SI7021_SDA_PIN, gpioModeDisabled, 1);

  // 1- Set GPIO pin to power-off the device, i.e. set to 0 (SENSOR_ENABLE), don’t disable the GPIO!
  GPIO_PinOutClear(SI7021_ENABLE_PORT, SI7021_ENABLE_PIN);
}

void gpioLed0SetOn()
{
	GPIO_PinOutSet(LED_port, LED0_pin);
}


void gpioLed0SetOff()
{
	GPIO_PinOutClear(LED_port, LED0_pin);
}

void gpioLed0Toggle(){
  GPIO_PinOutToggle(LED_port, LED0_pin);
}

void gpioLed1SetOn()
{
	GPIO_PinOutSet(LED_port, LED1_pin);
}


void gpioLed1SetOff()
{
	GPIO_PinOutClear(LED_port, LED1_pin);
}




