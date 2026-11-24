/* Generated from profiles/mage/sky_evidence.json; frontend only. */
#ifndef MAIZO_MAGE_PROFILES_H
#define MAIZO_MAGE_PROFILES_H
#include "../ee_core/include/maizo_mage_profile.h"
#include <string.h>
static inline void mzSkyChoose(const char *id,mz_sky_record_t *out)
{
    static const struct {char id[12];mz_sky_record_t record;} profiles[]={
        {"SLES_525.84", {0x001f4658u, 0x001f4b58u, 0x004b8980u, 0x004b8a00u, 0x0049b1c8u, 0x17cbed3fu, 0x001f4e5cu, 0x3c39041au, 0u, 0u}},
        {"SLES_539.67", {0x002fd4a0u, 0x002fd9f8u, 0x005ec4a0u, 0x005ec520u, 0x005c5568u, 0x1fc4fa39u, 0x002fdd18u, 0xbd209bdfu, 0u, 0u}},
        {"SLUS_205.52", {0x003bd5c8u, 0x003bdac8u, 0x004ae360u, 0x004ae3b0u, 0x0044dea0u, 0xc651bb36u, 0x003bddbcu, 0x80cf620du, 0u, 0u}},
        {"SLUS_207.77", {0x003b6ca8u, 0x003b71a8u, 0x004cd8a0u, 0x004cd8f0u, 0x004ba930u, 0x45413539u, 0x003b74acu, 0xae21de26u, 0u, 0u}},
        {"SLUS_209.46", {0x0034fd68u, 0x00350268u, 0x0064e2d0u, 0x0064e350u, 0x00600d88u, 0x7fd09a3bu, 0x0035056cu, 0x023909aau, 0u, 0u}},
        {"SLUS_212.15", {0x0048e250u, 0x0048e750u, 0x0058f060u, 0x0058f0e0u, 0x0052f628u, 0x2107049fu, 0x0048ea54u, 0x6b1cb7au, 0u, 0u}},
        {"SLUS_212.69", {0x002356c8u, 0x00235bc8u, 0x005c36d0u, 0x005c3750u, 0x00578b70u, 0x55e938b1u, 0x00235eccu, 0x25809052u, 0u, 0u}},
        {"SLUS_213.76", {0x002b5088u, 0x002b5590u, 0x00402f20u, 0x00402fa0u, 0x003c3d90u, 0xb5784ba3u, 0x002b5894u, 0x4774caaau, 0x001c27a0u, 0x50270b46u}},
        {"SLUS_218.99", {0x002fef48u, 0x002ff4a0u, 0x0058a3c0u, 0x0058a440u, 0x0047c390u, 0x6c8a0849u, 0x002ff7c0u, 0x3f75a261u, 0u, 0u}},
    };
    memset(out,0,sizeof(*out));
    for(unsigned i=0;i<sizeof(profiles)/sizeof(profiles[0]);i++)
        if(!strcmp(id,profiles[i].id)){*out=profiles[i].record;return;}
}
#endif
