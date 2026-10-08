/* SPDX-License-Identifier: MIT */
/* Host-test stand-in for zephyr/smf.h (State Machine Framework): Zephyr's
   names, layouts and contract for flat state machines (no parent states), as
   CONFIG_SMF without CONFIG_SMF_ANCESTOR_SUPPORT. Implemented in zephyr_sim.c.
   smf_set_state() runs the current state's exit and the new state's entry at
   once; an entry action may set the next state itself. */
#ifndef _SHIM_SMF_H
#define _SHIM_SMF_H
#include "zephyr_shim.h"

enum smf_state_result
{
   SMF_EVENT_HANDLED,
   SMF_EVENT_PROPAGATE,
};

typedef void (*state_method)(void *obj);
typedef enum smf_state_result (*state_execution)(void *obj);

struct smf_state
{
   const state_method entry;
   const state_execution run;
   const state_method exit;
   const struct smf_state *parent;
   const struct smf_state *initial;
};

struct smf_ctx
{
   const struct smf_state *current;
   const struct smf_state *previous;
   int32_t terminate_val;
   uint32_t internal;
};

#define SMF_CREATE_STATE(_entry, _run, _exit, _parent, _initial) \
   { .entry = (_entry), .run = (_run), .exit = (_exit), .parent = (_parent), .initial = (_initial) }
#define SMF_CTX(o)            ((struct smf_ctx *)(o))

extern void smf_set_initial(struct smf_ctx *ctx, const struct smf_state *init_state);
extern void smf_set_state(struct smf_ctx *ctx, const struct smf_state *new_state);
extern void smf_set_terminate(struct smf_ctx *ctx, int32_t val);
extern int32_t smf_run_state(struct smf_ctx *ctx);

#endif //!_SHIM_SMF_H
