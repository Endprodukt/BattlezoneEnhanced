/* audio.h - the sound's samples to the sound device.
 *
 * The SDK's vp_audio plays files and ready samples; this sound is made as
 * the game runs, so it has an SDL audio device of its own that pulls from
 * the ring of sound/sound.h. With no sound device the game runs silent.
 */
#ifndef BZ_AUDIO_H
#define BZ_AUDIO_H

#include "sound/sound.h"

/* Returns 0, or -1 if no device could be opened (a message is printed). */
int  bz_audio_open(bz_sound *snd);
void bz_audio_close(void);

/* Keep the ring from running full when the device takes samples a little
 * slower than the game makes them: call once per picture. */
void bz_audio_balance(bz_sound *snd);

/* Samples the device wanted and the game had not made yet (it ran dry: a
 * click), and samples dropped because it ran behind, since the last call. */
void bz_audio_trouble(uint32_t *dry, uint32_t *dropped);

#endif
