// PotsSquad-CYD — PusiMesh, the half that does not touch a radio.
//
// Who is visiting, what their advert said, when they were last heard, and
// where a message frame goes: all of it is bytes in and state out, so it lives
// here where the emulator can compile it too. The radio half -- NimBLE
// advertising and our own address -- is Mesh::radioTick() in detection.cpp on
// the device and in sim/meshsim.cpp in the emulator.
//
// That split is the point. The emulator's virtual peer feeds real advert bytes
// into the same onManufacturerData() a real scan callback calls, so a visit in
// the emulator goes through the real decoder, the real one-visitor rule and the
// real staleness timeout rather than a copy of them that could drift.
//
// v2 keys all of this on the nodeId a presence advert claims, never on the
// link address. A phone that rotates its address (RPA) -- or a board whose
// address changes even once -- therefore stays the same guest instead of
// arriving again as a stranger. Identity is the thing that survives the
// address; that is the whole of Ф1.
#include "detection.h"

#if POTS_MESH
#include "pusimesh.h"
#include "pusimesh_v2.h"
#include "meshframe_v2.h"
#include "pusimesh_identity.h"
#include "potsy.h"
#include "settings.h"
#include "meshmsg.h"
#include "meshtalk.h"
#include <string.h>
#if defined(ARDUINO)
#include <esp_system.h>
#endif

