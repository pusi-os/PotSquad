// PusiMesh v2 -- identity and the link->nodeId map. See pusimesh_identity.h.
#include "pusimesh_identity.h"

#if POTS_MESH
#include <string.h>

#if defined(ARDUINO)
#include <Preferences.h>
#endif

namespace PusiMesh2 {

bool identityValid(const uint8_t nodeId[NODE_ID_LEN]) {
    bool zero = true, ones = true;
    for (size_t i = 0; i < NODE_ID_LEN; i++) {
        if (nodeId[i])        zero = false;
        if (nodeId[i] != 0xFF) ones = false;
    }
    return !zero && !ones;
}

void identityGenerate(uint8_t nodeId[NODE_ID_LEN], uint32_t (*rng)()) {
    for (size_t i = 0; i < NODE_ID_LEN; i++) nodeId[i] = (uint8_t)(rng() >> 24);
    // A degenerate draw (a stuck RNG, or a corrupted store we are replacing)
    // gets a fixed, valid escape rather than an id that reads as unset.
    if (!identityValid(nodeId)) {
        nodeId[0] = 0xA5; nodeId[1] = 0x5A;
        for (size_t i = 2; i < NODE_ID_LEN; i++) nodeId[i] = (uint8_t)rng();
    }
}

#if defined(ARDUINO)
bool identityLoad(uint8_t nodeId[NODE_ID_LEN]) {
    Preferences p;
    if (!p.begin("pusimesh", true)) return false;
    uint8_t buf[NODE_ID_LEN] = {};
    const bool have = p.getBytesLength("nodeid") == NODE_ID_LEN;
    const size_t n = have ? p.getBytes("nodeid", buf, NODE_ID_LEN) : 0;
    p.end();
    if (n != NODE_ID_LEN || !identityValid(buf)) return false;
    memcpy(nodeId, buf, NODE_ID_LEN);
    return true;
}
bool identityPersist(const uint8_t nodeId[NODE_ID_LEN]) {
    Preferences p;
    if (!p.begin("pusimesh", false)) return false;
    const size_t n = p.putBytes("nodeid", nodeId, NODE_ID_LEN);
    p.end();
    return n == NODE_ID_LEN;
}
#else
bool identityLoad(uint8_t nodeId[NODE_ID_LEN]) { (void)nodeId; return false; }
bool identityPersist(const uint8_t nodeId[NODE_ID_LEN]) { (void)nodeId; return false; }
#endif

void LinkMap::clear() {
    for (uint8_t i = 0; i < N; i++) e[i].used = false;
}

void LinkMap::note(const uint8_t mac[6], const uint8_t nodeId[NODE_ID_LEN], uint32_t now) {
    int slot = -1, oldest = 0;
    for (uint8_t i = 0; i < N; i++) {
        if (e[i].used && memcmp(e[i].mac, mac, 6) == 0) { slot = i; break; }
        if (!e[i].used && slot < 0) slot = i;
        if (e[i].used && e[i].at < e[oldest].at) oldest = i;
    }
    if (slot < 0) slot = oldest;   // full: the stalest entry makes way
    memcpy(e[slot].mac, mac, 6);
    memcpy(e[slot].nodeId, nodeId, NODE_ID_LEN);
    e[slot].at = now;
    e[slot].used = true;
}

bool LinkMap::resolve(const uint8_t mac[6], uint32_t now, uint32_t ttlMs,
                      uint8_t out[NODE_ID_LEN]) const {
    for (uint8_t i = 0; i < N; i++) {
        if (!e[i].used || memcmp(e[i].mac, mac, 6) != 0) continue;
        if ((uint32_t)(now - e[i].at) > ttlMs) return false;
        memcpy(out, e[i].nodeId, NODE_ID_LEN);
        return true;
    }
    return false;
}

uint8_t LinkMap::prune(uint32_t now, uint32_t ttlMs) {
    uint8_t live = 0;
    for (uint8_t i = 0; i < N; i++) {
        if (e[i].used && (uint32_t)(now - e[i].at) > ttlMs) e[i].used = false;
        if (e[i].used) live++;
    }
    return live;
}

} // namespace PusiMesh2
#endif // POTS_MESH
