#include "include/cheatconfig.h"
/*
 * Manage cheat codes
 *
 * Copyright (C) 2009-2010 Mathias Lafeldt <misfire@debugon.org>
 * Copyright (C) 2014 doctorxyz
 *
 * This file is part of PS2rd, the PS2 remote debugger.
 *
 * PS2rd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PS2rd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PS2rd.  If not, see <http://www.gnu.org/licenses/>.
 *
 * $Id$
 */

#include <unistd.h>
#include "include/cheatman.h"
#include "include/ioman.h"
#include "include/maizo_cheatdb.h"
#include "include/cheatdb_storage.h"
#include "include/maizo_cheat_map.h"
#include <zlib.h>

static int gEnableCheat; // Enables PS2RD Cheat Engine - 0 for Off, 1 for On
static int gCheatMode;   // Cheat Mode - 0 Enable all cheats, 1 Cheats selected by user

static u32 gCheatList[MAX_CHEATLIST]; // Store hooks/codes addr+val pairs
cheat_entry_t gCheats[MAX_CODES];

void InitCheatsConfig(config_set_t *configSet)
{
    config_set_t *configGame = configGetByType(CONFIG_GAME);

    // Default values.
    gCheatSource = 0;
    gEnableCheat = 0;
    gCheatMode = 0;

    if (configGetInt(configSet, CONFIG_ITEM_CHEATSSOURCE, &gCheatSource)) {
        // Load the rest of the per-game CHEAT configuration if CHEAT is enabled.
        if (configGetInt(configSet, CONFIG_ITEM_ENABLECHEAT, &gEnableCheat) && gEnableCheat) {
            configGetInt(configSet, CONFIG_ITEM_CHEATMODE, &gCheatMode);
        }
    } else {
        if (configGetInt(configGame, CONFIG_ITEM_ENABLECHEAT, &gEnableCheat) && gEnableCheat) {
            configGetInt(configGame, CONFIG_ITEM_CHEATMODE, &gCheatMode);
        }
    }
}

int GetCheatsEnabled(void)
{
    return gEnableCheat;
}

const u32 *GetCheatsList(void)
{
    return gCheatList;
}

static char cheatDbSource[80], cheatDbHeading[256], cheatDbFingerprint[16];
static int cheatDbMatch;
static int cheatSkipped;
static unsigned variantGroup;

const char *cheatDatabaseSource(void) { return cheatDbSource; }
const char *cheatDatabaseHeading(void) { return cheatDbHeading; }
const char *cheatDatabaseFingerprint(void) { return cheatDbFingerprint; }
int cheatDatabaseMatch(void) { return cheatDbMatch; }
int cheatDatabaseSkipped(void) { return cheatSkipped; }

/* Only strict RAW address/value pairs are fed to PS2RD. A partial code
 * group or unresolved CheatDevice value map must never be run. */
