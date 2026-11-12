# VEGETAL · firmware para el M-VAVE FM-1

La familia vegetal completa de Ecotono como firmware para el FM-1: 21 procesos (filotaxis, planta L, raíz,
micorriza, nictinastia, semillas, ramificación, circumnutación, polinización, hojarasca, anillos, savia,
fototropismo, raíces exploradoras, floración, estomas, zarcillo, musgo, germinación, rizoma y reloj circadiano), con
su vista en la pantalla del FM-1. Construido sobre [Felucca](https://github.com/hugelton/Felucca) de Leo Kuroshita
(Hügelton Instruments), sin afiliación con sus autores.

## Novedades de 1.8.1-vegetal2

- **TIMB, un timbre por motivo** (EDIT 2, donde estaba MADR): SENO, MADERA, CAMPAN (FM ×3,5), VIDRIO, CUERDA
  (pulsada), SOPLO (aliento en banda), HOJA (crujido) y AGUA (la gota que sube). Cada pista tiene el suyo y cada
  preset trae uno.
- **Las notas de los nodos salen por MIDI** (USB), en el canal de su pista + 4 (pistas 1-4 → canales 5-8), con su
  nota apagada al terminar. En AUM, el FM-1 conectado por cable puede tocar Koala, Poly 2 o cualquier sinte.
- **Espejo visual** (`webapp/espejo/`): una página para Chrome en el Mac que escucha al FM-1 por USB y proyecta cada
  proceso en píxel, malla 3D o nube de puntos, por pista o en mezcla, con controles de VJ, mapeo de las perillas
  del FM-1 y una ventana de proyección a pantalla completa. Sin FM-1, una demostración con el mismo motor.
- El tempo del motor sigue al BPM del proyecto (antes, siempre 120).

## El protocolo del espejo (SysEx del editor de Felucca, F0 7D 46 4C …)

`WATCH` con el bit 2 (valor 7) pide, mientras haya un `PING` por segundo: 96 `VEG_NOTES` (cada nota al nacer: parte,
nodo, grado, intensidad, nota MIDI, duración, padre, x, y), 97 `VEG_STATE` (cada ~100 ms: por parte PROC, TIMB,
ESPC, nodos, día, luz, apertura) y 99 `VEG_POS` (las posiciones de la figura en ronda). 98 `VEG_CFG` enciende o
apaga las notas por MIDI. El editor oficial (`WATCH` 3) no recibe nada de esto.

## Bluetooth

Felucca no trae Bluetooth: por eso el iPhone no ve el FM-1. Con un iPhone con Lightning, usa el adaptador
«Lightning a USB 3 para cámaras» (con su cargador) y un cable USB-A a USB-C: el FM-1 aparece en AUM como MIDI y
audio USB. El paso «Memoria del firmware» de Actions dice cuánta RAM y pool quedan libres, para saber si cabría la
pila Bluetooth propia de fm1-base.

Este repositorio guarda solo lo que VEGETAL agrega o cambia (`overlay/`). GitHub Actions toma Felucca en un commit
fijo, aplica `overlay/`, compila el firmware, lo prueba en el anfitrión de Felucca y publica en GitHub Pages el sitio
con el instalador web.

Autor de VEGETAL: Ph.D. Mauricio Vásquez Arias, profesor titular – investigador asociado, Escuela de Artes y
Humanidades, Universidad EAFIT. Licencia: GPL-3.0 (como Felucca). Software experimental, sin garantía.
M-VAVE y FM-1 son marcas de sus dueños.
