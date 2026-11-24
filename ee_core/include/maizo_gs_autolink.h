/* Link-time automatic DMA interposition for source-available EE games.
 * Link with --wrap=dma_channel_send_normal and maizo_gs_autolink.c.
 * After ABI binding, mark a single known exclusive draw submission.
 * Other calls (including UI, texture upload, sound and unknown packets)
 * pass through untouched. Commercial stripped ELF needs a verified hook
 * site and scratch allocation; this does NOT install that binary hook. */
#ifndef MAIZO_GS_AUTOLINK_H
#define MAIZO_GS_AUTOLINK_H
void mzGsAutoEligible(int exclusive);
#endif
