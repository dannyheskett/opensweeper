#ifndef OPENSWEEPER_SOUND_H
#define OPENSWEEPER_SOUND_H

#include <stdbool.h>

typedef enum {
    SFX_REVEAL = 0,
    SFX_FLAG,
    SFX_UNFLAG,
    SFX_CHORD,
    SFX_DETONATE,
    SFX_WIN,
    SFX_MENU_MOVE,
    SFX_MENU_SELECT,
    SFX_COUNT,
} SfxId;

void sound_init(void);
void sound_shutdown(void);
bool sound_is_enabled(void);
void sound_toggle(void);
void sound_play(SfxId id);

#endif
