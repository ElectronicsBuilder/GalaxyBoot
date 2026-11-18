/*
 * boot_main.c - GalaxyBoot: decide between starting BeaconGalaxy and updating it.
 *
 * Called from main() after the CubeMX init. This first version only starts
 * the application when its vector table is valid, else it blinks the red LED.
 * The SD-card update (fuse in EEPROM / ENTER held at reset) is added in M7.
 */
#include "boot_main.h"
#include "boot_jump.h"
#include "main.h"

void Boot_Main(void)
{
  if (Boot_AppValid()) Boot_JumpToApp();

  for (;;)                                     /* no application: blink red */
  {
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
    HAL_Delay(250);
  }
}