namespace Mesh {

// How long a peer survives without being heard from again. Adverts go out
// every 1500ms, but a scanner does not hear every one: at the usual scan
// window the watch caught one in three for stretches (a 4.5 s gap, bench
// 2026-09-24), and a wrist over the antenna eats more. At 12 s that walked
// squad members off the watch's screen and straight back on. 20 s is four
// such gaps -- long enough that a pocket, a wall or an arm does not end a
// visit, short enough that somebody who actually left soon stops standing on
// your screen.
static const uint32_t PEER_STALE_MS = 20000;

// ---- who we are -----------------------------------------------------------
// Our stable identity (spec §2): eight random bytes, made once and kept in
// NVS. The link address is the ephemeral handle; this is who we are across it,
// and both the presence advert and every sealed frame are built from it.
static uint8_t s_nodeId[PusiMesh2::NODE_ID_LEN] = { 0 };
static bool    s_nodeIdSet = false;

#if !defined(ARDUINO)
// A tiny xorshift for the emulator and any host build, so identityGenerate has
// a source of chance that is not the radio's.
static uint32_t hostRng() {
    static uint32_t x = 0x2545F491u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}
#endif

const uint8_t* nodeId() {
    if (!s_nodeIdSet) {
        if (!PusiMesh2::identityLoad(s_nodeId)) {
#if defined(ARDUINO)
            PusiMesh2::identityGenerate(s_nodeId, esp_random);
#else
            PusiMesh2::identityGenerate(s_nodeId, hostRng);
#endif
            PusiMesh2::identityPersist(s_nodeId);
        }
        s_nodeIdSet = true;
        Serial.printf("[mesh-id] nodeId %02x%02x%02x%02x%02x%02x%02x%02x\n",
                      s_nodeId[0], s_nodeId[1], s_nodeId[2], s_nodeId[3],
                      s_nodeId[4], s_nodeId[5], s_nodeId[6], s_nodeId[7]);
    }
    return s_nodeId;
}

// ---- who is around: one table, keyed on the nodeId a presence claims -------
static const uint8_t SQUAD_N = 8;
struct Guest {
    uint8_t        nodeId[PusiMesh2::NODE_ID_LEN];
    uint8_t        mac[6];        // its latest link address
    PusiMesh::Peer peer;          // appearance (a PEX name merged in when it comes)
    PusiMesh::Look look;          // clothes + eyes, carried inline by v2 presence
    uint32_t       seen;
    bool           live;
    bool           haveName;      // a PEX name advert arrived for it
};
static Guest s_guest[SQUAD_N];

static Guest* guestByNode(const uint8_t* id) {
    for (uint8_t i = 0; i < SQUAD_N; i++)
        if (s_guest[i].live && memcmp(s_guest[i].nodeId, id, PusiMesh2::NODE_ID_LEN) == 0) return &s_guest[i];
    return nullptr;
}
static Guest* guestByMac(const uint8_t* mac) {
    for (uint8_t i = 0; i < SQUAD_N; i++)
        if (s_guest[i].live && memcmp(s_guest[i].mac, mac, 6) == 0) return &s_guest[i];
    return nullptr;
}
static Guest* guestSlot() {
    Guest* oldest = &s_guest[0];
    for (uint8_t i = 0; i < SQUAD_N; i++) {
        if (!s_guest[i].live) return &s_guest[i];
        if ((int32_t)(s_guest[i].seen - oldest->seen) < 0) oldest = &s_guest[i];
    }
    return oldest;
}

// The visitor: one at a time, decided deliberately and for screen space rather
// than memory. Held by nodeId, so a peer whose address rotates keeps the seat
// instead of being dropped and re-chosen. His mac follows his address.
static uint8_t  s_peerNodeId[PusiMesh2::NODE_ID_LEN] = {0};
static uint8_t  s_peerMac[6] = {0};
static uint32_t s_peerSeen   = 0;
static bool     s_havePeer   = false;

// The visitor somebody picked on the SQUAD screen, if any. RAM only.
static uint8_t  s_preferMac[6] = { 0 };
static bool     s_preferSet    = false;

static const char* peerDisplay(const PusiMesh::Peer& p) {
    return (p.custom && p.name[0]) ? p.name : Potsy::nicknameAt(p.nick);
}

static Guest* noteGuest(const uint8_t* mac, const PusiMesh2::Presence& pr, uint32_t now) {
    Guest* g = guestByNode(pr.nodeId);
    if (!g) {
        g = guestSlot();
        memset(g, 0, sizeof *g);
        memcpy(g->nodeId, pr.nodeId, PusiMesh2::NODE_ID_LEN);
        g->live = true;
        Serial.printf("[squad] %02x%02x%02x%02x arrived\n",
                      pr.nodeId[4], pr.nodeId[5], pr.nodeId[6], pr.nodeId[7]);
    } else if ((int32_t)(now - g->seen) > 4000) {
        Serial.printf("[squad] %02x%02x%02x%02x heard again after %lu ms%s\n",
                      pr.nodeId[4], pr.nodeId[5], pr.nodeId[6], pr.nodeId[7],
                      (unsigned long)(now - g->seen),
                      (int32_t)(now - g->seen) > (int32_t)PEER_STALE_MS ? " (had left)" : "");
    }
    memcpy(g->mac, mac, 6);
    g->seen = now;

    uint8_t nick = 0, outfit = 0, shade = 0; bool custom = false, aura = false;
    PusiMesh::unpackAppearance(pr.appearance, nick, outfit, shade, custom, aura);
    PusiMesh::Peer p{};
    p.nick = nick; p.outfit = outfit; p.shade = shade; p.aura = aura;
    p.phone = pr.phone();
    // The name is not in this advert on BLE -- it rides its own PEX slot. If a
    // PEX name already arrived, keep it; the custom bit rides the appearance
    // word either way, so a name that is still in flight reads as "custom, no
    // name yet" and falls back to the nickname until it lands.
    p.custom = g->haveName;
    if (g->haveName) memcpy(p.name, g->peer.name, sizeof p.name);
    else             p.name[0] = '\0';
    g->peer = p;

    g->look.shirt   = pr.shirt;
    g->look.shorts  = pr.shorts;
    g->look.shoes   = pr.shoes;
    g->look.state   = pr.mood;
    g->look.glasses = pr.glasses;
    g->look.lashes  = pr.lashes;

    // The one-visitor rule, by nodeId.
    const bool chosen = s_preferSet && memcmp(mac, s_preferMac, 6) == 0;
    if (!s_havePeer) {
        s_havePeer = true;
        memcpy(s_peerNodeId, g->nodeId, PusiMesh2::NODE_ID_LEN);
        memcpy(s_peerMac, mac, 6);
    } else if (memcmp(s_peerNodeId, g->nodeId, PusiMesh2::NODE_ID_LEN) == 0) {
        memcpy(s_peerMac, mac, 6);                       // same visitor, new address
    } else if (chosen) {
        memcpy(s_peerNodeId, g->nodeId, PusiMesh2::NODE_ID_LEN);
        memcpy(s_peerMac, mac, 6);
        s_preferSet = false;
    }
    if (s_havePeer && memcmp(s_peerNodeId, g->nodeId, PusiMesh2::NODE_ID_LEN) == 0)
        s_peerSeen = now;
    return g;
}

static void noteGuestName(const uint8_t* nodeIdIn, const char* nm, uint32_t now) {
    Guest* g = guestByNode(nodeIdIn);
    if (!g) return;                       // the presence has not arrived yet
    size_t i = 0;
    for (; i < sizeof g->peer.name - 1 && nm[i]; i++) g->peer.name[i] = nm[i];
    g->peer.name[i] = '\0';
    g->peer.custom = i > 0;
    g->haveName    = i > 0;
    g->seen = now;
}

bool peerLook(const uint8_t mac[6], PusiMesh::Peer& out) {
    Guest* g = guestByMac(mac);
    if (!g) return false;
    out = g->peer;
    return true;
}

bool lookFor(const uint8_t mac[6], PusiMesh::Look& out) {
    const uint32_t now = millis();
    Guest* g = guestByMac(mac);
    if (!g || (int32_t)(now - g->seen) > (int32_t)PEER_STALE_MS) return false;
    out = g->look;
    return true;
}

void preferPeer(const uint8_t mac[6]) {
    memcpy(s_preferMac, mac, 6);
    s_preferSet = true;
}

uint8_t squadList(uint32_t now, SquadMember* out, uint8_t cap) {
    if (!Settings::meshDetect()) return 0;
    uint8_t n = 0;
    for (uint8_t i = 0; i < SQUAD_N && n < cap; i++) {
        if (!s_guest[i].live || (int32_t)(now - s_guest[i].seen) > (int32_t)PEER_STALE_MS) continue;
        memcpy(out[n].mac, s_guest[i].mac, 6);
        out[n].peer = s_guest[i].peer;
        out[n].seen = s_guest[i].seen;
        n++;
    }
    // Insertion sort by address: eight at most, and a stable order is what
    // keeps the carousel from shuffling as adverts arrive in a new order.
    for (uint8_t i = 1; i < n; i++)
        for (uint8_t j = i; j > 0 && memcmp(out[j - 1].mac, out[j].mac, 6) > 0; j--) {
            SquadMember t = out[j]; out[j] = out[j - 1]; out[j - 1] = t;
        }
    return n;
}

uint8_t squadCount(uint32_t now) {
    if (!Settings::meshDetect()) return 0;
    uint8_t n = 0;
    // Signed: the BLE task's millis() can be a tick ahead of the loop's.
    for (uint8_t i = 0; i < SQUAD_N; i++)
        if (s_guest[i].live && (int32_t)(now - s_guest[i].seen) <= (int32_t)PEER_STALE_MS) n++;
    return n;
}

const PusiMesh::Peer* peer() {
    if (!s_havePeer) return nullptr;
    Guest* g = guestByNode(s_peerNodeId);
    return g ? &g->peer : nullptr;
}
const uint8_t* peerMac() { return s_peerMac; }

// ---- what we look like, read fresh each advert ----------------------------
static PusiMesh2::Presence selfPresence(bool withName) {
    PusiMesh2::Presence p{};
    const char* cn = Potsy::customName();
    const bool custom = (cn && cn[0]);
    memcpy(p.nodeId, nodeId(), PusiMesh2::NODE_ID_LEN);
    // squadTag is filled by the relay membership layer (Ф3); presence is not
    // authenticated by it, so zeroes are honest here, not a placeholder lie.
    p.appearance = PusiMesh::packAppearance(Potsy::nicknameIndex(), Potsy::outfitIndex(),
                                            Potsy::shadesIndex(), custom,
                                            Potsy::hasAura() && Settings::auraShown());
    p.caps   = PusiMesh2::CAP_EXT;
    p.shirt  = Potsy::shirtIndex();
    p.shorts = Potsy::shortsIndex();
    p.shoes  = Potsy::shoesIndex();
    p.glasses = Potsy::glassesOn();
    p.lashes  = Potsy::lashesOn();
    p.mood    = Potsy::moodIndex();
    if (custom && withName) {
        p.flags |= PusiMesh2::FLAG_NAME;
        size_t i = 0;
        for (; i < PusiMesh2::NAME_BYTES && cn[i]; i++) p.name[i] = cn[i];
        p.name[i] = '\0';
        p.nameLen = (uint8_t)i;
    }
    return p;
}

// The short (unnamed) presence body: identity, appearance and clothes, 23
// bytes. This is what a BLE advert carries -- it fits 31 with the company ID
// and an AD header (2 + 23 + 2 = 27), leaving the name to its own PEX slot.
size_t buildSelfShort(uint8_t* out) {
    const PusiMesh2::Presence p = selfPresence(false);
    return PusiMesh2::encodePresence(p, out, PusiMesh2::BODY_NAMED);
}

// The named body (spec §3), used where there is room for it -- ESP-NOW's 250
// bytes. Byte-identical to the short form when there is no custom name.
size_t buildSelf(uint8_t* out) {
    const PusiMesh2::Presence p = selfPresence(true);
    return PusiMesh2::encodePresence(p, out, PusiMesh2::BODY_NAMED);
}

// The PEX name advert body, for a BLE slot of its own. 0 when there is no
// custom name to send.
size_t buildSelfNameAd(uint8_t* out) {
    const char* cn = Potsy::customName();
    if (!cn || !cn[0]) return 0;
    return PusiMesh2::encodeNameAd(nodeId(), PusiMesh2::FLAG_NAME, cn, out, PusiMesh2::BODY_NAMED);
}

void begin() { s_havePeer = false; }

bool onManufacturerData(const uint8_t* d, size_t len, const uint8_t* mac, uint32_t now) {
    // Refused at the door when detection is off, so a peer cannot be latched
    // between the setting changing and the next tick.
    if (!Settings::meshDetect()) return false;
    if (len < 2) return false;
    const uint16_t cid = (uint16_t)(d[0] | ((uint16_t)d[1] << 8));
    if (cid != PusiMesh2::COMPANY_ID) return false;
    const uint8_t* body = d + 2;
    const size_t   blen = len - 2;

    // Presence v2, either length.
    PusiMesh2::Presence pr;
    if (PusiMesh2::decodePresence(body, blen, pr)) { noteGuest(mac, pr, now); return true; }

    // The PEX name advert, for the custom name the presence slot could not hold.
    {
        uint8_t nid[PusiMesh2::NODE_ID_LEN], flags = 0;
        char nm[PusiMesh2::NAME_BYTES + 1];
        if (PusiMesh2::decodeNameAd(body, blen, nid, flags, nm)) { noteGuestName(nid, nm, now); return true; }
    }

    // A sealed message frame. Its sender's identity is the nodeId resolved from
    // the address it came from, never the address itself: that is what lets a
    // phone with a rotating address be read. Unknown sender -- the presence has
    // not landed yet -- falls back to the address, and the frame repeats.
    // v2 frames ('PU') and the legacy 'ST' ones both walk this road; the magic
    // tells them apart and only the frame runtimes read them.
    if (PusiMesh2::isFrame(body, blen) || MeshMsg::isFrame(body, blen)) {
        if (!Settings::messagesOn()) return false;
        Guest* g = guestByMac(mac);
        uint8_t sender[6];
        if (g) memcpy(sender, g->nodeId, 6);
        else   memcpy(sender, mac, 6);
        const char* nm = g ? peerDisplay(g->peer) : "";
        MeshTalk::onFrame(mac, body, blen, nm, sender);
        return true;
    }
    return false;
}

void tick(uint32_t now) {
    radioTick(now);

    // Detecting is its own switch now, and off means no visitor at all --
    // not "advertise less". A peer already on screen is dropped rather than
    // frozen there.
    if (!Settings::meshDetect()) { s_havePeer = false; return; }

    // SIGNED. s_peerSeen is stamped in the BLE task with its own millis(),
    // which can be later than this loop pass's `now`; unsigned, that age was
    // minus a few ms, read as 49 days, and the visitor was dropped the
    // instant it was heard -- then heard again: the watch's guest phasing in
    // and out. The watch loops ten times a second with its screen off, so it
    // hit this far more often than the CYDs. The squad list was already
    // signed, which is why the member count never flickered.
    if (s_havePeer && (int32_t)(now - s_peerSeen) > (int32_t)PEER_STALE_MS) s_havePeer = false;
}

} // namespace Mesh
#endif // POTS_MESH
