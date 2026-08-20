/*
 * A runtime workaround for MStar "m3" sets, where the video decoder accepts H.264 data and
 * then quietly stops displaying it after the first fraction of a second.
 *
 * MS_VDEC_Init, in the platform's libkadaptor, guards its setup on the codec type. On these
 * models the guard rejects the combination a direct-media client asks for, so decoding
 * starts and then stalls - buffers keep being accepted, the picture does not advance, and
 * nothing reports an error. Patching the comparison out lets it run.
 *
 * This is not something a sample invents: it is the same patch ss4s applies before using
 * NDL or LGNC (modules/webos/utils/m3_kadp_fix.c). It is included here because without it
 * the NDL DirectMedia v1 sample appears to work - it opens, feeds, and reports no failure -
 * while showing roughly 26 frames and freezing, which is a deeply confusing way to learn
 * about a hardware quirk.
 *
 * Harmless elsewhere: if the symbol or the instruction pattern is absent, nothing happens.
 */
#pragma once

/* Returns 0 if the patch was applied or was not needed, non-zero if it could not be. */
int webos_m3_vdec_fix(void);
