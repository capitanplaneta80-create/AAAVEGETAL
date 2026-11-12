/* SPDX-License-Identifier: GPL-3.0-only */
/* VEGETAL hacia afuera (plant_out.c, editor.c ED_VEG_*): notas por MIDI OUT y el espejo, sobre el firmware real. */
/* The firmware editor handler, USB framing and UART recovery against RAM flash.
 * Build with the generated tables and the same flags as hostsim.c. */
static unsigned char host_samples[3][0x14000];
#define SMP_USER_XIP(k) host_samples[k]
#define FELUCCA_OTA 1
#define FELUCCA_FLASH 0
#define FELUCCA_VERSION "TEST"
#define FELUCCA_CDC 1                                    /* (as the firmware: MENU > USB SERIAL switches the console) */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define MENU_N (32u + FELUCCA_CPU2)                  /* 1.6.1 (core.h FELUCCA_CPU2, the default): 2ND CORE, id 32 */

static uint32_t host_progress = 1, host_erases, host_writes;
static uint8_t host_wire[4096];
static uint32_t host_wire_n;
static void host_drain(void)
{
    while (so_r != so_w) {
        uint32_t p = sx_out_q[so_r++ % SXQ], cin = p & 15u, i;
        uint32_t n = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        for (i = 0; i < n && host_wire_n < sizeof host_wire; i++)
            host_wire[host_wire_n++] = (uint8_t)(p >> (8u * (i + 1u)));
    }
}
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { host_drain(); fm1_ms++; }
static void fm1_wdt_feed(void)
{
    fm1_ms++;
    if (host_progress && transport_req == 2u) { seq_stop(); transport_req = 0; }
}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }   /* (MENU > SCREEN OFF: lcd.c) */
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
static void panel_setup(void) {}
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

