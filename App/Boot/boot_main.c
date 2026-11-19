/*
 * boot_main.c - GalaxyBoot: start BeaconGalaxy, or update it from the SD card.
 *
 * An update runs when the EEPROM fuse is set (BeaconGalaxy's About > UPGRADE)
 * or when ENTER is held during reset. Otherwise the application starts at once
 * (the OLED is not even switched on). Without a valid application GalaxyBoot
 * stays here, shows NO APP and tries the SD card whenever ENTER is pressed.
 *
 * The fuse is cleared after a successful update and after a failure that left
 * the application untouched (no card, no file, bad file), so a bad card never
 * causes a reset loop. After a failed flash write it stays set: the next reset
 * tries again.
 */
#include "boot_main.h"
#include "boot_jump.h"
#include "boot_ui.h"
#include "boot_update.h"
#include "boot_fuse.h"
#include "eeprom.h"
#include "oled.h"
#include "main.h"
#include <string.h>

#define BOOT_VERSION "1.0"

static const uint8_t fuse_set[BOOT_FUSE_LEN] = BOOT_FUSE_SET;

static int P_EnterHeld(void)
{
  int held = 0;
  for (int i = 0; i < 20; i++)                 /* 20 ms of steady press (buttons are active high) */
  {
    held += HAL_GPIO_ReadPin(BTN_ENTER_GPIO_Port, BTN_ENTER_Pin) == GPIO_PIN_SET;
    HAL_Delay(1);
  }
  return held == 20;
}

static int P_FuseSet(void)
{
  uint8_t f[BOOT_FUSE_LEN];
  return Ee_Read(BOOT_FUSE_ADDR, f, sizeof f) == 0 && memcmp(f, fuse_set, sizeof f) == 0;
}

static void P_FuseClear(void)
{
  static const uint8_t clear[BOOT_FUSE_LEN] = BOOT_FUSE_CLEAR;
  if (Ee_Write(BOOT_FUSE_ADDR, clear, sizeof clear) != 0) Boot_Log("boot: could not clear the fuse");
}

static void P_Header(void)
{
  BootUi_Clear();
  BootUi_Line(0, "GALAXYBOOT");
  BootUi_Line(1, "V" BOOT_VERSION);
  BootUi_Show();
}

static UpdateResult P_Update(int fuse)
{
  P_Header();
  UpdateResult r = Update_Run();
  Boot_Log("boot: update: %s", Update_ResultText(r));
  BootUi_Line(2, Update_ResultText(r));
  BootUi_Line(3, "");
  BootUi_Line(4, "");
  BootUi_Show();
  if (fuse && r != UPD_FLASH_FAILED) P_FuseClear();
  HAL_Delay(r == UPD_OK ? 1000u : 3000u);     /* time to read the result */
  return r;
}

void Boot_Main(void)
{
  Boot_Log("%s", "");
  Boot_Log("GalaxyBoot " BOOT_VERSION);
  int fuse = (Ee_Init() == 0) && P_FuseSet();
  int enter = P_EnterHeld();
  int ui = 0;

  if (fuse || enter)
  {
    Boot_Log("boot: update requested (%s)", fuse ? "fuse" : "ENTER held");
    BootUi_Init();
    ui = 1;
    if (P_Update(fuse) == UPD_OK) NVIC_SystemReset();       /* start the new application cleanly */
  }

  if (Boot_AppValid())
  {
    Boot_Log("boot: starting the application");
    if (ui) Oled_Power(0);
    Boot_JumpToApp();
  }

  /* no application: wait for an SD card with an update */
  Boot_Log("boot: no valid application; press ENTER to update from the SD card");
  if (!ui) BootUi_Init();
  for (;;)
  {
    P_Header();
    BootUi_Line(3, "NO APP");
    BootUi_Line(4, "ENTER:");
    BootUi_Line(5, "UPDATE");
    BootUi_Show();
    while (!P_EnterHeld())
    {
      HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
      HAL_Delay(230);
    }
    HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
    if (P_Update(0) == UPD_OK) NVIC_SystemReset();
  }
}