static int rawLine(const char *line, code_t *out)
{
    unsigned int words[2] = {0, 0};
    const char *p = line;
    for (int w = 0; w < 2; w++) {
        if (w) {
            if (!isspace((unsigned char)*p))
                return 0;
            while (isspace((unsigned char)*p))
                p++;
        }
        for (int i = 0; i < 8; i++) {
            unsigned char c = *p;
            if (!isxdigit(c))
                return 0;
            words[w] = (words[w] << 4) | (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
            p++;
        }
    }
    while (isspace((unsigned char)*p))
        p++;
    if (*p || !words[0])
        return 0;
    out->addr = words[0];
    out->val = words[1];
    return 1;
}

static void finishCheat(char name[CHEAT_NAME_MAX + 1], code_t *codes, int count, int bad, int *total)
{
    if (!name[0] || (!count && !bad))
        return;
    if (bad || !count || *total >= MAX_CODES) {
        cheatSkipped++;
        return;
    }
    cheat_entry_t *entry = &gCheats[(*total)++];
    strncpy(entry->name, name, CHEAT_NAME_MAX);
    memcpy(entry->codes, codes, count * sizeof(*codes));
    entry->enabled = (gCheatMode == 0) || cheatConfigIsMasterCode(entry->name);
    int hookOnly = 1;
    for (int i = 0; i < count; i++)
        if ((codes[i].addr & 0xfe000000) != 0x90000000)
            hookOnly = 0;
    if (cheatDbMatch > 0 && hookOnly && !cheatConfigIsMasterCode(entry->name)) {
        snprintf(entry->name, sizeof(entry->name), "Master Code");
        entry->enabled = 1;
    }
}

static void finishMapped(char *name,code_t *codes,int count,int bad,int *total,
                         const mz_map_t *map,int line,uint32_t prefix,uint32_t mask)
{
    if(!map){finishCheat(name,codes,count,bad,total);return;}
    if(bad||line<0||line>=count||cheatConfigIsMasterCode(name)){cheatSkipped++;return;}
    for(int j=0;j<count;j++)
        if((codes[j].addr&0xfe000000u)==0x90000000u){cheatSkipped++;return;}
    unsigned group=++variantGroup;
    for(unsigned i=0;i<map->count;i++){
        if(map->values[i].value&~mask){cheatSkipped++;continue;}
        if(*total>=MAX_CODES){cheatSkipped++;break;}
        char label[CHEAT_NAME_MAX+1];
        snprintf(label,sizeof(label),"%.90s: %.31s",name,map->values[i].label);
        codes[line].val=prefix|map->values[i].value;
        int before=*total;
        finishCheat(label,codes,count,0,total);
        if(*total>before){
            gCheats[before].variant_group=group;
            gCheats[before].enabled=0;
        }
    }
}
void cheatToggleSelection(int index)
{
    if(index<0||index>=MAX_CODES||!gCheats[index].name[0]||
       cheatConfigIsMasterCode(gCheats[index].name))return;
    if(!gCheats[index].enabled&&gCheats[index].variant_group)
        for(int i=0;i<MAX_CODES&&gCheats[i].name[0];i++)
            if(gCheats[i].variant_group==gCheats[index].variant_group)gCheats[i].enabled=0;
    gCheats[index].enabled=!gCheats[index].enabled;
}

/* Handles CRLF, EOF without newline, and bounded group sizes. */
static int parse_buf(const char *buf)
{
    char line[512], name[CHEAT_NAME_MAX + 1] = {0};
    code_t *codes = malloc(MAX_CHEATLIST * sizeof(*codes));
    mz_map_t *maps=calloc(MZ_MAP_MAX,sizeof(*maps));
    int total=0,count=0,bad=0,inMap=-1,mapped=-1,mapLine=-1;
    unsigned mapCount=0;
    uint32_t prefix=0,mask=0;
    variantGroup=0;
    if (!buf || !codes || !maps) {
        free(codes);free(maps);
        return -1;
    }
    while (*buf) {
        const char *end = strchr(buf, '\n');
        size_t length = end ? (size_t)(end - buf) : strlen(buf);
        if (length >= sizeof(line)) {
            bad = 1;
            buf += length + (end != NULL);
            continue;
        }
        memcpy(line, buf, length);
        line[length] = 0;
        buf += length + (end != NULL);
        char *p = line;
        while (isspace((unsigned char)*p))
            p++;
        char *comment = strstr(p, "//");
        if (comment)
            *comment = 0;
        if (*p == '#')
            continue;
        length = strlen(p);
        while (length && isspace((unsigned char)p[length - 1]))
            p[--length] = 0;
        if (!length)
            continue;
        if (*p == '[') {
            finishMapped(name,codes,count,bad,&total,mapped<0?NULL:&maps[mapped],mapLine,prefix,mask);
            name[0]=0;count=bad=0;mapped=mapLine=-1;inMap=-1;
            char mapName[16];
            if(mzMapName(p+1,mapName)){
                for(unsigned i=0;i<mapCount;i++)
                    if(!strcmp(maps[i].name,mapName))inMap=i;
                if(inMap<0&&mapCount<MZ_MAP_MAX){
                    inMap=mapCount++;strcpy(maps[inMap].name,mapName);
                }
            }
            if(inMap<0)cheatSkipped++;
            continue;
        }
        if(inMap>=0){
            int entry=mzMapEntry(&maps[inMap],p);
            if(entry)continue;
            inMap=-1;
        }
        code_t code;
        if (rawLine(p, &code)) {
            if (!name[0])
                continue;
            if (count >= MAX_CHEATLIST - 1)
                bad = 1;
            else
                codes[count++] = code;
        } else if (length >= 9 && isspace((unsigned char)p[8]) &&
                   strspn(p, "0123456789abcdefABCDEF") == 8) {
            uint32_t addr,v,m;
            int index=mzMapReference(p,maps,mapCount,&addr,&v,&m);
            if(index<0||mapped>=0||!name[0]||count>=MAX_CHEATLIST-1)
                bad=1;
            else{
                mapped=index;mapLine=count;prefix=v;mask=m;
                codes[count++]=(code_t){addr,v};
            }
        } else {
            finishMapped(name,codes,count,bad,&total,mapped<0?NULL:&maps[mapped],mapLine,prefix,mask);
            count=bad=0;mapped=mapLine=-1;
            if (*p == '"') {
                name[0] = 0;
                continue;
            }
            snprintf(name, sizeof(name), "%.*s", CHEAT_NAME_MAX, p);
        }
    }
    finishMapped(name,codes,count,bad,&total,mapped<0?NULL:&maps[mapped],mapLine,prefix,mask);
    free(codes);free(maps);
    return total ? 0 : -1;
}

/**
 * read_text_file - Reads text from a file into a buffer.
 * @filename: name of text file
 * @maxsize: max file size (0: arbitrary size)
 * @return: ptr to NULL-terminated text buffer, or NULL if an error occured
 */
static inline char *read_text_file(const char *filename, int maxsize)
{
    char *buf = NULL;
    int fd, filesize;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        LOG("%s: Can't open text file %s\n", __FUNCTION__, filename);
        return NULL;
    }

    filesize = lseek(fd, 0, SEEK_END);
    if (filesize < 0) {
        LOG("%s: Can't seek in text file %s\n", __FUNCTION__, filename);
        close(fd);
        return NULL;
    }

    if (maxsize > 0 && filesize > maxsize) {
        LOG("%s: Text file too large: %i bytes, max: %i bytes\n", __FUNCTION__, filesize, maxsize);
        close(fd);
        return NULL;
    }

    buf = malloc(filesize + 1);
    if (buf == NULL) {
        LOG("%s: Unable to allocate %i bytes\n", __FUNCTION__, filesize + 1);
        close(fd);
        return NULL;
    }

    if (filesize > 0) {
        lseek(fd, 0, SEEK_SET);
        if (read(fd, buf, filesize) != filesize) {
            LOG("%s: Can't read from text file %s\n", __FUNCTION__, filename);
            free(buf);
            close(fd);
            return NULL;
        }
    }

    buf[filesize] = '\0';
    close(fd);

    return buf;
}

