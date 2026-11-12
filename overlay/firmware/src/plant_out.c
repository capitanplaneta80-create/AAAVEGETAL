/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mauricio Vásquez Arias, Universidad EAFIT */
/* VEGETAL hacia afuera, en el ISR de audio (núcleo 0), después de las partes (audio.c audio_block):
 *  - cada nota de un nodo sale por MIDI OUT (USB) en el canal de su pista + 4 (CH1-4 -> 5..8), con su nota apagada
 *    al acabar su duración; AUM (o un DAW) puede llevarla a Koala, Poly 2 o cualquier sinte. Deja siempre sitio
 *    en la cola de MIDI para el secuenciador (midi_out_room).
 *  - y entra a la cola del espejo (plo_m), que editor.c manda por SysEx desde el bucle principal (ED_VEG_NOTES).
 * Las colas y plo_note están en engines.c (el núcleo que hace cada parte las llena). */
#define PLO_OFFS 12u
static struct {
    uint8_t note[PLO_OFFS], ch[PLO_OFFS];
    uint32_t t[PLO_OFFS];
} plo_ofs[NPART] __attribute__((section(".pool")));
static struct { uint8_t n; } plo_off[NPART];
static uint32_t plo_link;

static int plo_off_send(uint32_t k, uint32_t i)        /* apaga la i-ésima pendiente de la parte k: 0 = sin sitio */
{
    uint32_t last;
    if (midi_out_room() && !midi_out_try(0x08u | (0x80u | plo_ofs[k].ch[i]) << 8 | (uint32_t)plo_ofs[k].note[i] << 16))
        return 0;
    last = --plo_off[k].n;
    plo_ofs[k].note[i] = plo_ofs[k].note[last];
    plo_ofs[k].ch[i] = plo_ofs[k].ch[last];
    plo_ofs[k].t[i] = plo_ofs[k].t[last];
    return 1;
}

static int32_t plo_pitch16(track_t *t, uint32_t node, uint32_t deg)   /* la altura con que suena el nodo; -1: ninguna */
{
    const int16_t *spec = PL_SPEC[clamp(t->p[P_E4], 0, PL_NSPEC - 1)];
    uint32_t i, nv = 0, rank;
    int32_t d = (int32_t)deg - 32 + 64;
    for (i = 0; i < NVOICE; i++)
        nv += t->v[i].active;
    if (!nv)
        return -1;
    rank = node % nv;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && !rank--)
            return t->v[i].pitch16 + spec[d & 15] + ((d >> 4) - 4) * 192;
    return -1;
}

static void plo_drain(void)
{
    uint32_t k, i, now = fm1_ms, bpm = PL_BPM();
    if (midi_out_link() != plo_link) {                  /* otro anfitrión (o ninguno): lo pendiente no vale */
        plo_link = midi_out_link();
        for (k = 0; k < NPART; k++)
            plo_off[k].n = 0;
    }
    for (k = 0; k < NPART; k++) {
        track_t *t = &trk[k];
        int plant = ENGINES[eng_idx(t->eng_req)] == &ENG_PLANT;
        uint8_t r = plo_q[k].r, w = plo_q[k].w;
        for (i = 0; i < plo_off[k].n;) {                /* las que acabaron (todas, si la pista dejó de ser VEGETAL) */
            if (!plant || !plo_midi || (int32_t)(now - plo_ofs[k].t[i]) >= 0) {
                if (!plo_off_send(k, i))
                    break;
            } else
                i++;
        }
        RING_PUBLISH();
        for (; r != w; r++) {
            const plo_ev_t *e = &plo_qe[k][r % PLO_Q];
            int32_t p16, note = 0;
            if (!plant)
                continue;
            p16 = plo_pitch16(t, e->node, e->deg);
            if (p16 >= 0)
                note = clamp((p16 + 8) >> 4, 0, 127);
            if (p16 >= 0 && plo_midi && midi_out_room() > 24u) {
                uint32_t ch = (trk_midi_ch(k) + 4u) & 15u, j;
                for (j = 0; j < plo_off[k].n; j++)      /* la misma nota aún suena: se apaga antes */
                    if (plo_ofs[k].note[j] == note && plo_ofs[k].ch[j] == ch) {
                        plo_off_send(k, j);
                        break;
                    }
                if (plo_off[k].n >= PLO_OFFS)
                    plo_off_send(k, 0);
                if (plo_off[k].n < PLO_OFFS &&
                    midi_out_try(0x09u | (0x90u | ch) << 8 | (uint32_t)note << 16 | (uint32_t)e->vel << 24)) {
                    uint32_t ms = (uint32_t)e->dur * 3750u / bpm;   /* dur en 1/16 de pulso */
                    j = plo_off[k].n++;
                    plo_ofs[k].note[j] = (uint8_t)note;
                    plo_ofs[k].ch[j] = (uint8_t)ch;
                    plo_ofs[k].t[j] = now + (ms < 20u ? 20u : ms > 3000u ? 3000u : ms);
                }
            }
            if (plo_mirror && plo_m.w - plo_m.r < PLO_M) {
                plo_mev_t *m = &plo_me[plo_m.w % PLO_M];
                m->part = (uint8_t)k;
                m->node = e->node;
                m->deg = e->deg;
                m->vel = e->vel;
                m->dur = e->dur;
                m->par = e->par;
                m->note = (uint8_t)(p16 >= 0 ? note : 0);
                m->x = e->x;
                m->y = e->y;
                RING_PUBLISH();
                plo_m.w++;
            }
        }
        plo_q[k].r = r;
    }
}
