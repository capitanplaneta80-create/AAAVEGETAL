/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Mauricio Vásquez Arias, Escuela de Artes y Humanidades, Universidad EAFIT */
/* VEGETAL: la familia vegetal de Ecotono (y de las cartas Tríadas) como motor de Felucca para el M-VAVE FM-1.
 *
 * Veintiún procesos (PROC), los mismos organismos vegetales de Ecotono, cada uno adaptado al FM-1: en vez de mandar
 * notas MIDI a otro sintetizador, hace sonar nodos propios. Cada nodo es una voz leñosa: una senoide y un parcial de
 * timbre elegido por motivo (TIMB: SENO MADERA CAMPAN VIDRIO CUERDA SOPLO HOJA AGUA), con su envolvente. El tiempo se mide en pulsos del tempo de Felucca (BPM), como en
 * Ecotono; CREC elige cada cuánto da un paso el proceso (4, 2, 1, 1/2, 1/4 o 1/8 de pulso). Las alturas son grados
 * del espectro (ESPC); cada 16 grados, una octava más.
 *
 *  0 FILO    Filotaxis: un primordio por paso a 137,5° del anterior; el sector donde nace elige la nota.
 *  1 PLANTA  Planta L: sistema de Lindenmayer X -> F+[[X]-X]-F[-FX]+X, F -> FF, leído por una tortuga.
 *  2 RAIZ    Raíz: la raíz principal baja un segmento por paso y echa raíces laterales.
 *  3 MICO    Micorriza: escucha a otra pista (FUEN) y le responde por la red del suelo, con retraso y transposición.
 *  4 NICTI   Nictinastia: el acorde se abre de una a varias notas y se vuelve a cerrar en el ciclo del día.
 *  5 SEMIL   Semillas: caen al azar (Poisson), agrupadas alrededor de la planta madre.
 *  6 RAMIF   Ramificación: árbol binario recorrido nivel por nivel, del tronco a la copa.
 *  7 CIRCUM  Circumnutación: la melodía gira en espiral alrededor de un eje que asciende.
 *  8 POLIN   Polinización: un polinizador visita dos flores y lleva una nota, el polen, de una a otra.
 *  9 HOJAR   Hojarasca: hojas que caen meciéndose, en notas lentas e irregulares.
 * 10 ANILLO  Anillos de árbol: un acorde por año; su tamaño, duración e intensidad siguen el clima.
 * 11 SAVIA   Savia: escalas ascendentes que corren con el sol y se detienen de noche.
 * 12 FOTOT   Fototropismo: el tallo persigue a una luz que recorre el registro, con retraso.
 * 13 RAICES  Raíces exploradoras: puntas que bajan buscando agua y se bifurcan.
 * 14 FLOR    Floración: botones cada vez más frecuentes; la flor se abre en arpegio áureo y caen los pétalos.
 * 15 ESTOMA  Estomas: la hoja respira; la apertura mueve el timbre y, abierta, exhala notas largas.
 * 16 ZARCI   Zarcillo: gira con giros cada vez más amplios hasta agarrarse en un acorde.
 * 17 MUSGO   Musgo: una colonia cubre el registro célula a célula y suena como una alfombra.
 * 18 GERMIN  Germinación: semillas dormidas brotan y suben nota a nota hasta su altura.
 * 19 RIZOMA  Rizoma: una red sin centro entre grados; se recorre y de vez en cuando se recablea.
 * 20 RELOJ   Reloj circadiano: el día de la planta (el FM-1 no tiene reloj: un ciclo de RELO compases).
 * EDIT 1: PROC NODO (nodos que pueden sonar) CREC (el paso) y A; EDIT 2: ESPC TIMB DECA y B. A y B cambian de nombre y
 * de sentido con el proceso (plant_desc).
 *
 * Seguridad y carga: punto fijo, sin float, sin asignación, sin bibliotecas; ningún bucle de audio divide; todo bucle
 * acotado. Un regulador (pl_govern) lee la carga real del FM-1 y fija cuántos nodos suenan: crece bajo 62 %, cede
 * sobre 72 %, recorta un cuarto sobre 80 %. Felucca descarta voces desde el 85 %.
 * Pantalla: cada nodo lleva una posición (x, y) y un padre; ui_graph.c (graph_plant) los dibuja en HOME. */
#include "pl_tables.h"

#ifndef PL_MAX
#define PL_MAX 128
#endif
#define PL_MIN 4
#define PL_LSYS 1600
#define PL_STACK 16
#ifndef PL_LOAD_Q8
#define PL_LOAD_Q8() 0u
#endif
#ifndef PL_FAST_Q8
#define PL_FAST_Q8() PL_LOAD_Q8()
#endif
#ifndef PL_BPM
#define PL_BPM() 120u            /* el anfitrión: el tempo (Felucca: song.g[G_BPM]) */
#endif
#ifndef PL_COUNT
#define PL_COUNT(kind, n) ((void)0)
#endif
#define PL_LO   (62u * 256u / 100u)
#define PL_HI   (72u * 256u / 100u)
#define PL_HARD (80u * 256u / 100u)
#define PL_INC_MAX 0x60000000u
#define PL_WOOD 281
#ifndef PL_OUT_NOTE                                     /* el firmware: la nota sale al espejo y por MIDI (plant_out.c) */
#define PL_OUT_NOTE(P, q, vel, dur64) ((void)0)
#endif
#define PL_GOLD 25033u
#define PL_NONE 0xFFu
#define PL_NPROC 21

static const char *const N_PL_PROC[PL_NPROC] = {
    "FILO", "PLANTA", "RAIZ", "MICO", "NICTI", "SEMIL", "RAMIF", "CIRCUM", "POLIN", "HOJAR", "ANILLO",
    "SAVIA", "FOTOT", "RAICES", "FLOR", "ESTOMA", "ZARCI", "MUSGO", "GERMIN", "RIZOMA", "RELOJ"};
enum { PL_FILO, PL_PLANTA, PL_RAIZ, PL_MICO, PL_NICTI, PL_SEMIL, PL_RAMIF, PL_CIRCUM, PL_POLIN, PL_HOJAR, PL_ANILLO,
       PL_SAVIA, PL_FOTOT, PL_RAICES, PL_FLOR, PL_ESTOMA, PL_ZARCI, PL_MUSGO, PL_GERMIN, PL_RIZOMA, PL_RELOJ };
/* cómo se dibuja (graph_plant): 1 = la figura del proceso; 2 = una partitura que corre (x = el tiempo, y = el grado);
 * bit 4: con las aristas de la red (nb) */
static const uint8_t PL_VIEW[PL_NPROC] = {1, 1, 1, 1 | 4, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1 | 4, 2};

typedef struct {
    int32_t x, y;                /* Q16 en [-1, 1]: la pantalla */
    uint32_t ph, ph2;
    int32_t a, tgt, env;         /* Q15 */
    int32_t gx;                  /* un dato propio del proceso */
    uint16_t hold, age;          /* bloques que sostiene; bloques desde que nació */
    uint16_t timer, refr;        /* las redes: bloques hasta disparar, refractario */
    uint16_t aux;                /* un dato propio del proceso (FILO: el ángulo) */
    int16_t deg;                 /* el grado */
    int16_t vel;                 /* la intensidad del ataque, Q15 */
    int16_t str;                 /* la fuerza de la señal (redes) */
    uint8_t par, live;           /* el padre (para dibujar una línea), PL_NONE = ninguno */
    uint8_t nb[3], nd[3];        /* las redes: los vecinos y su distancia */
} pl_node_t;

typedef struct {
    pl_node_t n[PL_MAX];
    union {                      /* lo grande que solo usa un proceso */
        struct { uint8_t s[2][PL_LSYS]; } l;   /* PLANTA */
        int8_t g[64][3];         /* RIZOMA: el grafo entre grados */
    } u;
    uint16_t llen, lpos;
    uint8_t lbuf, lgen;
    int16_t tdeg, tsp;
    int32_t tx, ty;
    uint16_t tdir;
    uint8_t tpar, pad0;
    int16_t sdeg[PL_STACK];
    int32_t sx[PL_STACK], sy[PL_STACK];
    uint16_t sdir[PL_STACK];
    uint8_t spar[PL_STACK];
    int32_t s[24];               /* el estado de cada proceso (pl_* dice qué es cada uno) */
    uint32_t clk;                /* el reloj en 1/64 de pulso, Q16 */
    uint32_t next;               /* el próximo paso, en 1/64 de pulso */
    uint32_t k;                  /* pasos dados */
    uint32_t bpp;                /* bloques por pulso */
    int32_t lx, ly;              /* la luz o el sol, para dibujar (Q16) */
    int32_t nz;                  /* el ruido de SOPLO y HOJA (xorshift) */
    int32_t wmod;                /* ESTOMA: la apertura (mueve el timbre), Q15; 32767 en los demás */
    uint16_t built, nnet;
    int32_t rng;
    uint32_t dph;                /* la fase del día (NICTI, SAVIA, ESTOMA, RELOJ), 1/2^32 de ciclo */
    uint16_t rr, ncap, nlive;
    uint8_t nv, gcool, proc, init, self, pad1;
} pl_part_t;
ENG_MEM_FITS("VEGETAL", sizeof(pl_part_t));