/*
 * Load cheats from text file.
 */
int load_cheats(const char *cheatfile)
{
    char *buf = NULL;
    int ret;

    memset(gCheats, 0, sizeof(gCheats));
    cheatSkipped = 0;

    LOG("%s: Reading cheat file '%s'...\n", __FUNCTION__, cheatfile);
    buf = read_text_file(cheatfile, 256 * 1024);
    if (buf == NULL) {
        LOG("%s: Could not read cheats file '%s'\n", __FUNCTION__, cheatfile);
        return -1;
    }

    ret = parse_buf(buf);
    free(buf);

    if (ret < 0)
        return ret;

    return (gCheatMode == 0) ? 0 : 1;
}

/* Configure and launch through the same resolver. Local CHT is legacy
 * fallback ONLY when no ZIP was found. Never merge regions or databases. */
int load_game_cheats(const char *prefix, const char *serial, const char *title)
{
    char *text = NULL;
    memset(gCheats, 0, sizeof(gCheats));
    cheatSkipped = 0;
    cheatDbSource[0] = cheatDbHeading[0] = cheatDbFingerprint[0] = 0;
    cheatDbMatch = cheatDbLoad(serial, title, &text, cheatDbHeading, cheatDbSource);
    if (cheatDbMatch > 0 && text) {
        snprintf(cheatDbFingerprint, sizeof(cheatDbFingerprint), "%08lX", (unsigned long)crc32(0, (const unsigned char *)text, strlen(text)));
        int result = parse_buf(text);
        free(text);
        return result < 0 ? -1 : 1; /* database always manual */
    }
    free(text);
    if (cheatDbSource[0] || cheatDbMatch < 0)
        return cheatDbMatch == MZDB_NONE ? -ENOENT : -EINVAL;
    if (prefix && *prefix) {
        char file[256];
        if (snprintf(file, sizeof(file), "%sCHT/%s.cht", prefix, serial) < sizeof(file)) {
            int result = load_cheats(file);
            if (result >= 0) {
                snprintf(cheatDbSource, sizeof(cheatDbSource), "%.79s", file);
                return result;
            }
        }
    }
    return -ENOENT;
}

