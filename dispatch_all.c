#include "dispatch_all.h"

/* Stub for ExecSound_ext ($04:8003, real entry point per ROM bytes is
 * $04:8004): the title routine ShowTitle ($00:85FA) calls `jsl
 * ExecSound_ext` as its 8th instruction; the real sound engine (jsr
 * ExecSound) spins waiting for an SPC handshake that never completes on
 * G&W, so ShowTitle never returns and never loads the logo tilemap. No-op
 * here so the dispatch JSL hook simulates RTL and ShowTitle proceeds. This
 * silences all music/SFX (acceptable for now); the proper fix is a working
 * SPC responder. This table is hand-maintained (see dispatch_all.h) — this
 * stub and its table entry are edited here directly, not generated.
 *
 * FIX (2026-07-06): a plain no-op is not safe for every caller of this same
 * entry point. $13:FF12-FF39 ("send a sound command and wait for the SPC
 * to acknowledge it") does:
 *   STA $A9          ; save command id
 *   loop: LDA $A9 / STA $1E01 ; LDA #$01 / STA $1E00 ; JSL $048004
 *         LDA $1E05 / CMP $A9 / BNE loop   ; wait for ack in $1E05
 *         LDA $1E04 / CMP #$01 / BNE loop  ; wait for a second ack byte
 *         RTL
 * A pure no-op never updates $1E04/$1E05, so this caller spins forever --
 * found reproducing a "combat animation ends on a black screen that never
 * progresses" bug reported by Hoani (triggered by a random encounter near
 * Baron castle; see MemPalace wing=ff4-gnw room=obstacles-and-solutions).
 * The title screen's own call site (ShowTitle) never polls $1E04/$1E05, so
 * it's unaffected by also writing them here. Fake the acknowledgment the
 * real SPC would eventually send: mirror the command id into $1E05 and set
 * $1E04=1, satisfying any caller using this same handshake pattern, while
 * still not actually running any sound (same accepted trade-off as above --
 * silence, not a real SPC responder). Zero-page ($A9) is read via cpu->dp,
 * not assumed absolute -- same reasoning as the CheckMenu_c/InitMapRAM_c DP
 * fixes elsewhere in this project (see CONVENTIONS.md's field-module DP
 * note); this specific call site's real DP was not independently confirmed,
 * so this defaults safely to whatever the caller's actual DP is. */
static void ExecSound_ext_stub(Snes *snes) {
    uint16_t dp = snes->cpu->dp;
    uint8_t cmd_id = snes->ram[(uint16_t)(dp + 0xA9)];
    snes->ram[0x1E05] = cmd_id;
    snes->ram[0x1E04] = 0x01;
}

/* CheckMenu ($00:81F4) REMOVED from the dispatch on 2026-07-06 (204->203):
 * same class of problem as ExecBtlGfx ($03:8085, removed 2026-06-30).
 * CheckMenu_c's own translated body is a faithful, previously-DP-bug-fixed
 * translation, but it calls fade_out_menu_emu / main_menu_emu (MainMenu_ext)
 * / fade_in_menu_emu / exec_event_emu as plain C helper delegations — and
 * MainMenu_ext is an INTERACTIVE, multi-frame routine (reads input and
 * redraws across many real frames while the menu is open), which cannot be
 * run synchronously inside a single dispatched C call the way a short
 * leaf routine can (same category as WaitVblank-driven routines: running
 * it to completion inside one dispatch hook would need real vblank/NMI
 * progression that a synchronous call can't provide). The 4 helpers were
 * left as permanent no-op stubs rather than attempted delegations, which
 * meant the whole menu subsystem silently never ran in native mode: no
 * menu ever appeared, dispatch or not (see MemPalace wing=ff4-gnw
 * room=obstacles-and-solutions, "[CHANTIER INPUT/MENU]" for the original
 * 2026-06-30 diagnosis, and the ReadCtrl/UpdateCtrl fix from earlier the
 * same day as this removal for the companion navigation-side bug).
 * Verified via the desktop headless harness's --exclude flag: excluding
 * ONLY $0081F4 (interpreting CheckMenu, everything else stays native,
 * including the ReadCtrl/UpdateCtrl/InitCtrl/ResetSprites fixes) is
 * sufficient for the menu to open AND navigate correctly — confirming
 * CheckMenu itself (not any of its siblings) is the right, minimal thing
 * to retire. Registry: D0081F4 L1 -> RETIRED (was already carrying a
 * known 1/200 spike divergence pre-dating this session). */

