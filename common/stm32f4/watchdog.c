/*
 * watchdog.c - see watchdog.h.
 */
#include "watchdog.h"
#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

#define LSI_HZ        32000u
#define NOTE_ADDR     (CCMDATARAM_END + 1u - sizeof(WdgNote))   /* last bytes of the CCM RAM */
#define NOTE_MAGIC    0x57444F47u                                /* "WDOG" */

typedef struct {
  uint32_t magic;
  char task[12];
} WdgNote;

static const char *names[WDG_MAX_TASKS];
static volatile uint32_t last_seen[WDG_MAX_TASKS];
static volatile uint8_t stalled[WDG_MAX_TASKS];
static int count;
static const char *stuck;
static char cause[40];
static int was_watchdog;

static uint32_t P_Now(void)
{
  extern uint32_t HAL_GetTick(void);
  return HAL_GetTick();
}

void Wdg_Start(uint32_t timeout_ms)
{
  if (timeout_ms < 1u) timeout_ms = 1u;
  if (timeout_ms > 32000u) timeout_ms = 32000u;
  /* prescaler 4 << pr; pick the smallest one where the 12-bit reload fits */
  uint32_t pr = 0;
  uint32_t reload;
  for (;;)
  {
    uint32_t div = 4u << pr;
    reload = (uint32_t)(((uint64_t)timeout_ms * LSI_HZ) / (1000u * div));
    if (reload <= 4096u || pr == 6u) break;
    pr++;
  }
  if (reload > 4096u) reload = 4096u;
  if (reload < 1u) reload = 1u;

  DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;   /* halted in the debugger: no reset */
  IWDG->KR = 0xCCCCu;                                /* start (no effect if running) */
  IWDG->KR = 0x5555u;                                /* unlock PR / RLR */
  while (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) { }
  IWDG->PR = pr;
  IWDG->RLR = reload - 1u;
  while (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) { }
  IWDG->KR = 0xAAAAu;
}

void Wdg_Kick(void)
{
  IWDG->KR = 0xAAAAu;
}

int Wdg_Register(const char *name)
{
  if (count >= WDG_MAX_TASKS) return -1;
  names[count] = name;
  last_seen[count] = P_Now();
  return count++;
}

void Wdg_Alive(int id)
{
  if (id >= 0 && id < count && !stalled[id]) last_seen[id] = P_Now();
}

int Wdg_Count(void) { return count; }
const char *Wdg_Name(int id) { return (id >= 0 && id < count) ? names[id] : ""; }
uint32_t Wdg_Age(int id) { return (id >= 0 && id < count) ? P_Now() - last_seen[id] : 0u; }

int Wdg_Stall(const char *name)
{
  for (int i = 0; i < count; i++)
  {
    if (strcmp(names[i], name) == 0)
    {
      stalled[i] = 1;
      return 0;
    }
  }
  return -1;
}

const char *Wdg_Stuck(void)
{
  return stuck;
}

void Wdg_Poll(void)
{
  uint32_t now = P_Now();
  for (int i = 0; i < count; i++)
  {
    if ((int32_t)(now - last_seen[i]) > (int32_t)WDG_WINDOW_MS)   /* signed: a task may report after "now" was read */
    {
      if (stuck == NULL)                     /* note it once: the reset follows */
      {
        stuck = names[i];
        WdgNote *n = (WdgNote *)NOTE_ADDR;
        n->magic = NOTE_MAGIC;
        strncpy(n->task, names[i], sizeof n->task - 1u);
        n->task[sizeof n->task - 1u] = '\0';
      }
      return;                                /* no refresh: the IWDG resets the board */
    }
  }
  Wdg_Kick();
}

const char *Wdg_ResetCause(void)
{
  if (cause[0]) return cause;
  uint32_t csr = RCC->CSR;
  WdgNote *n = (WdgNote *)NOTE_ADDR;
  if (csr & RCC_CSR_IWDGRSTF)
  {
    was_watchdog = 1;
    if (n->magic == NOTE_MAGIC)
    {
      n->task[sizeof n->task - 1u] = '\0';
      snprintf(cause, sizeof cause, "watchdog (task %s stuck)", n->task);
    }
    else
    {
      snprintf(cause, sizeof cause, "watchdog (App task or interrupts stuck)");
    }
  }
  else if (csr & RCC_CSR_WWDGRSTF) snprintf(cause, sizeof cause, "window watchdog");
  else if (csr & RCC_CSR_LPWRRSTF) snprintf(cause, sizeof cause, "low-power");
  else if (csr & RCC_CSR_BORRSTF)  snprintf(cause, sizeof cause, "power-on / brown-out");
  else if (csr & RCC_CSR_PORRSTF)  snprintf(cause, sizeof cause, "power-on");
  else if (csr & RCC_CSR_SFTRSTF)  snprintf(cause, sizeof cause, "software");
  else if (csr & RCC_CSR_PINRSTF)  snprintf(cause, sizeof cause, "reset pin");
  else snprintf(cause, sizeof cause, "unknown");
  n->magic = 0;
  RCC->CSR |= RCC_CSR_RMVF;                  /* the next reset starts with clean flags */
  return cause;
}

int Wdg_LastResetWasWatchdog(void)
{
  (void)Wdg_ResetCause();
  return was_watchdog;
}
