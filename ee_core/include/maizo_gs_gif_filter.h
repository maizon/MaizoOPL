/*
 * MAIZO GS GIF packet filter core (experimental, NOT a universal game hook).
 *
 * PS2 GS receives PRIM/PRMODE, TEX1, DTHE, DIMX and SCANMSK as GIF drawing
 * commands, unlike DISPLAY/PMODE scanout configuration. This library can
 * patch an OWNED, WRITABLE GIF command buffer before DMA submission.
 *
 * IMPORTANT: It DOES NOT intercept game DMAs, allocate memory, or run each
 * frame. Not yet wired to gameplay. It is intentionally not exposed as
 * a working In-Game toggle. On/off below means policies for a packet
 * that is actually supplied to mzGifApplyFilters().
 *
 * This is original MAIZO code. GIF register behavior documented by ps2tek,
 * PS2 GS User's Manual and the PCSX2 GSRegs definitions.
 */
#ifndef MAIZO_GS_GIF_FILTER_H
#define MAIZO_GS_GIF_FILTER_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MZ_GS_KEEP = 0,
    MZ_GS_FORCE_ON = 1,
    MZ_GS_FORCE_OFF = 2,
    MZ_GS_TRILINEAR = 3, /* TEX1 only */
    MZ_GS_AA1_ADAPTIVE = 3 /* AA1 field only; scoped, fail closed */
};

/*
 * scanmask is NOT a true scanlines shader: it masks GS drawing to one
 * parity of framebuffer rows, which can leave old-frame ghosts.
 * Explicitly experimental and never enabled by default.
 */
enum {
    MZ_GS_SCAN_UNCHANGED = 0,
    MZ_GS_SCAN_ODD_ONLY = 2, /* prohibit even rows */
    MZ_GS_SCAN_EVEN_ONLY = 3 /* prohibit odd rows; 1 is reserved */
};

typedef struct {
    uint8_t aa1;
    uint8_t bilinear; /* texture mode bits 0..1; LOD mode bits 2..3 */
    uint8_t dither;
    uint8_t scanmask;
} mz_gs_gif_policy_t;

typedef struct {
    uint32_t tags;
    uint32_t primitive_changes;
    uint32_t texture_changes;
    uint32_t dither_changes;
    uint32_t scanmask_changes;
    uint32_t unsupported_primitives;
    uint32_t protected_dither;
} mz_gs_gif_stats_t;

typedef struct {
    uint8_t framebuffer_psm[2];
    uint8_t framebuffer_known[2];
    uint8_t dither_matrix_known;
    /* Track the selected GS rendering context for 16-bit dithering. */
    uint8_t prim_known;
    uint8_t prim_kind;
    uint8_t active_ctx;
    uint8_t active_ctx_known;
    /* PRMODECONT.AC selects PRIM (1) or PRMODE (0) for GS attributes. */
    uint8_t prmode_cont_known;
    uint8_t prmode_cont_ac;
    uint8_t prim_ctx;
    uint8_t prmode_ctx;
    uint8_t prmode_ctx_known;
} mz_gs_gif_state_t;

static inline uint64_t mzGifRead64(const uint8_t *ptr)
{
    uint64_t v;
    memcpy(&v, ptr, sizeof(v));
    return v;
}

static inline void mzGifWrite64(uint8_t *ptr, uint64_t value)
{
    memcpy(ptr, &value, sizeof(value));
}

static inline int mzGifPrimitiveSupported(uint64_t prim)
{
    unsigned int kind = (unsigned int)(prim & 7ULL);
    /* AA1 is a per-primitive GS feature for lines and triangles.
     * GS draws coverage-fringe pixels with its own blending regardless of
     * PRIM.ABE; requiring ABE here wrongly skips most opaque geometry.
     * DO NOT set ABE: changing the regular blend mode breaks games. */
    return kind >= 1 && kind <= 5;
}

static inline uint64_t mzGifPrimitive(uint64_t src,
                                      unsigned int mode,
                                      mz_gs_gif_stats_t *stats)
{
    uint64_t result = src;
    if (mode == MZ_GS_FORCE_OFF) {
        result &= ~(1ULL << 7);
    } else if (mode == MZ_GS_FORCE_ON) {
        if (mzGifPrimitiveSupported(src))
            result |= 1ULL << 7;
        else
            stats->unsupported_primitives++;
    }
    if (result != src)
        stats->primitive_changes++;
    return result;
}

