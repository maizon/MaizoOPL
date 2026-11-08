#include "include/opl.h"
#include "include/texcache.h"
#include "include/textures.h"
#include "include/ioman.h"
#include "include/gui.h"
#include "include/util.h"
#include "include/renderman.h"

typedef struct
{
    image_cache_t *cache;
    cache_entry_t *entry;
    item_list_t *list;
    // only for comparison if the deferred action is still valid
    int cacheUID;
    char *value;
} load_image_request_t;

static void cacheClearItem(cache_entry_t *item, int freeTxt);

/* Keep decoded ART bounded while retaining every texture used this frame.
 * Called under the GUI lock after an asynchronous image read completes. */
static void cacheTrim(image_cache_t *cache, cache_entry_t *loaded)
{
    const size_t budget = 8 * 1024 * 1024;
    size_t bytes = 0;
    int i;
    for (i = 0; i < cache->count; i++) {
        GSTEXTURE *texture = &cache->content[i].texture;
        if (texture->Mem)
            bytes += gsKit_texture_size_ee(texture->Width, texture->Height, texture->PSM) + (texture->Clut ? 1024 : 0);
    }
    while (bytes > budget) {
        cache_entry_t *oldest = NULL;
        for (i = 0; i < cache->count; i++) {
            cache_entry_t *entry = &cache->content[i];
            if (entry != loaded && !entry->qr && entry->texture.Mem && entry->lastUsed < guiFrameId &&
                (!oldest || entry->lastUsed < oldest->lastUsed))
                oldest = entry;
        }
        if (!oldest)
            break;
        bytes -= gsKit_texture_size_ee(oldest->texture.Width, oldest->texture.Height, oldest->texture.PSM) + (oldest->texture.Clut ? 1024 : 0);
        cacheClearItem(oldest, 1);
    }
}

// Io handled action...
static void cacheLoadImage(void *data)
{
    load_image_request_t *req = data;

    // Safeguards...
    if (!req || !req->entry || !req->cache)
        return;

    item_list_t *handler = req->list;
    if (!handler)
        return;

    // the cache entry was already reused!
    if (req->cacheUID != req->entry->UID)
        return;

    // seems okay. we can proceed
    GSTEXTURE *texture = &req->entry->texture;
    texFree(texture);

    if (handler->itemGetImage(handler, req->cache->prefix, req->cache->isPrefixRelative, req->value, req->cache->suffix, texture, GS_PSM_CT24) < 0)
        req->entry->lastUsed = 0;
    else
        req->entry->lastUsed = guiFrameId;

    guiLock();
    cacheTrim(req->cache, req->entry);
    req->entry->qr = NULL;
    guiUnlock();

    free(req);
}

void cacheInit()
{
    ioRegisterHandler(IO_CACHE_LOAD_ART, &cacheLoadImage);
}

void cacheEnd()
{
    // nothing to do... others have to destroy the cache via cacheDestroyCache
}

static void cacheClearItem(cache_entry_t *item, int freeTxt)
{
    if (freeTxt) {
        if (item->texture.Mem)
            rmUnloadTexture(&item->texture);
        free(item->texture.Mem);
        free(item->texture.Clut);
    }

    memset(item, 0, sizeof(cache_entry_t));
    item->texture.Mem = NULL;
    item->texture.Vram = 0;
    item->texture.Clut = NULL;
    item->texture.VramClut = 0;
    item->texture.ClutStorageMode = GS_CLUT_STORAGE_CSM1; // Default
    item->qr = NULL;
    item->lastUsed = -1;
    item->UID = 0;
}

image_cache_t *cacheInitCache(int userId, const char *prefix, int isPrefixRelative, const char *suffix, int count)
{
    if (count <= 0 || !suffix)
        return NULL;
    image_cache_t *cache = calloc(1, sizeof(*cache));
    if (!cache)
        return NULL;
    cache->userId = userId;
    cache->count = count;
    cache->prefix = NULL;
    int length;
    if (prefix) {
        length = strlen(prefix) + 1;
        cache->prefix = (char *)malloc(length * sizeof(char));
        if (!cache->prefix) {
            free(cache);
            return NULL;
        }
        memcpy(cache->prefix, prefix, length);
    }
    cache->isPrefixRelative = isPrefixRelative;
    length = strlen(suffix) + 1;
    cache->suffix = (char *)malloc(length * sizeof(char));
    if (!cache->suffix) {
        free(cache->prefix);
        free(cache);
        return NULL;
    }
    memcpy(cache->suffix, suffix, length);
    cache->nextUID = 1;
    cache->content = (cache_entry_t *)malloc(count * sizeof(cache_entry_t));
    if (!cache->content) {
        free(cache->prefix);
        free(cache->suffix);
        free(cache);
        return NULL;
    }

    int i;
    for (i = 0; i < count; ++i)
        cacheClearItem(&cache->content[i], 0);

    return cache;
}

void cacheDestroyCache(image_cache_t *cache)
{
    int i;
    for (i = 0; i < cache->count; ++i) {
        cacheClearItem(&cache->content[i], 1);
    }

    free(cache->prefix);
    free(cache->suffix);
    free(cache->content);
    free(cache);
}

GSTEXTURE *cacheGetTexture(image_cache_t *cache, item_list_t *list, int *cacheId, int *UID, char *value)
{
    if (*cacheId == -2) {
        return NULL;
    } else if (*cacheId != -1) {
        cache_entry_t *entry = &cache->content[*cacheId];
        if (entry->UID == *UID) {
            if (entry->qr)
                return NULL;
            else if (entry->lastUsed == 0) {
                *cacheId = -2;
                return NULL;
            } else {
                entry->lastUsed = guiFrameId;
                return &entry->texture;
            }
        }

        *cacheId = -1;
    }

    // under the cache pre-delay (to avoid filling cache while moving around)
    if (guiInactiveFrames < list->delay)
        return NULL;

    cache_entry_t *currEntry, *oldestEntry = NULL;
    int i, rtime = guiFrameId;

    for (i = 0; i < cache->count; i++) {
        currEntry = &cache->content[i];
        if ((!currEntry->qr) && (currEntry->lastUsed < rtime)) {
            oldestEntry = currEntry;
            rtime = currEntry->lastUsed;
            *cacheId = i;
        }
    }

    if (oldestEntry) {
        load_image_request_t *req = malloc(sizeof(load_image_request_t) + strlen(value) + 1);
        if (!req) {
            *cacheId = -1;
            return NULL;
        }
        req->cache = cache;
        req->entry = oldestEntry;
        req->list = list;
        req->value = (char *)req + sizeof(load_image_request_t);
        strcpy(req->value, value);
        req->cacheUID = cache->nextUID;

        cacheClearItem(oldestEntry, 1);
        oldestEntry->qr = req;
        oldestEntry->UID = cache->nextUID;

        *UID = cache->nextUID++;

        ioPutRequest(IO_CACHE_LOAD_ART, req);
    }

    return NULL;
}