/* lo que tocó cada pista, para la micorriza (fuera de la memoria de motor: lo leen las otras pistas) */
#define PL_BUS 16u
static struct { uint8_t w; int8_t deg[PL_BUS]; uint8_t vel[PL_BUS]; } pl_bus[NPART];

static pl_part_t *pl_of(const track_t *t) { return (pl_part_t *)ENG_MEM((uint32_t)(t - trk) % NPART); }
static inline uint32_t pl_rnd(pl_part_t *P) { return noise32(&P->rng); }
static inline int32_t pl_rs(pl_part_t *P) { return (int32_t)(pl_rnd(P) >> 16) - 32768; }
static inline uint32_t pl_ru(pl_part_t *P, uint32_t n) { return n ? (uint32_t)(((pl_rnd(P) >> 16) * n) >> 16) : 0u; }   /* 0 .. n-1 */
static inline int32_t pl_cos(uint32_t a16) { return SINE[((a16 + 16384u) & 0xFFFFu) >> 6]; }
static inline int32_t pl_sin(uint32_t a16) { return SINE[(a16 & 0xFFFFu) >> 6]; }
static inline int32_t pl_ydeg(int32_t d) { return clamp((d - 12) * 4800, -62000, 62000); }   /* grado -> y */

/* ------------------------------------------------------------------ el regulador --- */
static void pl_govern(pl_part_t *P)
{
    uint32_t L = PL_LOAD_Q8(), F = PL_FAST_Q8();
    if (P->gcool) {
        P->gcool--;
        return;
    }
    if (F > PL_HARD) {
        P->ncap = (uint16_t)(P->ncap - (P->ncap >> 2));
        P->gcool = 4;
    } else if (L > PL_HI) {
        P->ncap = (uint16_t)(P->ncap > 2u ? P->ncap - 2u : 0u);
        P->gcool = 8;
    } else if (L < PL_LO && F < PL_HI && P->ncap < PL_MAX) {
        P->ncap++;
        P->gcool = 16;
    }
    if (P->ncap < PL_MIN)
        P->ncap = PL_MIN;
}

/* ------------------------------------------------------------------ nodos y notas --- */
static pl_node_t *pl_spawn(pl_part_t *P, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        pl_node_t *q = &P->n[i];
        if (!q->live && !q->a)
            return q;
    }
    if (!n)
        return 0;
    P->rr = (uint16_t)((P->rr + 1u) % n);
    return &P->n[P->rr];
}
/* una nota: grado, intensidad (0..127), duración en 1/64 de pulso, posición en pantalla, padre; el nodo o 0 */
static pl_node_t *pl_note(pl_part_t *P, uint32_t n, int32_t deg, int32_t vel, uint32_t dur64, int32_t x, int32_t y,
                          uint32_t par)
{
    pl_node_t *q = pl_spawn(P, n);
    uint32_t pi;
    if (!q)
        return 0;
    q->deg = (int16_t)clamp(deg, -32, 63);
    q->vel = (int16_t)(clamp(vel, 0, 127) * 258);
    q->env = q->vel;
    q->hold = (uint16_t)clamp((int32_t)((dur64 * P->bpp) >> 6), 1, 60000);
    q->x = clamp(x, -65535, 65535);
    q->y = clamp(y, -65535, 65535);
    q->par = (uint8_t)par;
    q->live = 1;
    q->age = 0;
    q->timer = 0;
    PL_OUT_NOTE(P, q, vel, dur64);
    pi = P->self % NPART;                               /* al bus, para la micorriza de otra pista */
    {
        uint32_t w = pl_bus[pi].w++ % PL_BUS;
        pl_bus[pi].deg[w] = (int8_t)q->deg;
        pl_bus[pi].vel[w] = (uint8_t)clamp(vel, 0, 127);
    }
    return q;
}
static uint8_t pl_idx(const pl_part_t *P, const pl_node_t *q) { return (uint8_t)(q - P->n); }

/* ------------------------------------------------------------------ PLANTA (sistema L) --- */
static const char PL_RULE_X[] = "F+[[X]-X]-F[-FX]+X";
static void pl_lsys_seed(pl_part_t *P)
{
    P->lbuf = 0;
    P->u.l.s[0][0] = 'X';
    P->llen = 1;
    P->lgen = 0;
    P->lpos = 0;
}
static void pl_lsys_grow(pl_part_t *P)
{
    const uint8_t *a = P->u.l.s[P->lbuf];
    uint8_t *b = P->u.l.s[P->lbuf ^ 1u];
    uint32_t i, j = 0, k;
    for (i = 0; i < P->llen; i++) {
        if (a[i] == 'X') {
            if (j + sizeof PL_RULE_X - 1u > PL_LSYS)
                return;
            for (k = 0; k < sizeof PL_RULE_X - 1u; k++)
                b[j++] = (uint8_t)PL_RULE_X[k];
        } else if (a[i] == 'F') {
            if (j + 2u > PL_LSYS)
                return;
            b[j++] = 'F';
            b[j++] = 'F';
        } else {
            if (j + 1u > PL_LSYS)
                return;
            b[j++] = a[i];
        }
    }
    P->lbuf ^= 1u;
    P->llen = (uint16_t)j;
    P->lgen++;
}
static void pl_turtle_home(pl_part_t *P)
{
    P->tdeg = 0;
    P->tsp = 0;
    P->tx = 0;
    P->ty = -60000;
    P->tdir = 16384;
    P->tpar = PL_NONE;
    P->lpos = 0;
}
static void pl_planta(pl_part_t *P, const int16_t *p, uint32_t n, uint32_t dur)
{
    uint32_t gmax = 1u + ((uint32_t)p[P_E7] * 4u >> 7), guard, i;
    int32_t intv = 1 + (p[P_E3] * 6 >> 7);
    if (gmax > 4u)
        gmax = 4u;
    for (guard = 0; guard < 64u; guard++) {
        uint8_t s;
        if (P->lpos >= P->llen) {
            if (P->lgen < gmax)
                pl_lsys_grow(P);
            else {
                pl_lsys_seed(P);
                for (i = 0; i < PL_MAX; i++)
                    P->n[i].live = 0;
            }
            pl_turtle_home(P);
            if (P->lgen == 0)
                pl_lsys_grow(P);
            return;
        }
        s = P->u.l.s[P->lbuf][P->lpos++];
        if (s == 'F') {
            int32_t step = 34000 >> P->lgen;
            pl_node_t *q;
            P->tx = clamp(P->tx + ((pl_cos(P->tdir) * step) >> 15), -65535, 65535);
            P->ty = clamp(P->ty + ((pl_sin(P->tdir) * step) >> 15), -65535, 65535);
            q = pl_note(P, n, P->tdeg + 4, 110 - P->tsp * 6, dur, P->tx, P->ty, P->tpar);
            P->tpar = q ? pl_idx(P, q) : PL_NONE;
            return;
        }
        if (s == '+') {
            P->tdeg = (int16_t)clamp(P->tdeg + intv, -12, 27);
            P->tdir = (uint16_t)(P->tdir + 4551u);
        } else if (s == '-') {
            P->tdeg = (int16_t)clamp(P->tdeg - intv, -12, 27);
            P->tdir = (uint16_t)(P->tdir - 4551u);
        } else if (s == '[' && P->tsp < PL_STACK) {
            P->sdeg[P->tsp] = P->tdeg;
            P->sx[P->tsp] = P->tx;
            P->sy[P->tsp] = P->ty;
            P->sdir[P->tsp] = P->tdir;
            P->spar[P->tsp] = P->tpar;
            P->tsp++;
        } else if (s == ']' && P->tsp > 0) {
            P->tsp--;
            P->tdeg = P->sdeg[P->tsp];
            P->tx = P->sx[P->tsp];
            P->ty = P->sy[P->tsp];
            P->tdir = P->sdir[P->tsp];
            P->tpar = P->spar[P->tsp];
        }
    }
}