int cheatHasOptionalSelection(void)
{
    for (int i = 0; i < MAX_CODES && gCheats[i].name[0]; i++)
        if (gCheats[i].enabled && !cheatConfigIsMasterCode(gCheats[i].name))
            return 1;
    return 0;
}

void cheatDisableLaunch(void)
{
    gEnableCheat = 0;
    memset(gCheats, 0, sizeof(gCheats));
    memset(gCheatList, 0, sizeof(gCheatList));
}

int cheatSelectionFits(void)
{
    int count = 0, hooks = 0, nextHook = 1;
    for(int i=0;i<MAX_CODES&&gCheats[i].name[0];i++)
        if(gCheats[i].enabled&&gCheats[i].variant_group)
            for(int j=0;j<i;j++)
                if(gCheats[j].enabled&&gCheats[j].variant_group==gCheats[i].variant_group)return 0;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < MAX_CODES && gCheats[i].name[0]; i++) {
            if (!gCheats[i].enabled || !!cheatConfigIsMasterCode(gCheats[i].name) != (pass == 0))
                continue;
            for (int j = 0; j < MAX_CHEATLIST && gCheats[i].codes[j].addr; j++) {
                u32 addr = gCheats[i].codes[j].addr;
                if ((addr & 0xfe000000) == 0x90000000 && nextHook)
                    hooks++;
                count += 2;
                nextHook = (addr & 0xf0000000) != 0x40000000 && (addr & 0xf0000000) != 0x30000000;
            }
        }
    return count <= MAX_CHEATLIST - 2 && hooks <= MAX_HOOKS && count / 2 - hooks <= MAX_CODES;
}