static inline uint64_t mzGifTex1(uint64_t src,
                                 unsigned int mode,
                                 mz_gs_gif_stats_t *stats)
{
    unsigned int min, new_min, lod=mode>>2;
    uint64_t result=src;
    mode &= 3u;
    if ((!mode && !lod) || lod>2u)
        return src;

    min = (unsigned int)((src >> 6) & 7ULL);
    if (min >= 6u) return src; /* reserved MMIN: do not reinterpret */
    new_min = min;
    if (mode == MZ_GS_FORCE_ON || mode == MZ_GS_TRILINEAR) {
        /* Preserve the selected mipmap family; change nearest -> linear.
           MMIN values 6/7 are reserved and must never be invented. */
        if (min == 0)
            new_min = 1;
        else if (min == 2)
            new_min = 4;
        else if (min == 3)
            new_min = 5;
        /* Upgrade only an existing mipmapped sampler with nonzero MXL.
         * Never manufacture levels or change their base pointers. */
        if (mode == MZ_GS_TRILINEAR && min >= 2 && ((src >> 2) & 7ULL))
            new_min = 5;
    } else if (mode == MZ_GS_FORCE_OFF) {
        if (min == 1)
            new_min = 0;
        else if (min == 4)
            new_min = 2;
        else if (min == 5)
            new_min = 3;
    }

    if(mode){
        result = (src & ~(1ULL << 5 | 7ULL << 6)) |
                 ((uint64_t)new_min << 6);
        if (mode == MZ_GS_FORCE_ON || mode == MZ_GS_TRILINEAR)
            result |= 1ULL << 5;
    }
    /* TEX1.K is signed 12-bit fixed point (4 fraction bits).
     * Bias only existing mip chains using calculated LOD. */
    if(lod && min>=2u && ((src>>2)&7u) && !(src&1u)){
        int k=(int)((src>>32)&4095u);
        if(k&2048)k-=4096;
        k+=lod==1u?-8:8;
        if(k < -2048)k=-2048;
        if(k > 2047)k=2047;
        result=(result&~(4095ULL<<32))|((uint64_t)((unsigned)k&4095u)<<32);
    }
    if (result != src)
        stats->texture_changes++;
    return result;
}

/* PRMODE has no PRIM primitive kind in bits 0..2: do not treat it as
   PRIM, otherwise AA1 silently fails on every PRMODE write. */
static inline uint64_t mzGifPrmode(uint64_t src,
                                  unsigned int mode,
                                  const mz_gs_gif_state_t *state,
                                  mz_gs_gif_stats_t *stats)
{
    uint64_t result = src;
    (void)state;
    /* PRMODE may be configured BEFORE the first PRIM, especially in
     * engines that set a persistent draw environment and then switch
     * PRMODECONT.AC=0. It has no primitive-kind bits. AA1=1 is defined
     * on PRMODE independently of the current primitive, and the GS
     * ignores it for points/sprites. Do NOT invent/change ABE. */
    if (mode == MZ_GS_FORCE_OFF)
        result &= ~(1ULL << 7);
    else if (mode == MZ_GS_FORCE_ON)
        result |= 1ULL << 7;
    if (result != src)
        stats->primitive_changes++;
    return result;
}
/* Fully packed GIF A+D register IDs. */
enum {
    MZ_GS_PRIM = 0x00,
    MZ_GS_TEX1_1 = 0x14,
    MZ_GS_TEX1_2 = 0x15,
    MZ_GS_PRMODECONT = 0x1a,
    MZ_GS_PRMODE = 0x1b,
    MZ_GS_SCANMSK = 0x22,
    MZ_GS_DIMX = 0x44,
    MZ_GS_DTHE = 0x45,
    MZ_GS_FRAME_1 = 0x4c,
    MZ_GS_FRAME_2 = 0x4d
};

