/* SPDX-License-Identifier: GPL-2.0-or-later
 * First person (tweak_first_person in bbport.ini, Gameplay patches in the launcher): hold Lock On
 * (R3, or its key) for a second to look out through the hunter's eyes, hold it again for the
 * usual camera. A tap still locks on. The camera itself is the shared core in
 * third_party/first-person-bloodborne; first_person.c hooks it in. */
#ifndef BB_FIRST_PERSON_H
#define BB_FIRST_PERSON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hooks the follow camera's update (a game build the core knows); returns how many hooks went in. */
int bb_first_person_install(void);

/* The pad buttons the game is given, at each read: times the Lock On hold. */
void bb_first_person_pad(uint32_t buttons);

#ifdef __cplusplus
}
#endif

#endif