/* ------------------------------------------------------------------ FILO --- */
static uint32_t pl_isqrt(uint32_t v)
{
    uint32_t r = 0, bit = 1u << 30;
    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else
            r >>= 1;
        bit >>= 2;
    }
    return r;
}
static void pl_filo(pl_part_t *P, const int16_t *p, uint32_t n, uint32_t dur)
{
    uint32_t i, maxage = 8u + (uint32_t)p[P_E7] * 2u;
    uint32_t step = (uint32_t)((int32_t)PL_GOLD + (p[P_E3] - 64) * 40);
    pl_node_t *q;
    for (i = 0; i < n; i++) {                           /* al nacer uno, los demás envejecen: r = raíz de la edad */
        pl_node_t *o = &P->n[i];
        if (!o->live)
            continue;
        if (++o->gx >= (int32_t)maxage) {
            o->live = 0;
            continue;
        }
        {
            int32_t rad = (int32_t)pl_isqrt(((uint32_t)o->gx << 16) / maxage);   /* 0 .. 256 */
            o->x = (pl_cos(o->aux) * rad) >> 7;
            o->y = (pl_sin(o->aux) * rad) >> 7;
        }
    }
    q = pl_note(P, n, (int32_t)(((P->k * step) & 0xFFFFu) >> 12) + 4, 100, dur, 0, 0, PL_NONE);
    if (q) {
        q->aux = (uint16_t)(P->k * step);
        q->gx = 0;
    }
}

/* ------------------------------------------------------------------ RAIZ: raíz principal y laterales --- */
/* s[0] la profundidad de la principal; s[1] su último nodo; s[2..5] laterales: profundidad<<8 | lado, s[6..9] su nodo */
static void pl_raiz(pl_part_t *P, const int16_t *p, uint32_t n, uint32_t dur)
{
    int32_t prof = 6 + (p[P_E7] * 18 >> 7), i;          /* PROF: 6 .. 23 segmentos */
    pl_node_t *q;
    if (P->s[0] >= prof) {                              /* llegó al fondo: brota otra */
        P->s[0] = 0;
        P->s[1] = PL_NONE;
        for (i = 2; i < 10; i++)
            P->s[i] = 0;
        for (i = 0; i < PL_MAX; i++)
            P->n[i].live = 0;
    }
    P->s[0]++;
    q = pl_note(P, n, 24 - P->s[0], 100, dur, (pl_rs(P) >> 6), 60000 - P->s[0] * 120000 / prof, (uint32_t)P->s[1]);
    P->s[1] = q ? pl_idx(P, q) : PL_NONE;
    if ((int32_t)(pl_rnd(P) >> 25) < p[P_E3] && q) {   /* RAMA: brota una lateral */
        for (i = 2; i < 6; i++)
            if (!P->s[i]) {
                P->s[i] = (P->s[0] << 8) | ((int32_t)(pl_rnd(P) >> 31) + 1);
                P->s[i + 4] = pl_idx(P, q);
                break;
            }
    }
    for (i = 2; i < 6; i++) {                           /* las laterales crecen de lado y hacia abajo */
        pl_node_t *r0, *r1;
        int32_t side, dep;
        if (!P->s[i])
            continue;
        side = (P->s[i] & 0xFF) == 1 ? -1 : 1;
        dep = P->s[i] >> 8;
        r0 = &P->n[(uint32_t)P->s[i + 4] % PL_MAX];
        r1 = pl_note(P, n, 24 - dep - 2, 70, dur >> 1, clamp(r0->x + side * 9000, -64000, 64000), r0->y - 7000,
                     (uint32_t)P->s[i + 4]);
        if (!r1 || (pl_rnd(P) >> 30) == 0u)
            P->s[i] = 0;                                /* la lateral termina */
        else
            P->s[i + 4] = pl_idx(P, r1);
    }
}

/* ------------------------------------------------------------------ las redes (MICO, RIZOMA) --- */
static void pl_net_build(pl_part_t *P, uint32_t n, uint32_t soil)
{
    uint32_t i, j, k, per = 6;
    if (n != P->nnet) {
        for (i = 0; i < PL_MAX; i++) {
            pl_node_t *q = &P->n[i];
            q->x = pl_rs(P) * 2;
            q->y = soil ? clamp(-64000 + (int32_t)(pl_rnd(P) >> 17) * 2, -64000, 8000) : pl_rs(P) * 2;
            q->deg = (int16_t)clamp((q->y + 65536) >> 12, 0, 31);
            q->timer = q->refr = 0;
            q->nb[0] = q->nb[1] = q->nb[2] = PL_NONE;
            q->live = 0;
            q->env = q->a;
            q->par = PL_NONE;
        }
        P->nnet = (uint16_t)n;
        P->built = 0;
    }
    for (; P->built < n && per; per--, P->built++) {
        pl_node_t *q = &P->n[P->built];
        uint32_t bd[3] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
        for (j = 0; j < n; j++) {
            int32_t dx, dy;
            uint32_t d;
            if (j == P->built)
                continue;
            dx = (P->n[j].x - q->x) >> 8;
            dy = (P->n[j].y - q->y) >> 8;
            d = (uint32_t)(dx * dx + dy * dy);
            for (k = 0; k < 3u; k++)
                if (d < bd[k]) {
                    uint32_t m;
                    for (m = 2; m > k; m--) {
                        bd[m] = bd[m - 1u];
                        q->nb[m] = q->nb[m - 1u];
                    }
                    bd[k] = d;
                    q->nb[k] = (uint8_t)j;
                    break;
                }
        }
        for (k = 0; k < 3u; k++) {
            if (q->nb[k] == PL_NONE)
                continue;
            {
                int32_t dx = P->n[q->nb[k]].x - q->x, dy = P->n[q->nb[k]].y - q->y;
                q->nd[k] = (uint8_t)clamp(((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy)) >> 11, 1, 255);
            }
        }
    }
}
static void pl_net_fire(pl_part_t *P, uint32_t i, int32_t str, int32_t tr)
{
    pl_node_t *q = &P->n[i % PL_MAX];
    if (q->refr || str < 1200)
        return;
    q->str = (int16_t)str;
    q->timer = 1;
    q->gx = tr;                                         /* la transposición que lleva */
}
/* MICO: s[0] el último índice del bus leído */
static void pl_mico(pl_part_t *P, const int16_t *p, uint32_t n, uint32_t self)
{
    uint32_t i, k, speed = 1u + ((uint32_t)p[P_E2] >> 3), src = ((uint32_t)p[P_E7] * 4u) >> 7;
    int32_t tr = (p[P_E3] * 25 >> 7) - 12;              /* TRAN: -12 .. +12 grados */
    pl_net_build(P, n, 1);
    if (src >= NPART)
        src = NPART - 1u;
    if (src != self && P->built) {                     /* escucha a la pista FUEN */
        uint32_t w = pl_bus[src].w, r = (uint32_t)P->s[0] & 0xFFu;
        if (((w - r) & 0xFFu) > PL_BUS)                 /* se quedó atrás: solo lo último */
            r = (w - PL_BUS) & 0xFFu;
        while (r != w) {
            uint32_t e = r % PL_BUS;
            pl_net_fire(P, ((uint32_t)(pl_bus[src].deg[e] + 64) * 5u) % P->built, pl_bus[src].vel[e] * 258, tr);
            r = (r + 1u) & 0xFFu;
        }
        P->s[0] = (int32_t)r;
    } else
        P->s[0] = (int32_t)pl_bus[src].w;
    for (i = 0; i < n && i < P->built; i++) {
        pl_node_t *q = &P->n[i];
        if (q->refr)
            q->refr--;
        if (!q->timer || --q->timer)
            continue;
        if (q->str) {
            int32_t s2 = (q->str * 22000) >> 15;
            q->env = q->str;
            q->vel = q->str;
            q->hold = (uint16_t)(P->bpp >> 3);
            q->live = 1;
            q->deg = (int16_t)clamp(((q->y + 65536) >> 12) + q->gx, -16, 47);
            q->refr = (uint16_t)(400u / speed + 40u);
            PL_OUT_NOTE(P, q, q->str >> 8, 8u);
            for (k = 0; k < 3u; k++) {
                pl_node_t *m;
                if (q->nb[k] >= n)
                    continue;
                m = &P->n[q->nb[k]];
                if (m->refr || m->timer || s2 < 1200)
                    continue;
                m->str = (int16_t)s2;
                m->gx = q->gx;
                m->timer = (uint16_t)(1u + q->nd[k] * 24u / speed);
            }
            q->str = 0;
        }
    }
}
/* RIZOMA: s[0] el grado donde está; s[1] el nodo que sonó antes */
static void pl_rizoma(pl_part_t *P, const int16_t *p, uint32_t n, uint32_t dur)
{
    uint32_t N = 12u + ((uint32_t)p[P_E7] >> 4), i, k, cut = (uint32_t)p[P_E3];
    pl_node_t *q;
    if (N > 20u)
        N = 20u;
    if (!P->s[2] || P->s[3] != (int32_t)N) {           /* arma la red: cada grado, dos o tres vecinos al azar */
        for (i = 0; i < N; i++)
            for (k = 0; k < 3u; k++)
                P->u.g[i][k] = (int8_t)((k < 2u || (pl_rnd(P) >> 31)) ? (int32_t)pl_ru(P, N) : -1);
        P->s[2] = 1;
        P->s[3] = (int32_t)N;
        P->s[0] = 0;
        for (i = 0; i < PL_MAX; i++) {                 /* los grados, en un óvalo */
            P->n[i].live = 0;
            P->n[i].nb[0] = P->n[i].nb[1] = P->n[i].nb[2] = PL_NONE;
        }
    }
    if ((pl_rnd(P) >> 25) < cut) {                     /* CORT: corta una conexión y abre otra, sin jerarquía */
        uint32_t a = pl_ru(P, N);
        P->u.g[a][pl_ru(P, 3)] = (int8_t)pl_ru(P, N);
    }
    for (k = 0, i = 0; i < 6u && k == 0; i++) {        /* el paso: a un vecino */
        int8_t nx = P->u.g[(uint32_t)P->s[0] % N][pl_ru(P, 3)];
        if (nx >= 0) {
            P->s[0] = nx;
            k = 1;
        }
    }
    {
        uint32_t d = (uint32_t)P->s[0] % N;
        uint16_t ang = (uint16_t)(d * 65536u / N);
        q = pl_note(P, n, (int32_t)d + 2, 95, dur, (pl_cos(ang) * 55) >> 5 >> 0, (pl_sin(ang) * 50) >> 5,
                    PL_NONE);
        if (q) {
            for (k = 0; k < 3u; k++)                    /* para dibujar: las aristas del grado (a otros nodos vivos) */
                q->nb[k] = PL_NONE;
            q->gx = (int32_t)d;
            for (i = 0; i < n; i++) {
                pl_node_t *o = &P->n[i];
                if (o != q && o->live && o->gx >= 0 && o->gx < (int32_t)N)
                    for (k = 0; k < 3u; k++)
                        if (P->u.g[d][k] == o->gx && q->nb[k] == PL_NONE)
                            q->nb[k] = (uint8_t)i;
            }
            q->par = (uint8_t)P->s[1];
            P->s[1] = pl_idx(P, q);
        }
    }
}

