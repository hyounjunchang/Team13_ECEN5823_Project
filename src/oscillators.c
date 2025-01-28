/***********************************************************************
 * @file      oscillators.c
 * @brief     Configuration for oscillator/clock tree for Blue Gecko Board
 *
 * @author    Hyounjun Chang, hyounjun.chang@colorado.edu
 * @date      Jan 26, 2025
 *
 * @resources
 *
 *
 * Editor: Jan 26, 2025 Hyounjun Chang
 * Change: Initial oscillator/clock tree setup
 *
 */

#include <em_cmu.h>
#include "app.h"
#include "oscillators.h"

// Enable XO for timer
void initialize_oscillators(){
  // Enable oscillators for timer
  if (LOWEST_ENERGY_MODE == 0 || LOWEST_ENERGY_MODE == 1 || LOWEST_ENERGY_MODE == 2){
      /* Initialize LFXO with specific parameters */
      CMU_LFXOInit_TypeDef lfxoInit = CMU_LFXOINIT_DEFAULT;
      CMU_LFXOInit(&lfxoInit);

      CMU_OscillatorEnable(cmuOsc_LFXO, true, true); // enable LFXO, wait until ready
      CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO); // enable LFA clock, source=LFXO, LETIMER runs from LFA clock
  }
  else if (LOWEST_ENERGY_MODE == 3){
      CMU_OscillatorEnable(cmuOsc_ULFRCO, true, true); // enable ULFCRO, wait until ready
      CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_ULFRCO);  // enable LFA clock, sourcｅ=ULFRCO, LETIMER runs from LFA clock
  }
}