const ff4_dispatch_entry_t ff4_dispatch_table[FF4_DISPATCH_COUNT] = {
    { 0x00808e, AfterBattle_c },  /* field */
    { 0x0080a0, FieldMain_c },  /* field */
    { 0x008302, UpdatePlayerSpeed_c },  /* field */
    { 0x00834e, InitMapRAM_c },  /* field */
    { 0x00883d, _00883d_c },  /* field */
    { 0x00885e, _00885e_c },  /* field */
    { 0x00aa58, CheckTilePass_c },  /* field */
    { 0x00aad8, SetPlayerNPCMap_c },  /* field */
    { 0x00ab13, ClearPlayerNPCMap_c },  /* field */
    { 0x00ac7d, CheckVehicleBlock_c },  /* field */
    { 0x00be47, CalcVehicleSpritePos_c },  /* field */
    { 0x00c0c4, PlayerSpriteTiles_c },  /* field */
    { 0x00c3bd, UpdateWhalePal_c },  /* field */
    { 0x00cb5f, TfrBGAnimGfx_c },  /* field */
    { 0x00f535, UpdateBG2ScrollSkip_c },  /* field — bank $00's real (sole) entry point; loads
                                            the $C9 guard itself. The $00:f533 entry that used
                                            to sit above this one was retired 2026-07-05: the
                                            disassembly had a 2-byte offset error at this exact
                                            spot ($00:f533 is not an instruction boundary, it's
                                            the operand byte of the PRECEDING routine's LDX $43)
                                            and was never a real call target. See D00F533/D00F535
                                            in registry/dispatch_state.jsonl and MemPalace
                                            wing=ff4-gnw room=obstacles-and-solutions for the
                                            full finding. */
    { 0x00ffbc, InitCharProp_ext_c },  /* field */
    { 0x00ffe0, Vectors_c },  /* field */
    { 0x018010, UpdateCtrlField_ext_c },  /* menu */
    { 0x01ca85, TfrVRAM_c },  /* field */
    { 0x01d718, FadeIn_c },  /* field */
    { 0x01dfd2, LoadBattleSpeedPosText_c },  /* menu */
    /* bank $02 — combat graphics (btlgfx) primitives */
    { 0x028560, Mult8_btlgfx_c },         /* btlgfx — 8x8 shift-add multiply */
    { 0x0285d2, HardMult_btlgfx_c },      /* btlgfx — hardware 8x8 multiply ($4202/$4216) */
    { 0x0290a0, TfrBG2MenuTile_c },          /* btlgfx — BG2 menu tile DMA (atomic Mult8+read, race fix) */
    { 0x02a491, IncrTextPtr_c },           /* btlgfx — increment 16-bit text ptr at $30 */
    { 0x02bb0b, BackAttackYOffset_s_c },   /* btlgfx — Y offset adjust, back-attack (−8) */
    { 0x02bb1a, BackAttackYOffset_l_c },   /* btlgfx — Y offset adjust, back-attack (−16) */
    { 0x02da73, DrawMonsterSprite_c },      /* btlgfx — copy 32 sprite bytes from ROM $1C:FD00 to OAM $ED50 */
    { 0x02dafe, InitMonsterAnim_c },       /* btlgfx — initialise/re-arm animation control for monster slot */
    { 0x02dced, BuildOAMEntries_c },       /* btlgfx — write 2-tile OAM entries for combat object slot */
    { 0x02dda5, CheckSpriteVisible_c },    /* btlgfx — check monster sprite visibility (C=0 visible) */
    { 0x02dddc, UpdateMonsterAnim_c },     /* btlgfx — per-frame animation state machine for monster slot */
    { 0x038009, ExecBattle_c },  /* battle */
    { 0x03805f, DrawMP_c },  /* battle */
    /* 0x038085 ExecBtlGfx — REMOVED from dispatch (EXCL). BLOCKING animation: its body
     * (btlgfx.asm:50 + cmd_anim/magic/monster_death) chains jsr WaitVblank/WaitFrame across
     * multiple frames. ExecBtlGfx_c delegated to it via run_emulated_func (synchronous, single-frame)
     * → the NMI doesn't advance during the sub-execution → WaitVblank loops → 50M-opcode guard trips
     * (~1s freeze) → animation state corrupted → battle ends prematurely (monsters vanish,
     * victory pose). Must stay INTERPRETED in the normal flow. Category WaitVblank=EXCL. */
    { 0x0382cb, InitHWRegs_c },  /* field */
    { 0x038379, RandXA_c },  /* battle */
    { 0x0383b9, Mult16_c },  /* battle */
    { 0x0383e0, Mult8_c },  /* battle */
    { 0x038407, Div16_c },  /* battle */
    { 0x0384e3, Add16_c },  /* battle */
    { 0x0384fc, Sub16_c },  /* battle */
    { 0x03c99f, CalcDmg_c },  /* battle -- physical/elemental damage formula, see
                                battle/CalcDmg.c. Was reached only via the calc_dmg_emu
                                no-op stub before 2026-07-06 (KNOWN_FINDINGS.md F1-adjacent) --
                                the whole damage formula (variance/mitigation/9999 cap)
                                silently never ran. */
    { 0x038579, RandMonster_c },  /* battle */
    { 0x03858b, Rand99_c },  /* battle */
    { 0x03859b, AddMsg1_c },  /* battle */
    { 0x0385a6, AddMsg2_c },  /* battle */
    { 0x0385b1, AddMsg3_c },  /* battle */
    { 0x0387d8, CheckFanfare_c },  /* battle */
    { 0x0387e4, CheckBattleList_c },  /* battle */
    { 0x038803, CheckWinAnim_c },  /* battle */
    { 0x0395ce, InitCharRows_c },  /* battle */
    { 0x039741, GetPendingAction_c },  /* battle */
    { 0x039788, CheckTimer_c },  /* battle */
    { 0x0397b3, InitAction_c },  /* battle */
    { 0x039e65, TimerDur_00_c },  /* battle — ATB timer duration (F11) */
    { 0x039e71, TimerDur_02_c },  /* battle — ATB timer duration (F11) */
    { 0x039f1c, TimerDur_08_c },  /* battle — ATB timer duration (F11) */
    { 0x039f75, TimerDur_0a_c },  /* battle — ATB timer duration (F11) */
    /* TimerDur_0b/03 deferred: ROM access (bank $0F) not ported in C; TimerDur_07 non-standard signature */
    { 0x039fd8, ApplySpeedMod_c },  /* battle */
    /* 0x03b0ff ExecCmd omitted — tail-jump via jml [$0080], not a JSR/JSL; must run in interpreter */
    { 0x03b33f, Cmd_21_c },  /* battle */
    { 0x03ba04, AITarget_1e_c },  /* battle */
    { 0x03ba67, AITarget_1f_c },  /* battle */
    { 0x03ba74, AITarget_20_c },  /* battle */
    { 0x03ba81, AITarget_21_c },  /* battle */
    { 0x03ba8e, AITarget_22_c },  /* battle */
    { 0x03baef, AITarget_23_c },  /* battle */
    { 0x03bafe, AITarget_24_c },  /* battle */
    { 0x03bb0d, AITarget_25_c },  /* battle */
    { 0x03bb6b, AITarget_29_c },  /* battle */
    { 0x03bda9, AICond_02_c },  /* battle */
    { 0x03be1e, AICond_05_c },  /* battle */
    { 0x03be31, AICond_06_c },  /* battle */
    { 0x03bee4, AICond_09_c },  /* battle */
    { 0x03bf05, AICond_0b_c },  /* battle */
    { 0x03bff6, AICondTarget_25_c },  /* battle */
    { 0x03c03b, AICondTarget_26_c },  /* battle */
    { 0x03c042, AICondTarget_27_c },  /* battle */
    { 0x03c049, AnyTarget_c },  /* battle */
    { 0x03c06c, AICondTarget_23_c },  /* battle */
    { 0x03c987, CalcHits_c },  /* battle */
    { 0x03d378, MagicDmgEffect_c },  /* battle */
    { 0x03d466, MagicEffect_04_c },  /* battle */
    { 0x03d613, MagicEffect_08_c },  /* battle */
    { 0x03d83f, MagicEffect_0d_c },  /* battle */
    { 0x03d863, MagicEffect_0e_c },  /* battle */
    { 0x03d972, MagicEffect_13_c },  /* battle */
    { 0x03d9ec, MagicEffect_15_c },  /* battle */
    { 0x03da0c, MagicEffect_16_c },  /* battle */
    { 0x03dc9b, MagicEffect_20_c },  /* battle */
    { 0x03dcbb, MagicEffect_21_c },  /* battle */
    { 0x03dcd6, MagicEffect_22_c },  /* battle */
    { 0x03dd06, MagicEffect_24_c },  /* battle */
    { 0x03dd65, MagicEffect_2c_c },  /* battle */
    { 0x03dd95, MagicEffect_28_c },  /* battle */
    { 0x03ddb1, MagicEffect_2a_c },  /* battle */
    { 0x03ddc9, MagicEffect_2b_c },  /* battle */
    { 0x03dddd, MagicEffect_2f_c },  /* battle */
    { 0x03dfd2, MagicEffect_32_c },  /* battle */
    { 0x03e030, RemoveTarget_c },  /* battle */
    { 0x03e100, CheckStrongElem_c },  /* battle */
    { 0x03e133, CheckWeakElem_c },  /* battle */
    { 0x03e43b, Cmd_22_c },  /* battle */
    { 0x03e4d9, TwinFailed_c },  /* battle */
    /* Cmd_0f/0e/0c/08/01 removed: their _emu helpers (do_magic_attack_emu,
     * do_fight_cmd_emu, do_multi_attack_emu) are no-op weak stubs — all damage
     * was silently swallowed.  Run in pure interpreter until helpers are ported. */
    { 0x03fe03, TfrSprites_c },  /* field */
    { 0x048004, ExecSound_ext_stub },  /* sound: stub to unblock title (SPC wait); jsl target per ROM bytes at $00:860D */
    { 0x0485e1, PlayGameSfx_c },  /* sound */
    { 0x04861e, ExecInterrupt_c },  /* sound */
    { 0x088690, LoadTitleGfx_c },  /* field */
    { 0x08885e, TfrTitleCrystalTiles_c },  /* field */
    { 0x0e8b3c, CheckBattle_c },  /* field */
    { 0x12e35b, WaitVblankEvent_c },  /* field */
    { 0x12e55a, FindEventTerminator_c },  /* field */
    { 0x12e613, EventCmd_d8_c },  /* field */
    { 0x12e7d3, EventCmd_d7_c },  /* field */
    { 0x12eb47, EventCmd_e0_c },  /* field */
    { 0x12ec06, EventCmd_e6_c },  /* field */
    { 0x12ec3e, EventCmd_dd_c },  /* field */
    { 0x12ed1d, SetCurrGil_c },  /* field */
    { 0x12ee1c, EventCmd_d0_c },  /* field */
    { 0x12ee25, EventCmd_d1_c },  /* field */
    { 0x12ee35, TfrInvertPal_c },  /* field */
    { 0x13bfe3, _00bfe3_c },  /* field */
    { 0x13c11f, ReloadNPCs_c },  /* field */
    { 0x13d730, LoadTheEndGfx_c },  /* cutscene */
    { 0x13db10, _13db10_c },  /* cutscene */
    { 0x13db23, _13db23_c },  /* cutscene */
    { 0x13e07d, _13e07d_c },  /* cutscene */
    { 0x13e139, GetEarthSpritePos_c },  /* cutscene */
    { 0x13e247, InitStars_c },  /* cutscene */
    { 0x13e2dc, GetOtherPlanetTile_c },  /* cutscene */
    { 0x13e512, Mult16_1_c },  /* cutscene */
    { 0x13eb24, NewLine_c },  /* cutscene */
    { 0x13eb60, _13eb60_c },  /* cutscene */
    { 0x13ebb8, _13ebb8_c },  /* cutscene */
    { 0x13ef4c, _13ef4c_c },  /* cutscene */
    { 0x13fe36, AutoBattle_0003_c },  /* battle */
    { 0x14f58e, _14f58e_c },  /* field */
    { 0x14f626, GilWindowTiles3_c },  /* field */
    { 0x14f63e, GilWindowTiles4_c },  /* field */
    { 0x14f6d6, DlgTilesTop_c },  /* field */
    { 0x14f796, MapTitleTilesTop_c },  /* field */
    { 0x14f7b6, MapTitleTilesBtm_c },  /* field */
    { 0x14fa16, LavaAnimPal_c },  /* field */
    { 0x14fb1e, WipeScanlineTbl_c },  /* field */
    { 0x14fd00, InitCtrl_ext2_c },  /* menu */
    { 0x14fd03, UpdateCtrl_ext_c },  /* menu */
    { 0x14fd06, ClearText_ext_c },  /* menu */
    { 0x14fd09, UpdateWindowColor_ext_c },  /* menu */
    { 0x14fd0c, UpdateScrollRegs_ext_c },  /* menu */
    { 0x1585ab, InitWorld_c },  /* field */
    { 0x1589ed, InitInterrupts_c },  /* field */
    { 0x158b2a, InitDMA_c },  /* field */
    { 0x158d5d, PlayMapSong_c },  /* field */
    { 0x158dfc, WaitKeyUp_c },  /* field */
    { 0x158e05, WaitKeyDown_c },  /* field */
    { 0x158e47, UpdateWaterLavaAnim_c },  /* field */
    { 0x158e57, TfrWaterLavaGfx_c },  /* field */
    { 0x158f34, TfrLavaGfx_c },  /* field */
    { 0x159104, UpdateMode7Regs_c },  /* field */
    { 0x1591ca, UpdateWipeIRQ_c },  /* field */
    { 0x159204, UpdateWipeNMI_c },  /* field */
    { 0x159792, LoadPlayerGfxWorld_c },  /* field */
    { 0x1597a2, LoadPlayerGfxSub_c },  /* field */
    { 0x1599fb, GiveGil_c },  /* field */
    { 0x159ae9, GetTreasureTiles_c },  /* field */
    { 0x159b5b, GetTreasurePtr_c },  /* field */
    { 0x15af24, CloseYesNoWindow_c },  /* field */
    { 0x15b09c, ScrollItemListDown_c },  /* field */
    { 0x15b143, TfrBGGfx_c },  /* field */
    { 0x15b3dc, _15b3dc_c },  /* field */
    { 0x15b41b, GetDlgPtr1H_c },  /* field */
    { 0x15b6f1, InitDlgIRQ_c },  /* field */
    { 0x15b8c9, _15b8c9_c },  /* field */
    { 0x15bb6a, _15bb6a_c },  /* field */
    { 0x15c144, _15c144_c },  /* field */
    { 0x15c163, _15c163_c },  /* field */
    { 0x15c23d, _15c23d_c },  /* field */
    { 0x15c37f, Pow10Hi_c },  /* field */
    { 0x15ca5e, _15ca5e_c },  /* field */
    { 0x15ca85, _15ca85_c },  /* field */
    { 0x15cadc, _15cadc_c },  /* field */
    { 0x16c59a, AfterCutscene_c },  /* field */
    { 0x16c8bc, Special_2d_c },  /* field */
    { 0x16cb05, _00cb05_c },  /* field */
    { 0x16cb72, _00cb72_c },  /* field */
    { 0x16cfc4, Special_1d_c },  /* field */
    { 0x16cfd0, Special_1c_c },  /* field */
    { 0x16d263, LoadOverworldLeviathan_c },  /* field */
    { 0x16d342, Special_0d_c },  /* field */
    { 0x16d36d, LoadMapStack_c },  /* field */
    { 0x16d758, DrawDestroyedDamcyan_c },  /* field */
    { 0x16d831, Special_1e_c },  /* field */
    { 0x16d9d6, Special_06_c },  /* field */
    { 0x16db71, DrawRedWings_c },  /* field */
    { 0x16dbbe, IncBrightness_c },  /* field */
    { 0x16dbd2, LoadOverworldIntro_c },  /* field */
    { 0x16de1b, _00de1b_c },  /* field */
    { 0x16df53, _00df53_c },  /* field */
    { 0x16f533, UpdateBG2Scroll_c },  /* field */
    { 0x16f922, _00f922_c },  /* field */
    { 0x16fb93, TfrBG2Tilemap_c },  /* field */
    { 0x16ffab, DecodeBG1Tilemap_c },  /* field */
    { 0x1e9f6c, UpdateLocalTiles_c },  /* field */
    { 0x1ea03e, BoardChoco_c },  /* field */
};

