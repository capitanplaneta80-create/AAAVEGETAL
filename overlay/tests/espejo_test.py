#!/usr/bin/env python3
"""espejo.html en Chrome sin pantalla: (1) un FM-1 falso que repite lo que el firmware real mandó (vegetal_stream.c),
(2) la demostración con el motor en WebAssembly, (3) el proyector en otra ventana. Capturas en salida/."""
import json, sys, pathlib, time
from playwright.sync_api import sync_playwright
page_path, stream_path, outdir = map(pathlib.Path, sys.argv[1:4])
outdir.mkdir(exist_ok=True)
stream = json.loads(stream_path.read_text())
FAKE = """
(() => {
  const STREAM = %s; const sent = []; window.__sent = sent;
  const mk = (id, name) => ({id, name, manufacturer:'', state:'connected', connection:'open', onmidimessage:null, send(d){ sent.push(Array.from(d)); }});
  const inp = mk('in1', 'FM-1 MIDI'), out = mk('out1', 'FM-1 MIDI');
  let started = false;
  out.send = d => { sent.push(Array.from(d)); if (!started && d[4] === 22) { started = true; const t0 = performance.now();
      let i = 0; (function tick(){ const t = (performance.now() - t0)/1000; while (i < STREAM.length && STREAM[i][0] <= t){ if (inp.onmidimessage) inp.onmidimessage({data:new Uint8Array(STREAM[i][1])}); i++; } if (i < STREAM.length) setTimeout(tick, 4); })(); } };
  navigator.requestMIDIAccess = async () => ({ inputs:new Map([['in1', inp]]), outputs:new Map([['out1', out]]), onstatechange:null, sysexEnabled:true });
})();
""" % json.dumps(stream)
STATS = """(id) => { const c = document.getElementById(id); const x = c.getContext('2d'); const d = x.getImageData(0,0,c.width,c.height).data;
  let lit = 0, sum = 0; for (let i=0;i<d.length;i+=4){ const l = d[i]+d[i+1]+d[i+2]; sum += l; if (l > 60) lit++; } return {lit: lit/(d.length/4), mean: sum/(d.length/4)/3}; }"""
bad = 0
def check(what, ok):
    global bad
    print(f"espejo: {what:<64} {'ok' if ok else 'FAIL'}"); bad += not ok
with sync_playwright() as p:
    b = p.chromium.launch(args=["--autoplay-policy=no-user-gesture-required"])
    ctx = b.new_context(viewport={"width": 1500, "height": 980})
    ctx.add_init_script(FAKE)
    errs = []
    pg = ctx.new_page(); pg.on("pageerror", lambda e: errs.append(str(e))); pg.on("console", lambda m: m.type == "error" and errs.append(m.text))
    pg.goto(page_path.resolve().as_uri())
    pg.click("#bConnect"); pg.wait_for_timeout(4500)
    st = pg.inner_text("#stVeg"); check("WATCH 7 enviado; el FM-1 falso responde con el espejo", "sí" in st)
    sent = pg.evaluate("window.__sent.map(d=>d[4])")
    check("pide TRACK_DUMP de las cuatro pistas y VEG_STATE", sent.count(29) == 4 and 97 in sent)
    parts = pg.inner_text("#parts"); check("las cuatro pistas: FILO ANILLO MICO MUSGO", all(w in parts for w in ("FILO", "ANILLO", "MICO", "MUSGO")))
    n = pg.evaluate("S.parts.map(P=>P.nodes.filter(n=>n.pres).length)"); print("  nodos presentes por pista:", n)
    check("los nodos llegan (VEG_NOTES)", sum(n) > 20)
    for i, name in enumerate(["pixel", "malla", "nube"]):
        pg.select_option("#styA", str(i)); pg.wait_for_timeout(700)
        s = pg.evaluate(STATS, "pv"); pg.screenshot(path=str(outdir / f"espejo_fm_{name}.png"))
        check(f"estilo {name}: la vista previa no está negra ({s['lit']*100:.1f} %% encendido)" % (), s["lit"] > .002)
    pg.click("#bMix"); pg.select_option("#layout", "1"); pg.wait_for_timeout(1500)
    s = pg.evaluate(STATS, "pv"); pg.screenshot(path=str(outdir / "espejo_fm_mezcla.png"))
    check("mezcla en cuadrantes", s["lit"] > .002)
    pg.wait_for_timeout(6000)                                      # a los ~10 s el firmware elige la pista 2 (ANILLOS)
    sel = pg.evaluate("S.sel"); check("RELOAD: la pista elegida sigue al FM-1 (pista 2)", sel == 1)
    rings = pg.evaluate("S.parts[1].rings.length"); check(f"ANILLOS: anillos ({rings})", rings > 3)
    pg.click("#bInd"); pg.select_option("#styA", "1"); pg.wait_for_timeout(1200); pg.screenshot(path=str(outdir / "espejo_fm_anillos_malla.png"))
    hotmap = pg.evaluate("Object.keys(__esp.hot)"); check("la perilla (CHANGED NODO) se ve en el mapeo", "p:107" in hotmap)
    # el proyector
    with ctx.expect_page() as pi:
        pg.click("#bProj")
    pj = pi.value; pj.wait_for_timeout(2000)
    s = pj.evaluate(STATS, "projcv"); pj.screenshot(path=str(outdir / "espejo_proyector.png"))
    check("el proyector recibe y dibuja", s["lit"] > .002 and "abierto" in pg.inner_text("#projSt"))
    # aprender
    pg.click("#mapT tbody tr:nth-child(1) .mlearn"); pg.evaluate("__esp.touch('p:36')"); m = pg.evaluate("__esp.map[0].src")
    check("aprender: la fila toma el control que se mueve", m == "p:36")
    check("sin errores en la página", not errs)
    if errs: print("\n".join(errs[:8]))
    # la demostración
    pg2 = ctx.new_page(); e2 = []; pg2.on("pageerror", lambda e: e2.append(str(e)))
    pg2.goto(page_path.resolve().as_uri()); pg2.click("#bDemo"); pg2.wait_for_timeout(3000)
    n = pg2.evaluate("S.parts.map(P=>P.nodes.filter(n=>n.pres).length)"); print("  demostración, nodos:", n)
    check("demostración: el motor en WebAssembly crece", sum(n) > 20)
    for i, name in enumerate(["pixel", "malla", "nube"]):
        pg2.select_option("#styA", str(i)); pg2.wait_for_timeout(800); pg2.screenshot(path=str(outdir / f"espejo_demo_{name}.png"))
    pg2.click("#bMix"); pg2.wait_for_timeout(1200); pg2.screenshot(path=str(outdir / "espejo_demo_mezcla.png"))
    fps = pg2.evaluate("new Promise(r=>{let n=0;const t=performance.now();(function f(){n++; if(performance.now()-t<2000) requestAnimationFrame(f); else r(n/2);})();})")
    print(f"  cuadros/s (sin GPU, nube + mezcla): {fps:.0f}")
    check("demostración sin errores", not e2)
    if e2: print("\n".join(e2[:8]))
    b.close()
print("espejo test", "FAILED" if bad else "passed"); sys.exit(1 if bad else 0)
