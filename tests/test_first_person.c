/* SPDX-License-Identifier: GPL-2.0-or-later
 * first_person.c, the adapter to the camera core (third_party/first-person-bloodborne, whose own
 * test covers the camera itself): each hook hands the core the registers it reads and writes rsi
 * back; the game's resolver is called with the game's calling convention (a stand-in that faults
 * on an unaligned output); the sweep calls reach the game's sweep unless the core blocks them; the
 * pad; the refusal of an image that is not the game's and, with the player's game (game_code.h),
 * the install on the game's own code. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "game_code.h"

#include "../third_party/first-person-bloodborne/src/bbfp.c"
#include "../src/first_person.c"

#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); \
} } while (0)

static int g_setting = 1;
int bbgpu_setting_get(const char *key, int *value) {
    if (strcmp(key, "tweak_first_person")) return 0;
    *value = g_setting;
    return 1;
}

static const size_t IMAGE = 0x5600000;
static unsigned char *g_image;

static int close_to(float a, float b) { return fabsf(a - b) < 1e-4f; }
static void putf(void *base, size_t offset, float value) { memcpy((unsigned char *)base + offset, &value, 4); }
static float getf(const void *base, size_t offset) {
    float value;
    memcpy(&value, (const unsigned char *)base + offset, 4);
    return value;
}
static void put64(void *base, size_t offset, uint64_t value) { memcpy((unsigned char *)base + offset, &value, 8); }
static uint64_t addr(const void *p) { return (uint64_t)(uintptr_t)p; }
static void put3(void *base, size_t offset, float x, float y, float z) {
    putf(base, offset, x), putf(base, offset + 4, y), putf(base, offset + 8, z);
}
static int at3(const void *base, size_t offset, float x, float y, float z) {
    return close_to(getf(base, offset), x) && close_to(getf(base, offset + 4), y) && close_to(getf(base, offset + 8), z);
}

/* An install that writes nothing: 0 hooks and the image as it was. */
static void refused(void) {
    unsigned char *before = malloc(IMAGE);
    assert(before);
    memcpy(before, g_image, IMAGE);
    assert(bb_first_person_install() == 0);
    assert(!memcmp(before, g_image, IMAGE));
    assert(!g_installed);
    free(before);
}