uint32_t ff4_dispatch_hits = 0;
uint32_t ff4_dispatch_misses = 0;
uint32_t ff4_miss_per_bank[256] = {0};

/* Runtime A/B toggle (desktop validation host, ADR 0001). Default 1 = native
 * dispatch active, exactly as the device runs. The desktop oracle sets it to 0
 * to fall back to pure interpretation (ground truth) on the same binary/state.
 * The device never clears it, so on-device behaviour is unchanged. */
int ff4_dispatch_enabled = 1;

/* Optional per-hit trace hook (desktop A/B oracle, ADR 0001 / M3). NULL on the
 * device, so the only on-device cost is one predictably-not-taken NULL check.
 * The oracle installs a callback to record which original PC was dispatched on
 * each hit, so the first diverging frame can be attributed to a concrete hook. */
void (*ff4_dispatch_trace)(uint32_t pc) = 0;

/* Optional per-hook filter (desktop A/B oracle, ADR 0001 / M3). NULL on device.
 * When set, a matched hook runs natively only if the filter returns non-zero;
 * otherwise the call falls through to pure interpretation. Lets the oracle
 * exclude intentionally-divergent routines (e.g. the _15cadc OAM-DMA bypass)
 * so they stop contaminating the comparison and the remaining faithful ports
 * can be checked against ground truth. */