static inline uint64_t mzGifAdValue(uint64_t src,
                                    unsigned int register_id,
                                    const mz_gs_gif_policy_t *policy,
                                    mz_gs_gif_state_t *state,
                                    mz_gs_gif_stats_t *stats)
{
    unsigned int ctx;
    switch (register_id) {
    case MZ_GS_PRIM:
        state->prim_known = 1;
        state->prim_kind = (uint8_t)(src & 7ULL);
        state->prim_ctx = (uint8_t)((src >> 9) & 1ULL);
        if (!state->prmode_cont_known || state->prmode_cont_ac) {
            state->active_ctx = state->prim_ctx;
            state->active_ctx_known = 1;
        }
        return mzGifPrimitive(src, policy->aa1, stats);
    case MZ_GS_PRMODE:
        state->prmode_ctx = (uint8_t)((src >> 9) & 1ULL);
        state->prmode_ctx_known = 1;
        if (!state->prmode_cont_known || !state->prmode_cont_ac) {
            state->active_ctx = state->prmode_ctx;
            state->active_ctx_known = 1;
        }
        return mzGifPrmode(src, policy->aa1, state, stats);
    case MZ_GS_PRMODECONT:
        state->prmode_cont_known = 1;
        state->prmode_cont_ac = (uint8_t)(src & 1ULL);
        if (state->prmode_cont_ac) {
            state->active_ctx = state->prim_ctx;
            state->active_ctx_known = state->prim_known;
        } else {
            state->active_ctx = state->prmode_ctx;
            state->active_ctx_known = state->prmode_ctx_known;
        }
        return src;
    case MZ_GS_TEX1_1:
    case MZ_GS_TEX1_2:
        return mzGifTex1(src, policy->bilinear, stats);
    case MZ_GS_DIMX:
        state->dither_matrix_known = 1;
        return src;
    case MZ_GS_FRAME_1:
    case MZ_GS_FRAME_2:
        ctx = register_id == MZ_GS_FRAME_1 ? 0u : 1u;
        state->framebuffer_psm[ctx] = (uint8_t)((src >> 24) & 63ULL);
        state->framebuffer_known[ctx] = 1;
        return src;
    case MZ_GS_DTHE:
        if (policy->dither == MZ_GS_FORCE_OFF) {
            uint64_t value = src & ~1ULL;
            if (value != src)
                stats->dither_changes++;
            return value;
        }
        if (policy->dither == MZ_GS_FORCE_ON) {
            unsigned int active = state->active_ctx;
            int depth_16 = state->active_ctx_known &&
                state->framebuffer_known[active] &&
                (state->framebuffer_psm[active] == 2 ||
                 state->framebuffer_psm[active] == 10);
            /* Dithering needs a known 16-bit framebuffer AND an actual
               DIMX matrix supplied by the game. */
            if (!depth_16 || !state->dither_matrix_known) {
                stats->protected_dither++;
                return src;
            }
            if (!(src & 1ULL))
                stats->dither_changes++;
            return src | 1ULL;
        }
        return src;
    case MZ_GS_SCANMSK:
        if (policy->scanmask == MZ_GS_SCAN_ODD_ONLY ||
            policy->scanmask == MZ_GS_SCAN_EVEN_ONLY) {
            uint64_t value = (src & ~3ULL) | policy->scanmask;
            if (src != value)
                stats->scanmask_changes++;
            return value;
        }
        return src;
    default:
        return src;
    }
}

/*
 * Adaptive AA1: prove a self-contained opaque draw, not just a PRIM bit.
 * The renderer must still certify this packet as optional world geometry,
 * with exclusive ownership. No cached state from other GIF paths or
 * skipped UI packets is trusted. Only one PACKED PRE draw is eligible,
 * followed by an existing explicit PRIM restore with the original value.
 * This preserves the original outgoing primitive state and topology.
 * A general frame or arbitrary commercial packet is NOT such a scope.
 */
