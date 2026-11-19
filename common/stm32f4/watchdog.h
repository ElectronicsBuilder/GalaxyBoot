/*
 * watchdog.h - independent watchdog (IWDG, own 32 kHz LSI clock) with task
 * supervision, for STM32F4.
 *
 * The watchdog is refreshed by Wdg_Poll() only while every registered task has
 * called Wdg_Alive() within WDG_WINDOW_MS, so one stuck task resets the board
 * even when the others keep running. Before that happens the stuck task's name
 * is written to a few bytes at the end of the CCM RAM that the start-up code
 * does not clear; the next boot reports it (Wdg_ResetCause()).
 *
 * The IWDG stops while a debugger halts the core. Only a reset stops it
 * otherwise; a software reset (NVIC_SystemReset) does.
 */
/* Times are HAL_GetTick() milliseconds. */
#ifndef COMMON_WATCHDOG_H
#define COMMON_WATCHDOG_H

#include <stdint.h>

#define WDG_MAX_TASKS  6
#define WDG_WINDOW_MS  3000u     /* a task must report at least this often (longest legitimate wait: a 2 s SD read timeout) */

/* Starts the IWDG (or changes its timeout if it already runs), 1..32000 ms */
void Wdg_Start(uint32_t timeout_ms);
/* Refreshes the IWDG unconditionally (single-threaded code: bootloader) */
void Wdg_Kick(void);

/* Task supervision (RTOS firmware) */
int  Wdg_Register(const char *name);   /* returns an id; call once per task */
void Wdg_Alive(int id);                /* the task is making progress (any task, any time) */
void Wdg_Poll(void);                   /* one task (App) calls this often: refreshes if all are alive */
const char *Wdg_Stuck(void);           /* name of a task that stopped reporting, or NULL */

/* Console / tests */
int  Wdg_Count(void);
const char *Wdg_Name(int id);
uint32_t Wdg_Age(int id);              /* ms since the task last reported */
int  Wdg_Stall(const char *name);      /* ignore that task's reports from now on (test); -1 if unknown */

/* Why the last reset happened, e.g. "power-on", "reset pin", "software",
 * "watchdog (task fx stuck)". Call once at boot; it also clears the flags. */
const char *Wdg_ResetCause(void);
int  Wdg_LastResetWasWatchdog(void);

#endif