int (*ff4_dispatch_filter)(uint32_t pc) = 0;

/* Optional per-miss trace hook (desktop miss profiler). NULL on device.
 * Called on every interpreter fall-through so a harness can tally miss PCs
 * and produce the hot-miss list for the next porting sprint. */
void (*ff4_dispatch_miss_trace)(uint32_t pc) = 0;

/* Cycle-accounting for the desktop A/B oracle (ADR 0001 / oracle hardening).
 *
 * A dispatched C body runs in ~0 SNES cycles (only the JSR/JSL prologue + the
 * simulated RTS/RTL are charged), whereas the original asm routine it replaces
 * consumed real cycles. Left uncharged, the native pass races ahead of the
 * interpreter pass and the two drift in PPU phase — the "structural
 * execution-speed artefact" that produces false-positive CRC/WRAM divergences.
 *
 * When ff4_dispatch_charge_cycles is set (oracle only — 0 on device, so device
 * timing is unchanged), ff4_dispatch_try charges the difference between the
 * routine's measured interpreter cost (ff4_dispatch_cycle_cost[i], populated by
 * the calibration pass) and the cycles the C body actually consumed this call.
 * That single rule self-corrects all three body shapes:
 *   - pure C body (0 real cycles)             -> charge the full measured cost
 *   - self-accounting body (snes_runCycles)   -> ~0 left to charge
 *   - delegating body (run_emulated_func)     -> charge only the remainder
 * Indexed by dispatch-table slot (tracks ff4_dispatch_table order). */
