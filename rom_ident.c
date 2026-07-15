/* ROM identification + dispatch-profile arming -- see rom_ident.h. */

#include "rom_ident.h"
#include "rom_profiles.h"
#include "dispatch_all.h"

/* CRC32 (IEEE 802.3, the No-Intro/zlib polynomial, reflected). The 1 KiB
 * table lives in BSS and is built on first use rather than carried as a
 * flash constant; a full pass over a 2 MiB XIP image costs a few tens of
 * milliseconds once at boot -- noise next to flash init. Matches zlib's
 * crc32() bit-for-bit, so the manifest / registry hashes key directly. */
static uint32_t crc_table[256];
static int crc_table_ready = 0;

static void crc32_build_table(void) {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
    crc_table_ready = 1;
}

static uint32_t crc32_buf(const uint8_t *buf, int len) {
    if (!crc_table_ready) crc32_build_table();
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < len; i++)
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static ff4_rom_ident_t g_ident = FF4_ROM_UNKNOWN;
static uint32_t g_crc = 0;
static const char *g_name = "unknown ROM";

ff4_rom_ident_t ff4_rom_identify(const uint8_t *rom, int length) {
    g_crc = crc32_buf(rom, length);
    g_ident = FF4_ROM_UNKNOWN;
    g_name = "unknown ROM";
    ff4_dispatch_gate_clear();

    for (int p = 0; p < ff4_rom_profile_count; p++) {
        const ff4_rom_profile_t *prof = &ff4_rom_profiles[p];
        if (prof->crc32 != g_crc) continue;
        g_name = prof->name;
        g_ident = prof->is_vanilla ? FF4_ROM_VANILLA : FF4_ROM_KNOWN_VARIANT;
        for (int i = 0; i < prof->gated_count; i++) {
            /* A profile PC absent from the table is stale generator output
             * (table and profiles moved out of sync): fail safe by treating
             * the whole image as unknown rather than running part-gated. */
            if (!ff4_dispatch_gate_pc(prof->gated_pcs[i])) {
                ff4_dispatch_gate_clear();
                g_ident = FF4_ROM_UNKNOWN;
                g_name = "unknown ROM (stale dispatch profile)";
                return g_ident;
            }
        }
        return g_ident;
    }
    return g_ident;
}

ff4_rom_ident_t ff4_rom_ident(void) { return g_ident; }
uint32_t ff4_rom_ident_crc32(void) { return g_crc; }
const char *ff4_rom_ident_name(void) { return g_name; }
