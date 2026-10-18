// PusiMesh v2: identity and the link-address -> nodeId map.
//
// This is the part of Ф1 that has no radio in it: the arithmetic that decides
// whether an id is usable, and the table that keeps a guest the same guest when
// its address changes. The transport half (advertising presence v2, resolving a
// frame's sender) is exercised on the bench, not here.
#include "pusimesh_identity.h"
#include "test_util.h"
#include <cstring>

using namespace PusiMesh2;

static const uint32_t TTL = 20000;

// A deterministic draw, so a failing case reads the same twice.
static uint32_t s_draw = 0;
static uint32_t rng() { return (s_draw += 0x9E3779B9u); }

static bool eq(const uint8_t* a, const uint8_t* b, size_t n) { return memcmp(a, b, n) == 0; }

static void testIdentity() {
    suite("identity");
    uint8_t id[NODE_ID_LEN];
    identityGenerate(id, rng);
    ck("a generated id is valid", identityValid(id));
    bool zero = true, ones = true;
    for (size_t i = 0; i < NODE_ID_LEN; i++) { if (id[i]) zero = false; if (id[i] != 0xFF) ones = false; }
    ck("  and is not all-zero", !zero);
    ck("  and is not all-0xFF", !ones);

    uint8_t a[NODE_ID_LEN], b[NODE_ID_LEN];
    s_draw = 0; identityGenerate(a, rng);
    s_draw = 0; identityGenerate(b, rng);
    ck("the same draws give the same id", eq(a, b, NODE_ID_LEN));

    uint8_t z[NODE_ID_LEN] = {};               std::memset(z, 0x00, NODE_ID_LEN);
    uint8_t o[NODE_ID_LEN];                    std::memset(o, 0xFF, NODE_ID_LEN);
    ck("all-zero is refused", !identityValid(z));
    ck("all-0xFF is refused", !identityValid(o));

    // A stuck RNG must still not mint a degenerate id.
    struct Stuck { static uint32_t z() { return 0; } static uint32_t f() { return 0xFFFFFFFFu; } };
    uint8_t sz[NODE_ID_LEN], sf[NODE_ID_LEN];
    identityGenerate(sz, Stuck::z); ck("a stuck-zero RNG still yields a valid id", identityValid(sz));
    identityGenerate(sf, Stuck::f); ck("a stuck-ones RNG still yields a valid id", identityValid(sf));
    ck("  and the two differ", !eq(sz, sf, NODE_ID_LEN));
}

static void testLinkMap() {
    suite("link -> nodeId map");
    LinkMap m; m.clear();

    const uint8_t macA[6] = { 1, 2, 3, 4, 5, 6 };
    const uint8_t macB[6] = { 9, 9, 9, 9, 9, 9 };
    uint8_t idA[NODE_ID_LEN], idB[NODE_ID_LEN], out[NODE_ID_LEN];
    s_draw = 0x11; identityGenerate(idA, rng);
    s_draw = 0x22; identityGenerate(idB, rng);

    ck("an unknown address resolves to nothing", !m.resolve(macA, 100, TTL, out));
    m.note(macA, idA, 100);
    ck("a noted address resolves", m.resolve(macA, 100, TTL, out));
    ck("  to the node that claimed it", eq(out, idA, NODE_ID_LEN));

    // The freshest claim wins for an address that moved to another node.
    m.note(macA, idB, 200);
    ck("re-noting an address replaces the node", m.resolve(macA, 200, TTL, out) && eq(out, idB, NODE_ID_LEN));

    // TTL: the same entry goes stale.
    ck("a live entry resolves within its ttl", m.resolve(macA, 200 + TTL, TTL, out));
    ck("  and not past it", !m.resolve(macA, 201 + TTL, TTL, out));

    // RPA: the same node under a new address is a second entry, both live.
    m.note(macB, idA, 300);
    uint8_t o1[NODE_ID_LEN], o2[NODE_ID_LEN];
    ck("the node under its new address resolves", m.resolve(macB, 300, TTL, o1) && eq(o1, idA, NODE_ID_LEN));
    ck("and still under the old one", m.resolve(macA, 300, TTL, o2) && eq(o2, idB, NODE_ID_LEN));

    // Prune drops the stale ones only.
    const uint8_t n = m.prune(300 + TTL, TTL);
    ck("prune keeps only the fresh entries", n == 1);
    ck("  and the survivor is the newest", m.resolve(macB, 300 + TTL, TTL, out) && eq(out, idA, NODE_ID_LEN));

    // Filling the table must never lose a mapping to a collision; the stalest
    // makes way and the newest is always present.
    LinkMap full; full.clear();
    uint8_t mac[6] = { 0 };
    for (uint8_t i = 0; i < LinkMap::N; i++) { mac[0] = i; full.note(mac, idA, 1000 + i); }
    mac[0] = 0xFE; full.note(mac, idB, 9999);
    ck("the newest fits a full table", full.resolve(mac, 9999, 1u << 30, out) && eq(out, idB, NODE_ID_LEN));

    full.clear();
    ck("clear empties the table", !full.resolve(mac, 9999, 1u << 30, out));
}

int main() {
    testIdentity();
    testLinkMap();
    return report();
}
