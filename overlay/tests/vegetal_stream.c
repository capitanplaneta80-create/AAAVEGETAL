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


/* VEGETAL: graba lo que el firmware manda por USB-MIDI (SysEx del espejo y notas) en JSON, para probar espejo.html */
static FILE *J; static int first = 1;
static void put(double t, const uint8_t *b, uint32_t n){ uint32_t i; fprintf(J, "%s[%.4f,[", first ? "" : ",\n", t); first = 0;
  for (i = 0; i < n; i++) fprintf(J, "%s%u", i ? "," : "", b[i]); fprintf(J, "]]"); }
static double T;
static void wire_out(void){ uint32_t i = 0, j; while (i < host_wire_n){ for (j = i; j < host_wire_n && host_wire[j] != 0xF7; j++); if (j >= host_wire_n) break; put(T, host_wire + i, j - i + 1); i = j + 1; } host_wire_n = 0; }
static void midi_out(void){ while (mo_r != mo_w){ uint32_t p = midi_out_q[mo_r++ % MQ]; uint8_t b[3] = {(uint8_t)(p >> 8), (uint8_t)(p >> 16), (uint8_t)(p >> 24)}; put(T, b, 3); } }
int main(int argc, char **argv)
{
    uint32_t b, k, acc = 0, nb = 30u * FS / CTL;
    static const uint8_t PR[4] = {0, 10, 3, 17};
    uint8_t a = 7;
    J = fopen(argc > 1 ? argv[1] : "veg_stream.json", "w"); fprintf(J, "[\n");
    reset();
    for (k = 0; k < NTRK; k++){ host_preset(&trk[k], ENGI_PLANT, PR[k]); trk[k].engine = trk[k].eng_req; }
    request(ED_WATCH, &a, 1); wire_out();
    for (b = 0; b < nb; b++){
        int32_t o[2 * CTL];
        uint32_t ph = (b / 1378u) % 8u;
        if (b == 10u * 1378u) song.sel = 1;                       /* a los 10 s: pista 2 (ANILLOS) */
        if (b == 20u * 1378u) song.sel = 3;                       /* a los 20 s: pista 4 (MUSGO) */
        if (b % 1378u == 700u) trk[song.sel].p[P_E0 + 1] = (int16_t)(20 + (b / 1378u) * 7 % 100);   /* una perilla */
        fm1_in.notes = ph < 6u ? (1u << 7 | 1u << 11 | 1u << 14) : 0u;
        mix_block(o, CTL); plo_drain(); midi_out();
        acc += CTL * 1000u; while (acc >= FS){ acc -= FS; fm1_ms++; }
        T = (double)b * CTL / FS;
        if (b % 8u == 0u){ ed_sync(); host_drain(); wire_out(); }
        if (b % 1378u == 0u){ request(ED_PING, 0, 0); wire_out(); }
    }
    fprintf(J, "\n]\n"); fclose(J); return 0;
}