static uint32_t flash_ok = 1;
static void audio_silence(void) {}
static void fl_inval(uint32_t off, uint32_t n) { (void)off; (void)n; }
static uint8_t *host_flash_ptr(uint32_t off) { return &host_samples[0][0] + off - SMP_USER_BASE; }
static int fl_erase4k(uint32_t off, uint32_t *took)
{
    memset(host_flash_ptr(off), 0xFF, 4096); *took = 0; host_erases++; return 0;
}
static int fl_write(uint32_t off, const void *p, uint32_t n)
{
    memcpy(host_flash_ptr(off), p, n); host_writes++; return 0;
}
static int st_read(uint32_t off, void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int st_erase(uint32_t off) { (void)off; return -1; }
#include "../firmware/src/storage.c"
#include "../firmware/src/editor.c"
#include "../firmware/src/plant_out.c"
static void reset(void)
{
    uint32_t t, i;
    memset(&song, 0, sizeof song); memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain); chain_defaults(&chain_config);
    memset(&ed_w, 0, sizeof ed_w); memset(&ui, 0, sizeof ui);
    memset(&favorites, 0, sizeof favorites); memset(&settings, 0, sizeof settings); settings_init();
    memset(proj_slot, 0, sizeof proj_slot); memset(up_bank, 0, sizeof up_bank);
    memset(&um, 0, sizeof um); memset(usr_nz, 0, sizeof usr_nz);
    host_progress = 1; host_erases = host_writes = host_wire_n = 0;
    transport_req = panic_req = 0; sx_ready = sx_collect = sx_busy = 0;
    so_r = so_w = mi_r = mi_w = 0; midi_in_overflow = 0; usb.config = 1;
    for (i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
    for (t = 0; t < NTRK; t++) {
        track_defaults(&trk[t]); set_engine_of(&trk[t], 0); apply_preset_to(&trk[t], 0);
        trk[t].engine = trk[t].eng_req; track_defaults_steps(&trk[t]);
    }
}
static uint32_t request(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t i;
    host_wire_n = 0; ota_frame_done();
    sysex_byte(0xF0); sysex_byte(ED_HDR0); sysex_byte(ED_HDR1); sysex_byte(ED_HDR2); sysex_byte((uint8_t)cmd);
    for (i = 0; i < n; i++) sysex_byte(a[i]);
    sysex_byte(0xF7); ed_service(); host_drain();
    return host_wire_n;
}

static int check(const char *what, int ok) { printf("vegetal: %-66s %s\n", what, ok ? "ok" : "FAIL"); return !ok; }
static uint32_t n_pos, n_ev0, n_ev0n, on[16], off[16], other, bad_ev, n_ev, n_state, st_plant, st_proc_ok;
static int16_t held[16][128];
static int16_t maxheld;
static void midi_drain(void)
{
    while (mo_r != mo_w) {
        uint32_t p = midi_out_q[mo_r++ % MQ], st = p >> 8 & 0xF0u, ch = p >> 8 & 15u, nt = p >> 16 & 127u, v = p >> 24 & 127u;
        if (st == 0x90u && v) { on[ch]++; if (++held[ch][nt] > maxheld) maxheld = held[ch][nt]; }
        else if (st == 0x80u || st == 0x90u) { off[ch]++; held[ch][nt]--; }
        else other++;
    }
}
static const uint8_t PROC_OF[4] = {0, 1, 4, 3};       /* engines.c TRK_DEF: GIRASOL HELECHO NICTINASTIA MICORRIZA */
static void wire_parse(void)
{
    uint32_t i = 0;
    while (i + 5u < host_wire_n) {
        uint32_t j = i;
        while (j < host_wire_n && host_wire[j] != 0xF7) j++;
        if (j >= host_wire_n) break;
        if (host_wire[i] == 0xF0 && host_wire[i + 4] == 96) {
            uint32_t c = host_wire[i + 5], k;
            if (j - i != 6u + 11u * c || !c || c > 4u) bad_ev++;
            for (k = 0; k < c && i + 6u + 11u * k + 10u < j; k++) {
                const uint8_t *e = &host_wire[i + 6u + 11u * k];
                if (e[0] > 3u || e[1] > 127u || e[2] > 95u || !e[3]) bad_ev++;
                if (e[0] == 0u) { n_ev0++; if (e[4]) n_ev0n++; }
                n_ev++;
            }
        } else if (host_wire[i] == 0xF0 && host_wire[i + 4] == 99) {
            uint32_t c = host_wire[i + 7];
            n_pos++;
            if (j - i != 8u + 5u * c || !c || c > 8u || host_wire[i + 5] > 3u || host_wire[i + 6] + c > 128u) bad_ev++;
        } else if (host_wire[i] == 0xF0 && host_wire[i + 4] == 97) {
            uint32_t k;
            n_state++;
            if (j - i != 8u + 40u) bad_ev++;
            for (k = 0; k < 4u; k++) {
                const uint8_t *p = &host_wire[i + 8u + 10u * k];
                st_plant += p[0];
                st_proc_ok += p[1] == PROC_OF[k];
            }
        }
        i = j + 1u;
    }
    host_wire_n = 0;
}
static void run(uint32_t secs, uint32_t keys)
{
    uint32_t b, nb = secs * FS / CTL, acc = 0;
    for (b = 0; b < nb; b++) {
        int32_t o[2 * CTL];
        fm1_in.notes = keys;
        mix_block(o, CTL);
        plo_drain();
        midi_drain();
        acc += CTL * 1000u;
        while (acc >= FS) { acc -= FS; fm1_ms++; }
        if (b % 8u == 0u) { ed_sync(); host_drain(); wire_parse(); }
        if (b % 1378u == 0u) { uint8_t a = 0; request(ED_PING, &a, 0); wire_parse(); }
    }
}
int main(void)
{
    int bad = 0;
    uint32_t k, ons = 0, offs = 0, mute = 0;
    uint8_t a[2];
    reset();
    for (k = 0; k < NTRK; k++) { host_preset(&trk[k], ENGI_PLANT, PROC_OF[k]); trk[k].engine = trk[k].eng_req; }
    a[0] = 5; request(ED_WATCH, a, 1);
    bad += check("WATCH 5: watching, the mirror on (bit 2)", host_wire[5] == 5u && plo_mirror);
    host_wire_n = 0;
    run(6, 1u << 7 | 1u << 11 | 1u << 14);
    for (k = 0; k < 16; k++) { ons += on[k]; offs += off[k]; if (on[k] && k > 4u) mute += on[k]; }
    printf("vegetal: parte 1: %u eventos, %u con nota\n", n_ev0, n_ev0n);
    printf("vegetal: %u notas, %u apagadas, %u eventos al espejo, %u estados, a lo más %d a la vez en una nota\n",
           ons, offs, n_ev, n_state, maxheld);
    bad += check("plant notes go out (track 1, MIDI channel 5)", on[4] > 10u);
    bad += check("only the selected track holds the keys: channels 6-8 silent", mute == 0u);
    bad += check("the mirror gets the node events, well formed", n_ev > 10u && !bad_ev);
    bad += check("VEG_POS: the figure's positions go round (> 100 frames/s), well formed", n_pos > 600u && !bad_ev);
    bad += check("VEG_STATE ~10 per second: four VEGETAL parts, their PROC", n_state >= 40u && st_plant == 4u * n_state &&
                 st_proc_ok == 4u * n_state);
    run(6, 0);
    ons = offs = 0;
    for (k = 0; k < 16; k++) { ons += on[k]; offs += off[k]; }
    bad += check("every note ends (on = off) after the keys are up", ons == offs);
    bad += check("never two of the same note at once", maxheld <= 1);
    a[0] = 0; request(ED_VEG_CFG, a, 1);
    bad += check("VEG_CFG 0: version 1, MIDI off, mirror on", host_wire[5] == 1u && host_wire[6] == 0u && host_wire[7] == 1u);
    host_wire_n = 0;
    ons = on[4];
    run(3, 1u << 7 | 1u << 11);
    bad += check("MIDI off: no more plant notes; the mirror goes on", on[4] == ons && n_ev > 0u);
    a[0] = 1; request(ED_VEG_CFG, a, 1);
    { uint32_t ne = n_ev; a[0] = 3; request(ED_WATCH, a, 1);
      bad += check("WATCH 3 (the official editor): the mirror off", host_wire[5] == 3u && !plo_mirror);
      host_wire_n = 0; run(2, 1u << 7); wire_parse();
      bad += check("then no VEG frames", n_ev == ne); }
    printf("%s\n", bad ? "VEGETAL OUT TEST FAILED" : "vegetal out test passed");
    return bad != 0;
}