static inline size_t mzGifAdaptiveScope(const uint8_t *buf,size_t bytes)
{
    uint64_t frame[2]={0,0},zbuf[2]={0,0},test[2]={0,0},tex0[2]={0,0};
    unsigned known[2]={0,0},ac=0,pabe=2;
    size_t at=0,draw=(size_t)-1;
    unsigned original=0;
    while(at+16u<=bytes){
        uint64_t tag=mzGifRead64(buf+at),regs=mzGifRead64(buf+at+8);
        unsigned loops=(unsigned)(tag&32767u),nr=(unsigned)(tag>>60),kind=(unsigned)((tag>>58)&3u);
        size_t payload;
        if(!nr)nr=16;
        if(kind||!loops)return (size_t)-1;
        payload=(size_t)loops*nr*16u;
        if(payload>bytes-at-16u)return (size_t)-1;
        if(draw!=(size_t)-1){
            /* The unmodified final PRIM is a required restore, not a
             * fabricated extra tag or a write during active DMA. */
            if(loops!=1||nr!=1||regs!=14u||(tag&(1ULL<<46))||
               !(tag&(1ULL<<15))||at+32u!=bytes||
               mzGifRead64(buf+at+16)!=original||
               mzGifRead64(buf+at+24)!=MZ_GS_PRIM)return (size_t)-1;
            return draw;
        }
        if(tag&(1ULL<<15))return (size_t)-1;
        if(tag&(1ULL<<46)){
            unsigned prim=(unsigned)((tag>>47)&2047u),ctx=(prim>>9)&1u,k=prim&7u,vertices=0;
            unsigned psm=(unsigned)((frame[ctx]>>24)&63u);
            if(!((frame[ctx]>>16)&63u)||(psm!=0u&&psm!=1u&&psm!=2u&&psm!=10u))
                return (size_t)-1;
            if(ac!=1u||pabe!=0u||k<1||k>5||(prim&(0x40u|0x80u|0x100u))||
               (known[ctx]&7u)!=7u||(frame[ctx]>>32)||
               (zbuf[ctx]&(1ULL<<32))||
               (test[ctx]&0x4001u)||!(test[ctx]&(1ULL<<16))||
               ((test[ctx]>>17)&3u)<2u)return (size_t)-1;
            /* TCC=RGB only: never force AA over unknown texture alpha. */
            if((prim&16u)&&(!(known[ctx]&8u)||(tex0[ctx]&(1ULL<<34))))return (size_t)-1;
            for(unsigned i=0;i<nr;i++){
                unsigned reg=(unsigned)((regs>>(4u*i))&15u);
                if(reg==4u||reg==5u)vertices++;
                else if(reg!=1u&&reg!=2u&&reg!=3u&&reg!=10u)return (size_t)-1;
            }
            if(vertices!=1u||(k==1u&&(loops<2||loops%2u))||(k==2u&&loops<2)||
               (k==3u&&(loops<3||loops%3u))||(k>=4u&&loops<3u))return (size_t)-1;
            for(size_t i=0;i<(size_t)loops*nr;i++){
                unsigned reg=(unsigned)((regs>>(4u*(i%nr)))&15u);
                if((reg==4u||reg==5u)&&(mzGifRead64(buf+at+24u+i*16u)&(1ULL<<47)))
                    return (size_t)-1;
            }
            original=prim;draw=at;
        }else{
            /* Only state setup is accepted before the draw. Explicitly
             * exclude transfers, kick registers, PRMODE, unknown commands. */
            if(nr!=1||regs!=14u)return (size_t)-1;
            for(unsigned i=0;i<loops;i++){
                uint64_t v=mzGifRead64(buf+at+16u+i*16u),a=mzGifRead64(buf+at+24u+i*16u);
                unsigned c=(unsigned)a&1u;
                switch(a){
                case 0x1a:ac=(unsigned)v&1u;break;
                case 0x49:pabe=(unsigned)v&1u;break;
                case 0x4c:case 0x4d:frame[c]=v;known[c]|=1u;break;
                case 0x4e:case 0x4f:zbuf[c]=v;known[c]|=2u;break;
                case 0x47:case 0x48:c=(unsigned)a-0x47u;test[c]=v;known[c]|=4u;break;
                case 0x06:case 0x07:tex0[c]=v;known[c]|=8u;break;
                case 0x08:case 0x09:case 0x14:case 0x15:
                case 0x18:case 0x19:case 0x40:case 0x41:
                case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:break;
                default:return (size_t)-1;
                }
            }
        }
        at+=16u+payload;
    }
    return (size_t)-1;
}

/*
 * Rewrite a COMPLETE GIF packet stream, bounded by packet_size bytes.
 *  0    OK
 * -1    Invalid input / malformed tag / truncated packet; buffer unchanged
 * -2    Unsupported length/mode/policy; buffer unchanged
 *
 * The parser operates in two passes: validate the entire stream without any
 * write first; only then rewrite. No partial modifications on malformed data.
 * EOP stops the scan; DMA padding after EOP is not parsed or modified.
 *
 * The caller MUST own and synchronize the DMA buffer, flush the D-cache,
 * and call this only BEFORE submitting the GIF packet to hardware.
 * No live game-hook exists in this module.
 */