/* ------------------------------------------------------------------ los demás procesos --- */
/* el ciclo del día: o = 0 (noche) .. 32767 (mediodía), periodo en pulsos */
static int32_t pl_day(const pl_part_t *P)            /* (plant_block hace avanzar dph con el ciclo del proceso) */
{
    return 16384 - (pl_cos(P->dph >> 16) >> 1);
}

static void pl_step(pl_part_t *P, const int16_t *p, uint32_t n, uint32_t self, uint32_t dur)
{
    int32_t A = p[P_E3], B = p[P_E7], i;
    pl_node_t *q;
    switch (P->proc) {
    case PL_FILO: pl_filo(P, p, n, dur * 2u); break;
    case PL_PLANTA: pl_planta(P, p, n, dur); break;
    case PL_RAIZ: pl_raiz(P, p, n, dur); break;
    case PL_NICTI: {                                    /* s[0] el número de notas que sonaron (dibujo) */
        int32_t o = pl_day(P);                          /* CICL: un día de 2 .. 32 compases (plant_block) */
        int32_t max = 1 + (A * 7 >> 7), cnt = 1 + (((max - 1) * o) >> 15);
        pl_node_t *stem = pl_note(P, n, 0, 28, dur, 0, -50000, PL_NONE);   /* el pecíolo: una nota grave y suave */
        uint32_t sp = stem ? pl_idx(P, stem) : PL_NONE;
        for (i = 0; i < cnt; i++) {                     /* los folíolos, abiertos en abanico según la hora */
            uint16_t ang = (uint16_t)(16384u + (uint32_t)((i - cnt / 2) * (2500 + (o >> 2))));
            pl_note(P, n, 2 + i * 2, 45 + (60 * o >> 15), dur, (pl_cos(ang) * 50000) >> 15,
                    -50000 + ((pl_sin(ang) * 45000) >> 14), sp);
        }
        P->s[0] = cnt;
        break;
    }
    case PL_SEMIL: {                                    /* Poisson: en cada paso 0, 1 o 2 semillas */
        int32_t c = 4 + (B * 20 >> 7), sp = 1 + (A * 8 >> 7), k2 = (int32_t)pl_ru(P, 8);
        int32_t m = k2 < 3 ? 0 : k2 < 7 ? 1 : 2;
        for (i = 0; i < m; i++) {
            int32_t g = (pl_rs(P) + pl_rs(P) + pl_rs(P)) / 3;   /* casi gaussiana */
            int32_t d = c + ((g * sp) >> 14);
            pl_note(P, n, d, 60 + (int32_t)pl_ru(P, 50), dur, (g * 3) >> 1, pl_ydeg(d) + (pl_rs(P) >> 3), PL_NONE);
        }
        break;
    }
    case PL_RAMIF: {                                    /* s[0] el nivel, s[1] la rama del nivel */
        int32_t lv = 2 + (B * 5 >> 7), intv = 1 + (A * 6 >> 7), d = 6, w, par = PL_NONE;
        uint32_t br = (uint32_t)P->s[1], j;
        for (j = 0; j < (uint32_t)P->s[0]; j++)         /* el camino de la rama: arriba o abajo en cada nivel */
            d += ((br >> (P->s[0] - 1 - (int32_t)j)) & 1u ? intv : -intv) * (P->s[0] - (int32_t)j) / (P->s[0] ? P->s[0] : 1);
        w = 1 << P->s[0];
        for (j = 0; j < n; j++) {                       /* el padre: la rama del nivel anterior */
            pl_node_t *o = &P->n[j];
            if (o->live && o->gx == ((P->s[0] - 1) << 8 | (int32_t)(br >> 1)))
                par = (int32_t)j;
        }
        q = pl_note(P, n, d + 6, 100 - P->s[0] * 8, dur, -60000 + P->s[0] * 120000 / lv,
                    (((int32_t)br * 2 + 1 - w) * 56000) / w, (uint32_t)par);
        if (q)
            q->gx = P->s[0] << 8 | (int32_t)br;
        if (++P->s[1] >= w) {
            P->s[1] = 0;
            if (++P->s[0] >= lv) {
                P->s[0] = 0;
                for (j = 0; j < PL_MAX; j++)
                    P->n[j].live = 0;
            }
        }
        break;
    }
    case PL_CIRCUM: {                                   /* s[0] el ángulo, s[1] el eje (x256), s[2] el nodo anterior */
        int32_t R = 1 + (A * 6 >> 7), d;
        P->s[0] += 9000;
        P->s[1] += 40 + B;
        if (P->s[1] > 20 * 256) {
            P->s[1] = 0;
            for (i = 0; i < PL_MAX; i++)
                P->n[i].live = 0;
            P->s[2] = PL_NONE;
        }
        d = (P->s[1] >> 8) + ((R * pl_sin((uint32_t)P->s[0])) >> 15);
        q = pl_note(P, n, d + 2, 90, dur, (pl_sin((uint32_t)P->s[0]) * 30000) >> 15,
                    -60000 + P->s[1] * 22, (uint32_t)(P->s[2] < 0 ? PL_NONE : P->s[2]));
        P->s[2] = q ? pl_idx(P, q) : PL_NONE;
        break;
    }
    case PL_POLIN: {                                    /* s[0..5] flor A, s[6..11] flor B, s[12] dónde está, s[13] polen */
        int32_t sz = 2 + (B * 4 >> 7), f, base;
        if (!P->s[14]) {
            for (i = 0; i < 6; i++) {
                P->s[i] = 2 + i * 3;
                P->s[6 + i] = 5 + i * 2 + (i & 1);
            }
            P->s[13] = -1;
            P->s[14] = 1;
        }
        f = P->s[12] & 1;
        base = f * 6;
        if (P->s[13] >= 0 && (int32_t)(pl_rnd(P) >> 25) < A)   /* POLE: deja el polen */
            P->s[base + (int32_t)pl_ru(P, (uint32_t)sz)] = P->s[13];
        for (i = 0; i < sz; i++) {
            uint16_t ang = (uint16_t)(i * 65536 / sz);
            pl_note(P, n, P->s[base + i], 85, dur, (f ? 36000 : -36000) + ((pl_cos(ang) * 14000) >> 15),
                    ((pl_sin(ang) * 14000) >> 15), PL_NONE);
        }
        P->s[13] = P->s[base + (int32_t)pl_ru(P, (uint32_t)sz)];   /* se lleva una nota */
        q = pl_note(P, n, P->s[13] + 16, 45, dur >> 2, (f ? 36000 : -36000), 30000, PL_NONE);   /* el polinizador */
        P->s[12]++;
        break;
    }
    case PL_HOJAR: {                                    /* las hojas son nodos: gx = su posición en grados x256 */
        uint32_t j;
        if ((int32_t)(pl_rnd(P) >> 25) < B / 2 + 8) {    /* ESTA: cuántas caen */
            q = pl_note(P, n, 22, 70, dur, pl_rs(P) * 2, pl_ydeg(22), PL_NONE);
            if (q) {
                q->gx = 22 * 256;
                q->aux = (uint16_t)pl_rnd(P);
            }
        }
        for (j = 0; j < n; j++) {                       /* las que caen: bajan un poco, se mecen, suenan */
            pl_node_t *o = &P->n[j];
            if (!o->live || o->age == 0)
                continue;
            o->gx -= 80 + (int32_t)pl_ru(P, 300);
            o->aux = (uint16_t)(o->aux + 9000u);
            if (o->gx < 0) {
                o->live = 0;
                continue;
            }
            o->deg = (int16_t)(o->gx >> 8);
            o->y = pl_ydeg(o->deg);
            o->x = clamp(o->x + ((pl_sin(o->aux) * A) >> 9), -62000, 62000);
            if ((pl_rnd(P) >> 30) == 0u) {               /* a ratos suena al mecerse */
                o->env = o->vel = (int16_t)(50 * 258);
                o->hold = (uint16_t)((dur * P->bpp) >> 6);
            }
        }
        break;
    }
    case PL_ANILLO: {                                   /* s[0] el clima (Q15), s[1] el año */
        int32_t w, cnt, max = 1 + (B * 7 >> 7);
        P->s[0] += ((pl_rs(P) * A) >> 7) - ((P->s[0] - 16384) >> 3);
        P->s[0] = clamp(P->s[0], 0, 32767);
        w = P->s[0];
        cnt = 1 + (((max - 1) * w) >> 15);
        P->s[1]++;
        for (i = 0; i < cnt; i++) {
            uint16_t ang = (uint16_t)((uint32_t)(i * 65536 / cnt) + (uint32_t)P->s[1] * 7000u);
            int32_t r = 6000 + ((P->s[1] % 12) * 4800);
            pl_note(P, n, i * 2 + 2, 45 + (70 * w >> 15), (dur * (uint32_t)(11 + (19 * w >> 15))) >> 3,
                    (pl_cos(ang) * r) >> 15, (pl_sin(ang) * r) >> 15, PL_NONE);
        }
        break;
    }
    case PL_FOTOT: {                                    /* s[0] el ápice (Q15); la luz recorre el registro */
        int32_t light = 16384 + (pl_sin((uint32_t)(P->k * (300u + (uint32_t)B * 40u))) >> 1);
        int32_t resp = 400 + A * 120, d;
        P->s[0] += (((light - P->s[0]) >> 7) * resp) >> 8;
        P->ly = (light << 1) - 32767;
        P->lx = 52000;
        d = (P->s[0] * 22) >> 15;
        q = pl_note(P, n, d + 1, 80, dur, 52000, pl_ydeg(d + 1), (uint32_t)(P->s[1] < 0 ? PL_NONE : P->s[1]));
        P->s[1] = q ? pl_idx(P, q) : PL_NONE;
        if ((pl_rnd(P) >> 25) < 30u)                    /* a veces brota una hoja */
            pl_note(P, n, d + 2, 55, dur >> 2, 52000, pl_ydeg(d + 2) + 2400, PL_NONE);
        break;
    }
    case PL_RAICES: {                                   /* s[0..7] las puntas (grado; -1 = ninguna), s[8..15] su nodo */
        int32_t max = 1 + (B * 8 >> 7), any = 0;
        if (max > 8)
            max = 8;
        for (i = 0; i < 8; i++)
            if (P->s[i] > 0)
                any = 1;
        if (!any) {
            P->s[0] = 23;
            P->s[8] = PL_NONE;
        }
        for (i = 0; i < 8; i++) {
            int32_t j;
            if (P->s[i] <= 0)
                continue;
            P->s[i] -= (int32_t)pl_ru(P, 3);
            if (P->s[i] <= 0) {
                P->s[i] = 0;
                continue;
            }
            q = pl_note(P, n, P->s[i], 100 - i * 8, dur, 52000, pl_ydeg(P->s[i]), (uint32_t)P->s[8 + i]);
            P->s[8 + i] = q ? pl_idx(P, q) : PL_NONE;
            if ((int32_t)(pl_rnd(P) >> 25) < A)         /* RAMA: se bifurca */
                for (j = 0; j < max; j++)
                    if (P->s[j] <= 0) {
                        P->s[j] = P->s[i];
                        P->s[8 + j] = P->s[8 + i];
                        break;
                    }
        }
        break;
    }
    case PL_FLOR: {                                     /* s[0] el paso del ciclo, s[1] pétalos que faltan caer */
        int32_t C = 8 + (A >> 1), petals = 3 + (B * 6 >> 7), u;
        P->s[0]++;
        u = (P->s[0] << 15) / C;
        if (P->s[1] > 0) {                              /* caen los pétalos */
            int32_t d = 6 + P->s[1];
            pl_note(P, n, d, 40 + P->s[1] * 6, dur, pl_rs(P), pl_ydeg(d) - (petals - P->s[1]) * 2000, PL_NONE);
            P->s[1]--;
        } else if (P->s[0] >= C) {                      /* se abre: arpegio en ángulo áureo */
            for (i = 0; i < petals; i++) {
                uint16_t ang = (uint16_t)(i * PL_GOLD);
                q = pl_note(P, n, 8 + (int32_t)((ang >> 12) & 15u), 110, dur * 3u, (pl_cos(ang) * 40000) >> 15,
                            (pl_sin(ang) * 40000) >> 15, PL_NONE);
                if (q) {                                /* se despliega: cada pétalo un poco después */
                    q->timer = (uint16_t)(1u + (uint32_t)i * (P->bpp >> 3));
                    q->env = 0;
                    q->hold = (uint16_t)(q->hold + q->timer);
                }
            }
            P->s[0] = 0;
            P->s[1] = petals;
        } else if ((int32_t)(pl_rnd(P) >> 17) < 2000 + (u * u >> 15)) {   /* los botones se hinchan */
            pl_note(P, n, 4 + (int32_t)pl_ru(P, 8), 50 + (u >> 9), dur >> 1, pl_rs(P) >> 2, pl_rs(P) >> 2, PL_NONE);
        }
        break;
    }
    case PL_ESTOMA: {                                   /* la apertura mueve el timbre (P->wmod, en el bloque) */
        if (P->wmod > 20000 && (int32_t)(pl_rnd(P) >> 25) < B) {
            int32_t d = 4 + (int32_t)pl_ru(P, 12);
            pl_note(P, n, d, 50, dur * 4u, pl_rs(P) * 2, pl_rs(P) * 2, PL_NONE);
        }
        break;
    }
    case PL_ZARCI: {                                    /* s[0] el radio (x256), s[1] el ángulo, s[2] el nodo anterior */
        int32_t ax = 10, d, chord = 2 + (B * 4 >> 7);
        P->s[0] += 40 + A * 3;
        P->s[1] += 12000;
        if (P->s[0] > 9 * 256 || (P->s[0] > 3 * 256 && (pl_rnd(P) >> 27) == 0u)) {   /* se agarra: acorde sostenido */
            for (i = 0; i < chord; i++)
                pl_note(P, n, ax - 2 + i * 3, 95, dur * 6u, (i - chord / 2) * 12000, pl_ydeg(ax - 2 + i * 3), PL_NONE);
            P->s[0] = 0;
            P->s[2] = PL_NONE;
            break;
        }
        d = ax + (((P->s[0] >> 8) * pl_sin((uint32_t)P->s[1])) >> 15);
        q = pl_note(P, n, d, 80, dur, (P->s[0] * pl_cos((uint32_t)P->s[1])) >> 11, pl_ydeg(d),
                    (uint32_t)(P->s[2] < 0 ? PL_NONE : P->s[2]));
        P->s[2] = q ? pl_idx(P, q) : PL_NONE;
        break;
    }
    case PL_MUSGO: {                                    /* s[0] las células vivas (bits, grados 0..23) */
        uint32_t cells = (uint32_t)P->s[0], span = 8u + ((uint32_t)B * 16u >> 7), j;
        uint32_t full = span >= 32u ? 0xFFFFFFFFu : (1u << span) - 1u;
        int32_t grow = 1 + (A * 3 >> 7);
        if (!cells || (cells & full) == full)
            cells = 1u << (span / 2u);                  /* se seca y vuelve a empezar */
        for (i = 0; i < grow; i++) {                   /* una célula viva al azar coloniza a una vecina */
            uint32_t c = pl_ru(P, span), nb2, m;
            for (m = 0; m < span && !((cells >> c) & 1u); m++)
                c = c + 1u < span ? c + 1u : 0u;
            nb2 = (pl_rnd(P) >> 31) ? c + 1u : c - 1u;
            if (nb2 < span)
                cells |= 1u << nb2;
        }
        P->s[0] = (int32_t)cells;
        for (j = 0; j < span; j++)
            if ((cells >> j) & 1u)
                pl_note(P, n, (int32_t)j + 2, 28 + (int32_t)pl_ru(P, 16), dur, -60000 + (int32_t)(j * 120000u / span),
                        -40000 + (pl_rs(P) >> 2), PL_NONE);
        break;
    }
    case PL_GERMIN: {                                   /* s[0..7] plántulas: base | altura << 8 | vivo << 16; s[8..15] nodo */
        int32_t hmax = 2 + (B * 18 >> 7), slots = 4;
        if ((int32_t)(pl_rnd(P) >> 25) < A)              /* BROT: una semilla despierta */
            for (i = 0; i < 8; i++)
                if (!(P->s[i] >> 16)) {
                    P->s[i] = (int32_t)pl_ru(P, 10) | 1 << 16;
                    P->s[8 + i] = PL_NONE;
                    break;
                }
        for (i = 0; i < 8 && slots; i++) {
            int32_t base, h, d;
            if (!(P->s[i] >> 16))
                continue;
            base = P->s[i] & 0xFF;
            h = (P->s[i] >> 8) & 0xFF;
            d = base + h;
            q = pl_note(P, n, d + 2, 55 + (60 * h) / (hmax ? hmax : 1), dur, -56000 + i * 16000, -60000 + h * 110000 / (hmax ? hmax : 1),
                        (uint32_t)P->s[8 + i]);
            P->s[8 + i] = q ? pl_idx(P, q) : PL_NONE;
            if (++h > hmax)
                P->s[i] = 0;
            else
                P->s[i] = base | h << 8 | 1 << 16;
        }
        break;
    }
    case PL_RIZOMA: pl_rizoma(P, p, n, dur); break;
    default: break;                                     /* SAVIA, RELOJ, MICO: corren por bloque */
    }
}