/* The stand-in resolver's answer (a 3x4 world matrix) and the id it was asked for. */
static _Alignas(16) float g_head_matrix[12];
static volatile int32_t g_asked = -1;
static void head_at(float x, float y, float z) {
    static const float identity[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    memcpy(g_head_matrix, identity, sizeof(identity));
    g_head_matrix[3] = x, g_head_matrix[7] = y, g_head_matrix[11] = z;
}

/* At the resolver's site, System V (out, chr, &id): records *id, faults on an output that is not
 * 16-byte aligned (the game's own stores are aligned), copies g_head_matrix to out, returns out. */
static void place_resolver(uint64_t at) {
    unsigned char code[96], *p = code;
    const uint64_t asked = addr((const void *)&g_asked), matrix = addr(g_head_matrix);
    *p++ = 0x8b, *p++ = 0x02;                                /* mov eax, [rdx] */
    *p++ = 0xa3, memcpy(p, &asked, 8), p += 8;               /* mov [asked], eax */
    *p++ = 0x40, *p++ = 0xf6, *p++ = 0xc7, *p++ = 0x0f;      /* test dil, 15 */
    *p++ = 0x74, *p++ = 0x02, *p++ = 0x0f, *p++ = 0x0b;      /* jz +2; ud2 */
    *p++ = 0x48, *p++ = 0x89, *p++ = 0xf8;                   /* mov rax, rdi */
    *p++ = 0x48, *p++ = 0xbe, memcpy(p, &matrix, 8), p += 8; /* mov rsi, matrix */
    static const unsigned char copy[] = {0x0f, 0x10, 0x06, 0x0f, 0x11, 0x07, 0x0f, 0x10, 0x46, 0x10, 0x0f, 0x11,
                                         0x47, 0x10, 0x0f, 0x10, 0x46, 0x20, 0x0f, 0x11, 0x47, 0x20, 0xc3};
    memcpy(p, copy, sizeof(copy)), p += sizeof(copy);
    memcpy(g_image + at, code, (size_t)(p - code));
    FlushInstructionCache(GetCurrentProcess(), g_image + at, (size_t)(p - code));
}

static int sweep(void) { return on_sweep(0, 0, NULL, NULL, NULL, 0, NULL, 0.2f); }

/* The adapter's hooks, run by hand on a stand-in camera with the core started. */
static void check_wiring(void) {
    static _Alignas(16) unsigned char man[0x100], chr[0x600], position[0x100], module[0x200], cam[0x400],
        row[ROW_SIZE], frame[0x400], model[0x100], helmet[0x40], drawable[0x220];
    const BbfpGame *game = &bbfp_games[0];
    put64(g_image, game->world_chr_man, addr(man));
    put64(man, PLAYER, addr(chr));
    put64(chr, CHR_POSITION, addr(position));
    put64(position, POSITION_MODULE, addr(module));
    putf(module, FACING, 0.5f);
    static const unsigned char sweep_hit[] = {0xb0, 0x01, 0xc3}; /* mov al, 1; ret: a hit */
    memcpy(g_image + game->sweep, sweep_hit, sizeof(sweep_hit));
    FlushInstructionCache(GetCurrentProcess(), g_image + game->sweep, sizeof(sweep_hit));
    place_resolver(game->dummy_poly.at);
    head_at(10.0f, 20.0f, 30.0f);
    put3(cam, PIVOT, 10.0f, 20.2f, 30.0f);
    put3(cam, EYE, 10.3f, 20.2f, 30.4f);
    putf(cam, FOV, 0.75f);
    putf(cam, YAW, 10.0f);
    g_sites = game;
    bbfp_start(&host, game);
    BbRegs r = {0};
    r.r15 = addr(chr), r.r13 = addr(cam), r.rbp = addr(frame + 0x200), r.rsi = addr(row);

    /* Not wanted: the row is the game's, and the sweep calls reach the game's sweep. */
    on_row(&r);
    on_fov(&r);
    assert(r.rsi == addr(row) && !g_engaged && sweep() == 1);

    /* Wanted: the core engages through the game's resolver, asked for Dummy_eye. */
    g_wanted = 1;
    on_fov(&r);
    assert(g_asked == 58 && g_engaged && g_engaged_chr == addr(chr));
    /* rsi written back: the update reads the core's row. */
    on_row(&r);
    assert(r.rsi == addr(g_row));
    /* rbp: the follow delta and the attach position in the update's frame. */
    on_follow_delta(&r);
    assert(at3(frame + 0x200 - DELTA_SLOT, 0, g_delta[0], g_delta[1], g_delta[2]));
    on_anchor(&r);
    assert(at3(frame + 0x200 - ANCHOR_POS, 0, g_target[0], g_target[1], g_target[2]));
    /* r15: the part pass of the player hides the helmet (part 2). */
    put64(chr, MODEL, addr(model));
    put64(model, PART_BASE + 8 * 2, addr(helmet));
    put64(helmet, PART_DRAWABLE, addr(drawable));
    putf(drawable, DRAW_COLOR + 12, 1.0f);
    drawable[DRAW_FLAGS] = 0x83;
    on_parts(&r);
    assert(getf(drawable, DRAW_COLOR + 12) == 0.0f && drawable[DRAW_TRANSLUCENT] == 1 && drawable[DRAW_FLAGS] == 0x81);
    /* In first person the sweeps report no hit. */
    assert(sweep() == 0);

    /* The pad: switched off forgets the choice, and the next update is third person again. */
    g_setting = 0;
    bb_first_person_pad(0);
    assert(!g_wanted);
    on_fov(&r);
    assert(!g_engaged && sweep() == 1);
    g_setting = 1;

    bbfp_stop();
    g_sites = NULL;
    g_asked = -1;
    memset(g_image + game->sweep, 0, sizeof(sweep_hit));
    memset(g_image + game->dummy_poly.at, 0, 96);
    put64(g_image, game->world_chr_man, 0);
}

static int branches_to(uint64_t site, uint64_t target) {
    int32_t rel;
    memcpy(&rel, g_image + site + 1, 4);
    return site + 5 + (int64_t)rel == target;
}

/* The host a hooked call now reaches: its stub is movabs r11, host; jmp r11. */
static uint64_t host_of(uint64_t site) {
    int32_t rel;
    uint64_t target;
    memcpy(&rel, g_image + site + 1, 4);
    memcpy(&target, g_image + site + 5 + (int64_t)rel + 2, 8);
    return target;
}

static void build_fixture(const BbfpGame *game) {
    memset(g_image, 0, IMAGE);
    const BbfpSite sites[] = {game->row, game->fov, game->follow_delta, game->anchor, game->parts, game->dummy_poly};
    for (size_t i = 0; i < sizeof(sites) / sizeof(*sites); ++i) bb_test_place_game_code(g_image, sites[i].at, sites[i].n);
    for (int i = 0; i < BBFP_SWEEP_CALLS; ++i) bb_test_place_game_code(g_image, game->sweep_calls[i], 5);
}

static void check_game(void) {
    const BbfpGame *game = &bbfp_games[0];
    /* Each site that is not the game's refuses the whole install. */
    const uint64_t changed[] = {game->row.at + 3, game->fov.at + 8, game->follow_delta.at + 4, game->anchor.at,
                                game->dummy_poly.at + 20, game->parts.at + 3, game->sweep_calls[0] + 2,
                                game->sweep_calls[BBFP_SWEEP_CALLS - 1]};
    for (size_t i = 0; i < sizeof(changed) / sizeof(*changed); ++i) {
        build_fixture(game);
        g_image[changed[i]] ^= 1;
        refused();
    }
    _putenv("BB_FIRST_PERSON_HOOKS=0");
    build_fixture(game);
    refused();
    _putenv("BB_FIRST_PERSON_HOOKS=");

    build_fixture(game);
    for (int i = 0; i < BBFP_SWEEP_CALLS; ++i) assert(branches_to(game->sweep_calls[i], game->sweep));
    assert(bb_first_person_install() == 5 + BBFP_SWEEP_CALLS && g_installed && g_sites == game);
    for (int i = 0; i < BBFP_SWEEP_CALLS; ++i) {
        assert(!branches_to(game->sweep_calls[i], game->sweep));
        assert(host_of(game->sweep_calls[i]) == addr(on_sweep));
    }
    /* Hooked sites are no longer the game's: a second install refuses. */
    bbfp_stop();
    unsigned char *before = malloc(IMAGE);
    assert(before);
    memcpy(before, g_image, IMAGE);
    assert(bb_first_person_install() == 0 && !g_installed);
    assert(!memcmp(before, g_image, IMAGE));
    free(before);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    g_image = VirtualAlloc(NULL, IMAGE, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    assert(g_image);
    assert(bb_hooks_init(g_image, IMAGE) == 0);
    refused(); /* another image */
    check_wiring();
    const char *game = getenv("BB_TEST_EBOOT_ELF");
    if (game && *game) {
        check_game();
        puts("first person: ok (with the game's code)");
        return 0;
    }
    puts("first person: ok (BB_TEST_EBOOT_ELF unset: the game's code not checked)");
    return 0;
}