void set_cheats_list(void)
{
    int cheatCount = 0;

    memset((void *)gCheatList, 0, sizeof(gCheatList));
    if (!cheatSelectionFits())
        return;

    int hooks = 0, nextHook = 1;
    /* Master entries first, without truncating any enabled group. */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < MAX_CODES && gCheats[i].name[0]; i++) {
            if (!gCheats[i].enabled || !!cheatConfigIsMasterCode(gCheats[i].name) != (pass == 0))
                continue;
            for (int j = 0; j < MAX_CHEATLIST && gCheats[i].codes[j].addr; j++) {
                u32 addr = gCheats[i].codes[j].addr;
                if ((addr & 0xfe000000) == 0x90000000 && nextHook)
                    hooks++;
                if (hooks > MAX_HOOKS || cheatCount + 4 > MAX_CHEATLIST) {
                    memset(gCheatList, 0, sizeof(gCheatList));
                    return;
                }
                gCheatList[cheatCount++] = addr;
                gCheatList[cheatCount++] = gCheats[i].codes[j].val;
                nextHook = (addr & 0xf0000000) != 0x40000000 && (addr & 0xf0000000) != 0x30000000;
            }
        }
    }

    // Append a blank cheat entry if theres space
    if (cheatCount < MAX_CHEATLIST) {
        gCheatList[cheatCount++] = 0; // addr
        gCheatList[cheatCount++] = 0; // val
    } else {
        // Overwrite the last cheat with a blank cheat if we are at max
        gCheatList[cheatCount - 2] = 0; // addr
        gCheatList[cheatCount - 1] = 0; // val
    }
}

#ifdef MAIZO_IGR_MENU
/* Build a catalog without modifying the old list until every enabled
 * group fits. Master hooks are immutable and installed by PS2RD.
 * NULL is a dry run used by the frontend before accepting an empty
 * initial selection. A rejected catalog leaves the old list intact. */
int cheatBuildLive(mz_live_cheats_t *out)
{
    mz_live_cheats_t p={0};
    u32 list[MAX_CHEATLIST]={0};
    unsigned words=0,hooks=0;
    for(int i=0;i<MAX_CODES&&gCheats[i].name[0];i++){
        cheat_entry_t *e=&gCheats[i];
        if(!cheatConfigIsMasterCode(e->name))continue;
        for(int j=0;j<MAX_CHEATLIST&&e->codes[j].addr;j++){
            u32 a=e->codes[j].addr;
            if((a&0xfe000000u)!=0x90000000u||
               (a&0x01fffffcu)<0x100000u||hooks==MAX_HOOKS)return 0;
            list[words++]=a;list[words++]=e->codes[j].val;hooks++;
        }
    }
    if(!hooks)return 0;
    for(int pass=0;pass<2;pass++)
        for(int i=0;i<MAX_CODES&&gCheats[i].name[0];i++){
            cheat_entry_t *e=&gCheats[i];
            if(cheatConfigIsMasterCode(e->name)||!!e->enabled!=(pass==0))continue;
            unsigned n=0,j;
            while(n<MAX_CHEATLIST&&e->codes[n].addr)n++;
            for(j=0;j<n;j++){
                uint32_t addr=e->codes[j].addr,value=e->codes[j].val;
                mzLiveNormalize(&addr,&value);
                int span=mzLiveLineSpan(addr,value);
                if(span<0||(unsigned)span>=n-j)break;
            }
            if(!n||n==MAX_CHEATLIST||j!=n||
               p.count>=MZ_LIVE_GROUPS||n>MZ_LIVE_CODES-p.lines||
               e->variant_group>255u){
                if(e->enabled)return 0;
                continue;
            }
            unsigned group=p.count++;
            const char *value=e->variant_group?strrchr(e->name,':'):NULL;
            if(value&&strlen(e->name)>21){
                value++;
                while(*value==' ')value++;
                snprintf(p.name[group],MZ_LIVE_NAME,"%.9s: %.10s",e->name,value);
            }else snprintf(p.name[group],MZ_LIVE_NAME,"%.21s",e->name);
            p.variant[group]=e->variant_group;
            if(e->enabled)p.mask|=1u<<group;
            for(unsigned j=0;j<n;j++){
                p.owner[p.lines++]=group;
                uint32_t addr=e->codes[j].addr,value=e->codes[j].val;
                mzLiveNormalize(&addr,&value);
                list[words++]=addr;list[words++]=value;
            }
        }
    if(!mzLiveCatalogOK(&p))return 0;
    if(out){*out=p;memcpy(gCheatList,list,sizeof(list));}
    return 1;
}
#endif