/* SAVIA y RELOJ: su densidad cambia con el día, así que deciden en cada octavo de pulso */
static void pl_tick8(pl_part_t *P, const int16_t *p, uint32_t n)
{
    int32_t B = p[P_E7], o;
    pl_node_t *q;
    if (P->proc == PL_SAVIA) {
        o = pl_day(P);                                  /* DIA: el ciclo, 2 .. 32 compases (plant_block) */
        P->lx = 52000;
        P->ly = (o << 1) - 32767;
        if ((int32_t)(pl_rnd(P) >> 17) < ((1 + (B >> 4)) * o) >> 3) {   /* TASA al mediodía */
            P->s[0]++;
            q = pl_note(P, n, 2 + P->s[0] % 20, 50 + (60 * o >> 15), 6, 52000, pl_ydeg(2 + P->s[0] % 20),
                        (uint32_t)(P->s[1] < 0 ? PL_NONE : P->s[1]));
            P->s[1] = q ? pl_idx(P, q) : PL_NONE;
        }
    } else {                                            /* RELOJ: de noche pocas notas graves y lentas; a mediodía, más y más agudas */
        int32_t d;                                      /* CICL: el día, 4 .. 64 compases (plant_block) */
        o = pl_day(P);
        P->lx = (pl_cos((P->dph >> 16) + 32768u) * 52000) >> 15;
        P->ly = (o << 1) - 32767;
        if ((int32_t)(pl_rnd(P) >> 17) < 200 + (((B + 16) * o) >> 6)) {
            d = 1 + ((o * 18) >> 15) + (int32_t)pl_ru(P, 4);
            pl_note(P, n, d, 45 + (60 * o >> 15), o > 16384 ? 8 : 48, 52000, pl_ydeg(d), PL_NONE);
        }
    }
}

