/* SPDX-License-Identifier: GPL-2.0-or-later
 * First person (first_person.h) for bbport: the adapter between bbport's engine hooks and the
 * camera core, third_party/first-person-bloodborne (MIT, Nick Saris; its README and src/bbfp.c
 * say what each hook does and why). Ported from his First Person for Dark Souls: Remastered.
 *
 * The core names its sites for the build it recognises (image offsets of game 1.09, checked by
 * their code, hashed as this loader has it); this file hooks them with bb_hook_mid and
 * bb_hook_call, gives the core bbport's guest memory, settings (bbport.ini: tweak_first_person,
 * tweak_first_person_reduce_bob, first_person_fov_scale, camera_fov_scale) and pad, and calls
 * the game's dummy-poly resolver with the game's calling convention. BB_FIRST_PERSON_HOOKS=0
 * installs nothing; the core's BB_FP_* tuning variables apply. */
#include "first_person.h"

#include "bbfp.h"
#include "engine_hooks.h"
#include "gpu/bbgpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const BbfpGame *g_sites; /* the build hooked */

typedef void *(BB_GUEST_ABI *DmyPolyFn)(float *out, uint64_t chr, const int32_t *id);
static void call_dummy_poly(void *fn, float *out, uint64_t chr, const int32_t *id) { ((DmyPolyFn)fn)(out, chr, id); }

static const BbfpHost host = {bb_guest, bb_read, call_dummy_poly, bbgpu_setting_get, NULL};

static BbfpRegs core_regs(const BbRegs *regs) { return (BbfpRegs){regs->rsi, regs->rbp, regs->r13, regs->r15}; }

static BB_GUEST_ABI void on_row(BbRegs *regs) {
    BbfpRegs r = core_regs(regs);
    bbfp_on_row(&r);
    regs->rsi = r.rsi;
}
static BB_GUEST_ABI void on_fov(BbRegs *regs) {
    const BbfpRegs r = core_regs(regs);
    bbfp_on_fov(&r);
}
static BB_GUEST_ABI void on_follow_delta(BbRegs *regs) {
    const BbfpRegs r = core_regs(regs);
    bbfp_on_follow_delta(&r);
}
static BB_GUEST_ABI void on_anchor(BbRegs *regs) {
    const BbfpRegs r = core_regs(regs);
    bbfp_on_anchor(&r);
}
static BB_GUEST_ABI void on_parts(BbRegs *regs) {
    const BbfpRegs r = core_regs(regs);
    bbfp_on_parts(&r);
}

/* Each of the camera update's sweep calls: no hit while the core says so, else the sweep. */
typedef uint8_t(BB_GUEST_ABI *SweepFn)(uint64_t world, int32_t filter, const void *start, const void *sweep,
                                       void *hit, uint64_t extra, void *out, float radius);
static BB_GUEST_ABI uint8_t on_sweep(uint64_t world, int32_t filter, const void *start, const void *sweep,
                                     void *hit, uint64_t extra, void *out, float radius) {
    if (bbfp_sweep_blocked()) return 0;
    return ((SweepFn)bb_guest(g_sites->sweep))(world, filter, start, sweep, hit, extra, out, radius);
}

static uint64_t clock_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u;
}

void bb_first_person_pad(uint32_t buttons) { bbfp_pad(buttons, clock_us()); }

int bb_first_person_install(void) {
    const char *env = getenv("BB_FIRST_PERSON_HOOKS");
    if (env && !strcmp(env, "0")) return 0;
    /* Every site first, so a mismatch leaves the game unchanged. */
    const BbfpGame *game = bbfp_find_game(bb_guest, bb_code_hash);
    if (!game) {
        printf("First person: the camera code is not a game build the core knows; not installed\n");
        return 0;
    }
    g_sites = game;
    const struct {
        BbfpSite site;
        void(BB_GUEST_ABI *host)(BbRegs *);
    } mids[] = {{game->row, on_row},
                {game->fov, on_fov},
                {game->follow_delta, on_follow_delta},
                {game->anchor, on_anchor},
                {game->parts, on_parts}};
    /* Inert until all are in: every hook does nothing until bbfp_start. */
    int hooks = 0;
    for (size_t i = 0; i < sizeof(mids) / sizeof(*mids); ++i, ++hooks)
        if (bb_hook_mid(mids[i].site.at, (BbCode){mids[i].site.n, mids[i].site.hash}, mids[i].host)) {
            printf("First person: %s\n", bb_hooks_error());
            return hooks;
        }
    for (int i = 0; i < BBFP_SWEEP_CALLS; ++i, ++hooks)
        if (bb_hook_call(game->sweep_calls[i], game->sweep, (void *)on_sweep)) {
            printf("First person: %s\n", bb_hooks_error());
            return hooks;
        }
    bbfp_start(&host, game);
    return hooks;
}