int ff4_dispatch_charge_cycles = 0;
int32_t ff4_dispatch_cycle_cost[FF4_DISPATCH_COUNT] = {0};
const int ff4_dispatch_count = FF4_DISPATCH_COUNT;  /* table size, for the oracle */

/* Calibration of ff4_dispatch_cycle_cost[] (oracle only; 0 on device).
 *
 * Run with ff4_dispatch_measure set and dispatch DISABLED (pure interpreter):
 * ff4_dispatch_measure_enter pushes a frame when a JSR/JSL targets a dispatch
 * entry, recording the in-routine entry SP and cycle count; the interpreter then
 * runs the real asm routine. On each RTS/RTL, ff4_dispatch_measure_return closes
 * every frame whose routine has returned (SP risen back above its entry level)
 * and records the routine's cycle cost (max over occurrences — the worst case is
 * the safe charge). Stack grows down, so the most recent (deepest) frame has the
 * smallest SP and returns first. */
int ff4_dispatch_measure = 0;
static struct { int slot; uint64_t cyc; uint16_t sp; uint32_t frame; } ff4_meas_stack[64];
static int ff4_meas_top = 0;

static void ff4_dispatch_measure_enter(Snes *snes, uint32_t pc) {
    for (int i = 0; i < FF4_DISPATCH_COUNT; i++) {
        if (ff4_dispatch_table[i].pc == pc) {
            if (ff4_meas_top < 64) {
                ff4_meas_stack[ff4_meas_top].slot  = i;
                ff4_meas_stack[ff4_meas_top].cyc   = snes->cycles;
                ff4_meas_stack[ff4_meas_top].sp    = snes->cpu->sp;
                ff4_meas_stack[ff4_meas_top].frame = snes->frames;
                ff4_meas_top++;
            }
            return;
        }
    }
}