/* ------------------------------------------------------------------ el bloque --- */
static const uint16_t PL_STEP64[6] = {256, 128, 64, 32, 16, 8};    /* CREC: 4, 2, 1, 1/2, 1/4, 1/8 de pulso */
static void plant_block(track_t *t)
{
    pl_part_t *P = pl_of(t);
    const int16_t *p = t->p;
    uint32_t want, n, i, nv = 0, dsh, bpm = PL_BPM(), now, step, self = (uint32_t)(t - trk) % NPART, guard;
    if (bpm < 20u)
        bpm = 120u;
    if (!P->init) {
        if (!P->rng)
            P->rng = 0x7EAF5EED + (int32_t)self * 104729;
        P->ncap = 24;
        P->init = 1;
        P->self = (uint8_t)self;
        P->proc = 0xFF;
    }
    if (P->proc != (uint8_t)clamp(p[P_E0], 0, PL_NPROC - 1)) {   /* otro proceso: vuelve a crecer */
        P->proc = (uint8_t)clamp(p[P_E0], 0, PL_NPROC - 1);
        for (i = 0; i < PL_MAX; i++) {
            pl_node_t *q = &P->n[i];
            q->live = 0;
            q->env = 0;
            q->hold = 0;
            q->timer = q->refr = 0;
            q->par = PL_NONE;
            q->nb[0] = q->nb[1] = q->nb[2] = PL_NONE;
            q->gx = -1;
        }
        for (i = 0; i < 24; i++)
            P->s[i] = 0;
        P->nnet = 0;
        P->built = 0;
        P->k = 0;
        P->dph = 0x40000000u;                           /* el día empieza al amanecer */
        if (P->proc == PL_FOTOT || P->proc == PL_SAVIA)
            P->s[1] = -1;
        if (P->proc == PL_CIRCUM || P->proc == PL_ZARCI)
            P->s[2] = -1;
        if (P->proc == PL_PLANTA) {
            pl_lsys_seed(P);
            pl_lsys_grow(P);
            pl_turtle_home(P);
        }
        if (P->proc == PL_RAIZ)
            P->s[1] = PL_NONE;
        if (P->proc == PL_ANILLO)
            P->s[0] = 16384;
        P->lx = P->ly = 0;
    }
    pl_govern(P);
    want = PL_MIN + (((uint32_t)p[P_E1] * (PL_MAX - PL_MIN)) >> 7);
    n = want < P->ncap ? want : P->ncap;
    for (i = 0; i < NVOICE; i++)
        nv += t->v[i].active;
    P->nv = (uint8_t)(nv ? nv : 1u);
    P->bpp = 82688u / bpm;                              /* bloques por pulso */
    {                                                   /* el ciclo del día de los procesos que lo usan, en pulsos */
        uint32_t L = P->proc == PL_NICTI ? 8u + ((uint32_t)p[P_E7] * 120u >> 7)
                   : P->proc == PL_SAVIA || P->proc == PL_ESTOMA ? 8u + ((uint32_t)p[P_E3] * 120u >> 7)
                   : P->proc == PL_RELOJ ? 16u + ((uint32_t)p[P_E3] * 240u >> 7) : 0u;
        if (L)
            P->dph += 0xFFFFFFFFu / (L * P->bpp);       /* una división por bloque */
    }
    /* el reloj: 1/64 de pulso, Q16 (bpm x 64 x 65536 / (60 x 1378,125) = bpm x 50,72) */
    P->clk += bpm * 3246u >> 6;
    now = P->clk >> 16;
    step = PL_STEP64[clamp(p[P_E2] * 6 >> 7, 0, 5)];
    if (P->proc == PL_MICO)
        pl_mico(P, p, n, self);
    else if (P->proc == PL_SAVIA || P->proc == PL_RELOJ) {
        for (guard = 0; (int32_t)(now - P->next) >= 0 && guard < 2u; guard++) {
            P->next += 8u;
            pl_tick8(P, p, n);
        }
        if ((int32_t)(now - P->next) > 64)
            P->next = now;
    } else {
        for (guard = 0; (int32_t)(now - P->next) >= 0 && guard < 2u; guard++) {
            P->next += step;
            pl_step(P, p, n, self, step);
            P->k++;
        }
        if ((int32_t)(now - P->next) > 512)
            P->next = now;
    }
    if (P->proc == PL_ESTOMA) {                         /* la respiración de la hoja: RESP 2 .. 32 compases */
        int32_t o = pl_day(P);
        P->wmod = 3000 + ((o * 29767) >> 15);
        P->lx = 0;
        P->ly = (o << 1) - 32767;
    } else
        P->wmod = 32767;
    PL_COUNT(1, n);
    /* las envolventes: sostiene hold bloques, luego cae (DECA: 2 ms .. 3 s); los nodos de la partitura corren */
    dsh = 3u + ((uint32_t)p[P_E6] >> 4);
    for (i = 0; i < n; i++) {
        pl_node_t *q = &P->n[i];
        if (q->age < 0xFFFFu)
            q->age++;
        if (q->timer && P->proc != PL_MICO) {          /* un ataque demorado (FLOR) */
            if (!--q->timer)
                q->env = q->vel;
            else
                q->env = 0;
        }
        if (q->hold)
            q->hold--;
        else {
            q->env -= (q->env >> dsh) + (q->env ? 1 : 0);
            if (q->env < 0)
                q->env = 0;
        }
        if (PL_VIEW[P->proc] & 2u) {
            if (q->x > -65535)
                q->x = q->x - 40 < -65535 ? -65535 : q->x - 40;   /* ~2 s de lado a lado */
            else if (!q->env)
                q->live = 0;
        }
        if (!q->env && !q->hold && (P->proc == PL_NICTI || P->proc == PL_POLIN ||
                                    P->proc == PL_ANILLO || P->proc == PL_FLOR || P->proc == PL_ESTOMA ||
                                    P->proc == PL_MUSGO || P->proc == PL_ZARCI || P->proc == PL_MICO))
            q->live = 0;
        q->tgt = q->env;
    }
    for (; i < P->nlive; i++) {
        pl_node_t *q = &P->n[i];
        q->tgt = q->a - (q->a >> 2);
        if (q->tgt < 64)
            q->tgt = 0;
        q->live = 0;
    }
    i = P->nlive > n ? P->nlive : n;
    while (i > n && P->n[i - 1u].a == 0 && P->n[i - 1u].tgt == 0)
        i--;
    P->nlive = (uint16_t)i;
}