static inline int mzGifApplyFilters(void *packet,
                                    size_t packet_size,
                                    const mz_gs_gif_policy_t *policy,
                                    mz_gs_gif_state_t *state,
                                    mz_gs_gif_stats_t *stats)
{
    uint8_t *buf = (uint8_t *)packet;
    int pass;
    size_t adaptive_at=(size_t)-1;
    mz_gs_gif_state_t snapshot;

    if (!packet || !policy || !state || !stats || !packet_size ||
        (packet_size & 15u) || packet_size > (1u << 20))
        return -1;
    if (policy->aa1 > MZ_GS_AA1_ADAPTIVE ||
        policy->bilinear > 11u ||
        policy->dither > MZ_GS_FORCE_OFF ||
        (policy->scanmask == 1u || policy->scanmask > MZ_GS_SCAN_EVEN_ONLY))
        return -2;

    if(policy->aa1==MZ_GS_AA1_ADAPTIVE)adaptive_at=mzGifAdaptiveScope(buf,packet_size);
    snapshot = *state;

    for (pass = 0; pass < 2; pass++) {
        mz_gs_gif_state_t current = snapshot;
        mz_gs_gif_stats_t tally;
        size_t at = 0;
        int saw_end = 0;
        memset(&tally, 0, sizeof(tally));

        while (at + 16 <= packet_size) {
            uint64_t tag = mzGifRead64(buf + at);
            uint64_t registers = mzGifRead64(buf + at + 8);
            size_t loops = (size_t)(tag & 0x7fffULL);
            size_t nreg = (size_t)((tag >> 60) & 15ULL);
            size_t kind = (size_t)((tag >> 58) & 3ULL);
            size_t entries, payload;
            size_t i;
            int end = (int)((tag >> 15) & 1ULL);
            /* FLG=3 is IMAGE2, not a reserved mode. Like IMAGE it
             * transports opaque GS pixel data without register writes. */
            if (nreg == 0)
                nreg = 16;
            entries = loops * nreg;
            if (kind == 0)
                payload = entries * 16u;
            else if (kind == 1)
                payload = ((entries + 1u) / 2u) * 16u;
            else
                payload = loops * 16u;

            if (payload > packet_size - at - 16u)
                return -1;

            if (kind == 0 && (tag & (1ULL << 46))) {
                /* Embedded PRIM is meaningful for PACKED mode only. */
                uint64_t orig_prim = (tag >> 47) & 0x7ffULL;
                current.prim_kind = (uint8_t)(orig_prim & 7ULL);
                current.prim_known = 1;
                current.prim_ctx = (uint8_t)((orig_prim >> 9) & 1ULL);
                if (!current.prmode_cont_known || current.prmode_cont_ac) {
                    current.active_ctx = current.prim_ctx;
                    current.active_ctx_known = 1;
                }
                if (pass == 1) {
                    uint64_t new_prim = mzGifPrimitive(orig_prim,
                                                        policy->aa1, &tally);
                    if(at==adaptive_at){new_prim|=128ULL;tally.primitive_changes++;}
                    if (new_prim != orig_prim) {
                        tag &= ~(0x7ffULL << 47);
                        tag |= new_prim << 47;
                        mzGifWrite64(buf + at, tag);
                    }
                }
            }
            at += 16u;
            if (kind <= 1) {
                for (i = 0; i < entries; i++) {
                    size_t index = i % nreg;
                    unsigned int reg = (unsigned int)((registers >> (4u * index)) & 15ULL);
                    size_t offset = at + (kind == 0 ? 16u * i : 8u * i);
                    uint64_t value;
                    if (kind == 0 && reg == 0xe) {
                        uint64_t addr = mzGifRead64(buf + offset + 8);
                        unsigned int reg_id = (unsigned int)(addr & 255ULL);
                        value = mzGifRead64(buf + offset);
                        if (pass == 1) {
                            uint64_t changed = mzGifAdValue(value, reg_id,
                                         policy, &current, &tally);
                            if (value != changed)
                                mzGifWrite64(buf + offset, changed);
                        }
                    } else if (reg == 0x00) {
                        value = mzGifRead64(buf + offset);
                        if (pass == 1) {
                            uint64_t changed = mzGifAdValue(value,
                                               MZ_GS_PRIM, policy,
                                               &current, &tally);
                            if (value != changed)
                                mzGifWrite64(buf + offset, changed);
                        }
                    }
                }
            }
            tally.tags++;
            at += payload;
            saw_end = end;
            if (end && at < packet_size) {
                /* One DMA payload may contain several complete GIF
                 * command lists. Stop only on zero-filled DMA padding. */
                size_t j;
                int padding = 1;
                for (j = at; j < packet_size; j++) {
                    if (buf[j] != 0) {
                        padding = 0;
                        break;
                    }
                }
                if (padding)
                    break;
            }
        }
        /* A GIF command list is complete only when its final tag has EOP.
         * Never attempt to rewrite streaming state with an open tag. */
        if (!saw_end)
            return -1;
        if (pass == 1) {
            *stats = tally;
            *state = current;
        }
    }
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