void ff4_dispatch_measure_return(Snes *snes) {
    if (!ff4_dispatch_measure) return;
    /* A frame's routine has returned once SP has risen strictly above the SP it
     * had just inside the routine (the original JSR/JSL return frame popped). */
    while (ff4_meas_top > 0 && snes->cpu->sp > ff4_meas_stack[ff4_meas_top - 1].sp) {
        int slot = ff4_meas_stack[ff4_meas_top - 1].slot;
        /* Only record routines that completed WITHIN a single frame. A routine
         * that crossed a vblank (WaitVblank-driven, NMI re-entry, or a missed
         * RTS/RTL via a JML tail-jump that the SP watermark closes much later)
         * has a variable, unbounded "cost" that must NOT be charged as a fixed
         * value — doing so injects cycle drift. Such routines self-regulate
         * through the interpreter anyway (their sub-calls charge real cycles). */
        if (snes->frames == ff4_meas_stack[ff4_meas_top - 1].frame) {
            int32_t cost = (int32_t)(snes->cycles - ff4_meas_stack[ff4_meas_top - 1].cyc);
            if (cost > ff4_dispatch_cycle_cost[slot]) ff4_dispatch_cycle_cost[slot] = cost;
        }
        ff4_meas_top--;
    }
}

/* Reset calibration state between oracle passes. */
void ff4_dispatch_measure_reset(void) {
    ff4_meas_top = 0;
    for (int i = 0; i < FF4_DISPATCH_COUNT; i++) ff4_dispatch_cycle_cost[i] = 0;
}

