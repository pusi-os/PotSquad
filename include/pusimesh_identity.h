// PusiMesh v2 -- the device's stable identity (spec §2) and the map from a
// link address to the nodeId behind it.
//
// Identity is the thing that survives an RPA rotation; the link address is the
// thing that does not. Keeping the two separate, and keying a guest on the
// first, is the whole of Ф1's point: a phone that hides or rotates its address
// stops being a new stranger every advert.
//
// The id arithmetic and the map are pure and host-tested (test/
// pusimesh_identity_test.cpp); only load/persist touch NVS, and only there is
// <Preferences.h> pulled in. Nothing here includes <Arduino.h>.
#pragma once

#if POTS_MESH
#include <stdint.h>
#include <stddef.h>
#include "pusimesh_v2.h"

namespace PusiMesh2 {

// Fills `nodeId` from `rng` (eight draws). The caller supplies the randomness
// -- esp_random() on the radio, a counted stand-in on the host -- so this file
// needs no platform include. The two degenerate ids are refused: eight zero
// bytes and eight 0xFF bytes read as "unset" and "broadcast" to anyone
// skimming a log, and neither is an identity.
void identityGenerate(uint8_t nodeId[NODE_ID_LEN], uint32_t (*rng)());

// True when `nodeId` is a plausible identity (not all-zero, not all-0xFF).
bool identityValid(const uint8_t nodeId[NODE_ID_LEN]);

// NVS `pusimesh/nodeid`. identityLoad leaves `nodeId` untouched and returns
// false when the blob is absent, the wrong length, or degenerate; a caller
// then generates and persists one. On a host build both are stubs, because
// there is no NVS to speak of.
bool identityLoad(uint8_t nodeId[NODE_ID_LEN]);
bool identityPersist(const uint8_t nodeId[NODE_ID_LEN]);

// linkAddr -> nodeId (spec §2). A presence advert says, in effect, "the
// address you just heard belongs to this nodeId"; the same node under a
// rotated address re-points its entry instead of arriving again. Bounded,
// freshest-wins, TTL-pruned.
struct LinkMap {
    static const uint8_t N = 16;
    struct Entry { uint8_t mac[6]; uint8_t nodeId[NODE_ID_LEN]; uint32_t at; bool used; };
    Entry e[N];

    void clear();
    // Records mac -> nodeId at `now`. A mac already mapped is updated in place
    // (the freshest claim wins; the address may have been handed to another
    // node); when the table is full the stalest entry makes way.
    void note(const uint8_t mac[6], const uint8_t nodeId[NODE_ID_LEN], uint32_t now);
    // The nodeId last seen at `mac`, as of `now` and within `ttlMs`. False when
    // there is none, or it has gone stale.
    bool resolve(const uint8_t mac[6], uint32_t now, uint32_t ttlMs,
                 uint8_t out[NODE_ID_LEN]) const;
    // Drops entries last seen more than `ttlMs` ago; returns how many remain.
    uint8_t prune(uint32_t now, uint32_t ttlMs);
};

} // namespace PusiMesh2
#endif // POTS_MESH