static void plant_note_on(track_t *t, voice_t *v)
{
    pl_part_t *P = pl_of(t);
    v->s[0] = 0;
    if (!P->init)
        return;
    if (P->proc == PL_MICO && P->built)                 /* la nota entra a la red por un nodo */
        pl_net_fire(P, ((uint32_t)v->note * 7u) % P->built, 32767, 0);
    else
        P->next = P->clk >> 16;                         /* el próximo paso, ya */
}

/* TIMB (E5): el timbre de cada motivo. Ocho voces baratas, de una o dos lecturas de SINE por muestra, todas con la
 * misma envolvente por nodo; ESTOMA (P->wmod) abre y cierra su brillo. Los parciales en 1/192 de octava. */
enum { PL_T_SENO, PL_T_MADERA, PL_T_CAMPANA, PL_T_VIDRIO, PL_T_CUERDA, PL_T_SOPLO, PL_T_HOJA, PL_T_AGUA, PL_NTIMB };
static const char *const N_PL_TIMB[PL_NTIMB] = {"SENO", "MADERA", "CAMPAN", "VIDRIO", "CUERDA", "SOPLO", "HOJA", "AGUA"};
static const int16_t PL_T_RATIO[PL_NTIMB] = {0, PL_WOOD, 347, 467, 0, 0, 192, 0};    /* x2,756 x3,5 x5,4 x2 */

static void plant_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    pl_part_t *P = pl_of(t);
    const int16_t *spec = PL_SPEC[clamp(t->p[P_E4], 0, PL_NSPEC - 1)];
    uint32_t tb = (uint32_t)clamp(t->p[P_E5], 0, PL_NTIMB - 1);
    int32_t bright = P->wmod;                           /* Q15: 32767 salvo en ESTOMA */
    int32_t acc[CTL];
    uint32_t rank = 0, i, k, cnt = 0, nv = P->nv;
    int32_t g;
    for (i = 0; i < NVOICE && &t->v[i] != v; i++)
        rank += t->v[i].active;
    if (nv < rank + 1u)
        nv = rank + 1u;
    for (k = 0; k < n; k++)
        acc[k] = 0;
    for (i = rank; i < P->nlive; i += nv) {
        pl_node_t *q = &P->n[i];
        uint32_t inc, inc2 = 0, ph, ph2;
        int32_t amp, da, w = 0, pt, d = q->deg + 64, oct = (d >> 4) - 4, e;
        if (!q->a && !q->tgt)
            continue;
        pt = m->pitch16 + spec[d & 15] + oct * 192;
        inc = pitch_inc((uint32_t)clamp(pt, 0, 2047));
        if (inc > PL_INC_MAX || pt > 2047) {
            q->a = q->tgt = 0;
            continue;
        }
        e = q->a;                                       /* la envolvente del nodo, Q15, al empezar el bloque */
        if (PL_T_RATIO[tb]) {
            int32_t p2 = pt + PL_T_RATIO[tb];
            inc2 = p2 > 2047 ? 0 : pitch_inc((uint32_t)p2);
            if (inc2 > PL_INC_MAX)
                inc2 = 0;
        }
        switch (tb) {                                   /* w: cuánto carácter, Q15 */
        case PL_T_MADERA: w = (((e * e) >> 15) * 26000) >> 15; break;            /* el parcial cae antes */
        case PL_T_CAMPANA: w = (e * 30000) >> 15; break;                         /* índice de FM, x e */
        case PL_T_VIDRIO: w = 18000; break;                                      /* parcial alto, sostenido */
        case PL_T_CUERDA: w = (((e * e) >> 15) * 32000) >> 15; break;            /* ataque brillante */
        case PL_T_SOPLO: w = 9000 + ((e * 6000) >> 15); break;                   /* aliento */
        case PL_T_HOJA: w = 26000; break;                                        /* ruido en banda */
        case PL_T_AGUA: inc += (uint32_t)(((inc >> 4) * (uint32_t)(32767 - e)) >> 13); break; /* sube al caer */
        default: break;
        }
        w = (w * (bright >> 7)) >> 8;
        if (!inc2 && (tb == PL_T_MADERA || tb == PL_T_VIDRIO || tb == PL_T_HOJA))
            w = 0;
        amp = q->a * 256;
        da = ((q->tgt - q->a) * 256) >> CTL_LOG2;
        if (tb == PL_T_HOJA && da < 0)
            da += da >> 1;                              /* la hoja cruje y se apaga antes */
        ph = q->ph;
        ph2 = q->ph2;
        switch (tb) {
        case PL_T_CAMPANA: {                            /* FM 2 op., moduladora x3,5 */
            int32_t ix = w * 3;                         /* hasta ~ 1,4 rad */
            for (k = 0; k < n; k++) {
                int32_t s;
                ph += inc;
                ph2 += inc2;
                amp += da;
                s = SINE[(ph + (uint32_t)(SINE[ph2 >> 22] * (ix >> 6) * 32)) >> 22];
                acc[k] += (s * (amp >> 8)) >> 16;
            }
            break;
        }
        case PL_T_CUERDA: {                             /* FM 1:1 con índice que cae: pulsada */
            int32_t ix = w * 3;
            for (k = 0; k < n; k++) {
                int32_t s;
                ph += inc;
                amp += da;
                s = SINE[(ph + (uint32_t)(SINE[ph >> 22] * (ix >> 6) * 24)) >> 22];
                acc[k] += (s * (amp >> 8)) >> 16;
            }
            break;
        }
        case PL_T_SOPLO:
        case PL_T_HOJA: {                               /* ruido por un seno: banda alrededor del parcial */
            int32_t rs = P->nz ? P->nz : 0x2545F491;
            uint32_t *pp = tb == PL_T_HOJA ? &ph2 : &ph;
            uint32_t ii = tb == PL_T_HOJA ? inc2 : 0;
            int32_t zf = 0, zs = tb == PL_T_HOJA ? 1 : 3;     /* SOPLO: banda estrecha; HOJA: ancha */
            for (k = 0; k < n; k++) {
                int32_t s, z;
                ph += inc;
                ph2 += ii;
                amp += da;
                zf += (((int32_t)noise32(&rs) >> 17) - zf) >> zs;
                z = zf << (zs - 1 > 0 ? 1 : 0);         /* ±16383, la banda estrecha recupera nivel */
                s = SINE[ph >> 22];
                s = ((s * (32767 - w)) >> 15) + ((((z * SINE[*pp >> 22]) >> 14) * w) >> 15);
                acc[k] += (s * (amp >> 8)) >> 16;
            }
            P->nz = rs;
            break;
        }
        default:                                        /* SENO, MADERA, VIDRIO, AGUA: seno + parcial */
            for (k = 0; k < n; k++) {
                int32_t s;
                ph += inc;
                ph2 += inc2;
                amp += da;
                s = SINE[ph >> 22] + ((SINE[ph2 >> 22] * w) >> 15);
                acc[k] += (s * (amp >> 8)) >> 16;
            }
            break;
        }
        q->ph = ph;
        q->ph2 = ph2;
        q->a = q->tgt;
        cnt++;
        PL_COUNT(0, n);
    }
    if (!cnt)
        return;
    g = (PL_NORM[cnt > 64u ? 64u : cnt] * 5) >> 5;
    for (k = 0; k < n; k++)
        out[k] += voice_amp(softclip(clamp((clamp(acc[k], -699000, 699000) * g) >> 11, -65535, 65535)), m, k);
}