#ifdef FF4_AUTOBOOT
uint32_t g_diag_miss_ring[8] = {0};
static uint8_t g_diag_miss_ring_head = 0;
#endif

int ff4_dispatch_try(Snes *snes, uint32_t pc) {
    /* Calibration runs with dispatch disabled, so measure BEFORE the early-out:
     * the real asm routine is about to run in the interpreter and we time it. */
    if (ff4_dispatch_measure) ff4_dispatch_measure_enter(snes, pc);
    if (!ff4_dispatch_enabled) return 0;  /* pure-interpreter side of the A/B */
    /* Linear scan (binary search was unreliable: gen_dispatch.py sorts by the
     * original pc, then rewrites banks, leaving the table unsorted). */
    for (int i = 0; i < FF4_DISPATCH_COUNT; i++) {
        if (ff4_dispatch_table[i].pc == pc) {
            if (ff4_dispatch_filter && !ff4_dispatch_filter(pc))
                return 0;  /* excluded: fall through to pure interpretation */
            ff4_dispatch_hits++;
            if (ff4_dispatch_trace) ff4_dispatch_trace(pc);
            if (ff4_dispatch_charge_cycles) {
                uint64_t cyc_before = snes->cycles;
                ff4_dispatch_table[i].fn(snes);
                int32_t cost = ff4_dispatch_cycle_cost[i];
                if (cost > 0) {
                    int64_t body = (int64_t)(snes->cycles - cyc_before);
                    int64_t charge = (int64_t)cost - body;
                    if (charge > 0) snes_runCycles(snes, (int)charge);
                }
            } else {
                ff4_dispatch_table[i].fn(snes);
            }
            return 1;
        }
    }
    ff4_dispatch_misses++;
    ff4_miss_per_bank[(pc >> 16) & 0xff]++;
    if (ff4_dispatch_miss_trace) ff4_dispatch_miss_trace(pc);
#ifdef FF4_AUTOBOOT
    {   /* record unique miss PCs in a small ring (dedup against existing) */
        int already = 0;
        for (int i = 0; i < 8; i++) {
            if (g_diag_miss_ring[i] == pc) { already = 1; break; }
        }
        if (!already) {
            g_diag_miss_ring[g_diag_miss_ring_head] = pc;
            g_diag_miss_ring_head = (g_diag_miss_ring_head + 1) & 7;
        }
    }
#endif
    return 0;
}


