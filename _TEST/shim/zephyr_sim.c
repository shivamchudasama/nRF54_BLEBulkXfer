/**
 * @file          zephyr_sim.c
 * @brief         Out-of-line part of the host-test Zephyr shim, linked into
 *                every test that uses zephyr_shim.h:
 *
 *                - simulated time and the verbose switch,
 *                - log capture: every LOG_* / APP_LOG_* line is kept so a test
 *                  can assert on what the firmware would print,
 *                - __ASSERT trapping for SIM_EXPECT_ASSERT(),
 *                - gv_SimRunThread(): runs a K_THREAD_DEFINE body until it
 *                  would block for ever, so one loop iteration of an endless
 *                  thread can be tested,
 *                - the State Machine Framework (zephyr/smf.h), flat machines.
 *
 * @date          29/09/2026
 * @author        Shivam Chudasama
 * @copyright     Shivam Chudasama
 * @license       MIT
 */

/* SPDX-License-Identifier: MIT */

#include <stdarg.h>
#include "zephyr_shim.h"
#include "zephyr/smf.h"

/******************************************************************************/
/*  Time and verbosity                                                        */
/******************************************************************************/
int64_t gi64_simNowMs = 0;
bool gb_simVerbose = false;
uint32_t gu32_simLogBuffered = 0U;

/******************************************************************************/
/*  Log capture                                                               */
/******************************************************************************/
#define SIM_LOG_LINES        (8192U)  /* a 64 KiB hex dump is 4096 lines     */
#define SIM_LOG_LINE_LEN     (160U)

static char scar_logLines[SIM_LOG_LINES][SIM_LOG_LINE_LEN];
static uint32_t su32_logCount = 0U;          /* lines kept (oldest dropped)  */

void gv_SimLog(const char *cpt_level, const char *cpt_fmt, ...)
{
   char *cpt_line = scar_logLines[su32_logCount % SIM_LOG_LINES];
   va_list t_args;

   va_start(t_args, cpt_fmt);
   (void)vsnprintf(cpt_line, SIM_LOG_LINE_LEN, cpt_fmt, t_args);
   va_end(t_args);
   su32_logCount++;

   if (gb_simVerbose)
   {
      printf("[%7d] %s %s\n", (int)gi64_simNowMs, cpt_level, cpt_line);
   }
}

void gv_SimLogClear(void)
{
   su32_logCount = 0U;
}

uint32_t gu32_SimLogCount(void)
{
   return MIN(su32_logCount, SIM_LOG_LINES);
}

/** Line u32_idx of the captured log, oldest first; NULL if out of range. */
const char *gcpt_SimLogLine(uint32_t u32_idx)
{
   uint32_t u32_first = (su32_logCount > SIM_LOG_LINES) ? (su32_logCount - SIM_LOG_LINES) : 0U;

   if (u32_idx >= gu32_SimLogCount())
   {
      return NULL;
   }
   return scar_logLines[(u32_first + u32_idx) % SIM_LOG_LINES];
}

/** First captured line containing cpt_text, or NULL. */
const char *gcpt_SimLogFind(const char *cpt_text)
{
   uint32_t i;

   for (i = 0U; i < gu32_SimLogCount(); i++)
   {
      if (strstr(gcpt_SimLogLine(i), cpt_text) != NULL)
      {
         return gcpt_SimLogLine(i);
      }
   }
   return NULL;
}

/******************************************************************************/
/*  __ASSERT                                                                  */
/******************************************************************************/
jmp_buf *gpt_simAssertJmp = NULL;

void gv_SimAssertFail(const char *cpt_msg)
{
   if (gpt_simAssertJmp != NULL)
   {
      longjmp(*gpt_simAssertJmp, 1);
   }
   printf("ASSERT: %s\n", cpt_msg);
   abort();
}

/******************************************************************************/
/*  Threads                                                                   */
/******************************************************************************/
jmp_buf *gpt_simThreadJmp = NULL;

void gv_SimRunThread(void (*fpt_entry)(void *, void *, void *))
{
   jmp_buf st_jb;
   jmp_buf *pt_outer = gpt_simThreadJmp;     /* a thread run from a block hook */

   gpt_simThreadJmp = &st_jb;
   if (setjmp(st_jb) == 0)
   {
      fpt_entry(NULL, NULL, NULL);
   }
   gpt_simThreadJmp = pt_outer;
}

/******************************************************************************/
/*  State Machine Framework (zephyr/smf.h): flat state machines               */
/******************************************************************************/
void smf_set_initial(struct smf_ctx *ctx, const struct smf_state *init_state)
{
   ctx->previous = NULL;
   ctx->current = init_state;
   ctx->terminate_val = 0;
   if (init_state->entry != NULL)
   {
      init_state->entry(ctx);
   }
}

/* As Zephyr's for a flat machine: the current state's exit, then the new
   state's entry, at once. An entry action may set the next state itself. */
void smf_set_state(struct smf_ctx *ctx, const struct smf_state *new_state)
{
   if (ctx->current->exit != NULL)
   {
      ctx->current->exit(ctx);
   }
   ctx->previous = ctx->current;
   ctx->current = new_state;
   if (new_state->entry != NULL)
   {
      new_state->entry(ctx);
   }
}

void smf_set_terminate(struct smf_ctx *ctx, int32_t val)
{
   ctx->terminate_val = val;
}

int32_t smf_run_state(struct smf_ctx *ctx)
{
   if (ctx->terminate_val != 0)
   {
      return ctx->terminate_val;
   }
   if (ctx->current->run != NULL)
   {
      (void)ctx->current->run(ctx);
   }
   return ctx->terminate_val;
}