/* A (E3) y B (E7): su nombre y su sentido en cada proceso */
static const param_desc_t PL_DA[PL_NPROC] = {
    {"ANGL", F_INT, 0, 127, 64, 0, 0}, {"INTV", F_INT, 0, 127, 40, 0, 0}, {"RAMA", F_PCT, 0, 127, 40, 0, 0},
    {"TRAN", F_INT, 0, 127, 64, 0, 0}, {"NOTA", F_INT, 0, 127, 80, 0, 0}, {"DISP", F_PCT, 0, 127, 60, 0, 0},
    {"INTV", F_INT, 0, 127, 40, 0, 0}, {"GIRO", F_PCT, 0, 127, 64, 0, 0}, {"POLE", F_PCT, 0, 127, 50, 0, 0},
    {"MECE", F_PCT, 0, 127, 64, 0, 0}, {"CLIM", F_PCT, 0, 127, 64, 0, 0}, {"DIA", F_PCT, 0, 127, 40, 0, 0},
    {"RESP", F_PCT, 0, 127, 64, 0, 0}, {"RAMA", F_PCT, 0, 127, 20, 0, 0}, {"CICL", F_PCT, 0, 127, 32, 0, 0},
    {"RESP", F_PCT, 0, 127, 30, 0, 0}, {"GIRO", F_PCT, 0, 127, 50, 0, 0}, {"COLO", F_PCT, 0, 127, 40, 0, 0},
    {"BROT", F_PCT, 0, 127, 30, 0, 0}, {"CORT", F_PCT, 0, 127, 20, 0, 0}, {"CICL", F_PCT, 0, 127, 40, 0, 0}};
static const param_desc_t PL_DB[PL_NPROC] = {
    {"EDAD", F_PCT, 0, 127, 64, 0, 0}, {"GENE", F_PCT, 0, 127, 96, 0, 0}, {"PROF", F_PCT, 0, 127, 64, 0, 0},
    {"FUEN", F_INT, 0, 127, 0, 0, 0}, {"CICL", F_PCT, 0, 127, 30, 0, 0}, {"CENT", F_INT, 0, 127, 50, 0, 0},
    {"NIVE", F_PCT, 0, 127, 80, 0, 0}, {"SUBE", F_PCT, 0, 127, 40, 0, 0}, {"FLOR", F_PCT, 0, 127, 64, 0, 0},
    {"ESTA", F_PCT, 0, 127, 50, 0, 0}, {"NOTA", F_INT, 0, 127, 80, 0, 0}, {"TASA", F_PCT, 0, 127, 64, 0, 0},
    {"HOJA", F_PCT, 0, 127, 30, 0, 0}, {"PUNT", F_PCT, 0, 127, 64, 0, 0}, {"PETA", F_PCT, 0, 127, 64, 0, 0},
    {"APER", F_PCT, 0, 127, 40, 0, 0}, {"ACOR", F_PCT, 0, 127, 64, 0, 0}, {"AREA", F_PCT, 0, 127, 64, 0, 0},
    {"ALTU", F_PCT, 0, 127, 50, 0, 0}, {"CONE", F_PCT, 0, 127, 50, 0, 0}, {"LUZ", F_PCT, 0, 127, 64, 0, 0}};
static const param_desc_t *plant_desc(const track_t *t, uint32_t k)
{
    uint32_t pr = (uint32_t)clamp(t->p[P_E0], 0, PL_NPROC - 1);
    if (k == 3u)
        return &PL_DA[pr];
    if (k == 7u)
        return &PL_DB[pr];
    return 0;
}

static const preset_t PLANT_PRESETS[] = {
    /* name, {PROC, NODO, CREC, A, ESPC, TIMB, DECA, B}, {A D S R}, fenv, mono */
    {"GIRASOL", {PL_FILO, 64, 80, 64, 0, 2, 70, 60}, {0, 90, 127, 80}, 0, 0, FX(0, 20, 30, 70), PAT(0)},
    {"HELECHO", {PL_PLANTA, 80, 80, 40, 3, 1, 60, 127}, {0, 90, 127, 80}, 0, 0, FX(0, 0, 40, 60), PAT(0)},
    {"RAIZ", {PL_RAIZ, 70, 60, 50, 3, 4, 70, 80}, {0, 90, 127, 80}, 0, 0, FX(0, 0, 30, 60), PAT(0)},
    {"MICORRIZA", {PL_MICO, 96, 60, 70, 1, 3, 75, 0}, {0, 90, 127, 90}, 0, 0, FX(0, 0, 30, 70), PAT(0)},
    {"NICTINASTIA", {PL_NICTI, 40, 20, 90, 3, 5, 100, 30}, {40, 90, 127, 100}, 0, 0, FX(0, 40, 10, 90), PAT(0)},
    {"SEMILLAS", {PL_SEMIL, 64, 80, 50, 0, 7, 60, 50}, {0, 90, 127, 70}, 0, 0, FX(0, 0, 40, 70), PAT(0)},
    {"RAMIFICA", {PL_RAMIF, 80, 80, 40, 3, 1, 60, 90}, {0, 90, 127, 70}, 0, 0, FX(0, 0, 30, 60), PAT(0)},
    {"TREPADORA", {PL_CIRCUM, 64, 80, 70, 0, 4, 70, 40}, {0, 90, 127, 80}, 0, 0, FX(0, 20, 30, 70), PAT(0)},
    {"POLINIZA", {PL_POLIN, 64, 60, 60, 3, 3, 70, 70}, {0, 90, 127, 80}, 0, 0, FX(0, 0, 40, 70), PAT(0)},
    {"HOJARASCA", {PL_HOJAR, 64, 80, 64, 1, 6, 80, 60}, {0, 90, 127, 90}, 0, 0, FX(0, 0, 30, 80), PAT(0)},
    {"ANILLOS", {PL_ANILLO, 60, 20, 70, 3, 2, 90, 90}, {20, 90, 127, 100}, 0, 0, FX(0, 30, 10, 90), PAT(0)},
    {"SAVIA", {PL_SAVIA, 64, 64, 30, 3, 7, 50, 80}, {0, 90, 127, 60}, 0, 0, FX(0, 0, 40, 60), PAT(0)},
    {"FOTOTROPO", {PL_FOTOT, 64, 80, 40, 0, 0, 70, 40}, {0, 90, 127, 80}, 0, 0, FX(0, 20, 30, 70), PAT(0)},
    {"RAICES", {PL_RAICES, 80, 80, 25, 2, 4, 60, 70}, {0, 90, 127, 70}, 0, 0, FX(0, 0, 30, 60), PAT(0)},
    {"FLORACION", {PL_FLOR, 64, 80, 30, 0, 2, 80, 80}, {0, 90, 127, 90}, 0, 0, FX(0, 30, 30, 80), PAT(0)},
    {"ESTOMAS", {PL_ESTOMA, 40, 40, 30, 3, 5, 110, 60}, {60, 90, 127, 110}, 0, 0, FX(0, 50, 10, 95), PAT(0)},
    {"ZARCILLO", {PL_ZARCI, 64, 90, 50, 0, 1, 70, 64}, {0, 90, 127, 80}, 0, 0, FX(0, 20, 30, 70), PAT(0)},
    {"MUSGO", {PL_MUSGO, 96, 40, 40, 2, 6, 110, 64}, {80, 90, 127, 110}, 0, 0, FX(0, 50, 10, 95), PAT(0)},
    {"GERMINA", {PL_GERMIN, 64, 80, 40, 3, 0, 70, 60}, {0, 90, 127, 70}, 0, 0, FX(0, 0, 30, 60), PAT(0)},
    {"RIZOMA", {PL_RIZOMA, 64, 80, 30, 1, 1, 70, 50}, {0, 90, 127, 80}, 0, 0, FX(0, 0, 40, 70), PAT(0)},
    {"CIRCADIANO", {PL_RELOJ, 80, 64, 30, 0, 3, 80, 80}, {0, 90, 127, 90}, 0, 0, FX(0, 30, 20, 80), PAT(0)},
};

static const engine_t ENG_PLANT = {
    .name = "VEGETAL",
    .page_title = {"PLANTA", "VOZ"},
    .edit = {
        {"PROC", F_ENUM, 0, PL_NPROC - 1, 0, N_PL_PROC, 0},
        {"NODO", F_PCT, 0, 127, 64, 0, 0},
        {"CREC", F_PCT, 0, 127, 80, 0, 0},
        {"ANGL", F_INT, 0, 127, 64, 0, 0},
        {"ESPC", F_ENUM, 0, PL_NSPEC - 1, 0, PL_SPEC_NAMES, 0},
        {"TIMB", F_ENUM, 0, PL_NTIMB - 1, 1, N_PL_TIMB, 0},
        {"DECA", F_PCT, 0, 127, 70, 0, 0},
        {"EDAD", F_PCT, 0, 127, 64, 0, 0},
    },
    .presets = PLANT_PRESETS,
    .npresets = NELEM(PLANT_PRESETS),
    .note_on = plant_note_on,
    .render = plant_render,
    .desc = plant_desc,
    .knob = {P_E1, P_E2, P_E3, P_E7},
    .block = plant_block,
    .mem = sizeof(pl_part_t),
};