/* ---------------------------------------------------------
 * Auto-stubs (hand-maintained; originally seeded by gen_dispatch.py
 * back when it still wrote this file — it is now read-only, see
 * dispatch_all.h. `python gen_dispatch.py` reports which NEW *_emu
 * names a candidate routine would need; add the stub here by hand.)
 *
 * Weak no-op definitions for *_emu helpers referenced by
 * the dispatched .c files above but not yet declared in
 * ff4_helpers.c. A stronger definition elsewhere (e.g. a
 * real RunEmulatedFunc-backed delegate) will override the
 * weak symbol automatically.
 * --------------------------------------------------------- */

__attribute__((weak)) void fade_out_menu_emu(Snes *snes) { (void)snes; }  /* first needed by field/CheckMenu.c */
__attribute__((weak)) void main_menu_emu(Snes *snes) { (void)snes; }  /* first needed by field/CheckMenu.c */
__attribute__((weak)) void fade_in_menu_emu(Snes *snes) { (void)snes; }  /* first needed by field/CheckMenu.c */
__attribute__((weak)) void exec_event_emu(Snes *snes) { (void)snes; }  /* first needed by field/CheckMenu.c */
__attribute__((weak)) void fadein_emu(Snes *snes) { (void)snes; }  /* first needed by field/FadeIn.c */
__attribute__((weak)) void remove_float_emu(Snes *snes) { (void)snes; }  /* first needed by field/InitWorld.c */
__attribute__((weak)) void tfr_world_gfx_emu(Snes *snes) { (void)snes; }  /* first needed by field/InitWorld.c */
__attribute__((weak)) void invert_pal_emu(Snes *snes) { (void)snes; }  /* first needed by field/InitWorld.c */
__attribute__((weak)) void update_lava_anim_emu(Snes *snes) { (void)snes; }  /* first needed by field/UpdateWaterLavaAnim.c */
__attribute__((weak)) void update_water_anim_emu(Snes *snes) { (void)snes; }  /* first needed by field/UpdateWaterLavaAnim.c */
__attribute__((weak)) void load_whirlpool_pal_emu(Snes *snes) { (void)snes; }  /* first needed by field/LoadOverworldLeviathan.c */
__attribute__((weak)) void draw_whirlpool_emu(Snes *snes) { (void)snes; }  /* first needed by field/LoadOverworldLeviathan.c */
__attribute__((weak)) void ExecSound_ext_emu(Snes *snes) { (void)snes; }  /* first needed by field/LoadMapStack.c */
__attribute__((weak)) void decode_sub_tilemap_emu(Snes *snes) { (void)snes; }  /* first needed by field/DecodeBG1Tilemap.c */
