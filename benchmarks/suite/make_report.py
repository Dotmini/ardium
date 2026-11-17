#!/usr/bin/env python3
"""Builds the formal Thai benchmark report (HTML -> PDF via headless Chrome).

Reads the JSON written by run_suite.py from SUITE_OUT/results and the analysis
text from analysis_th.html (next to this script), and writes report.html and
the PDF. Intermediate files stay in SUITE_OUT; the PDF goes to --pdf.
"""
import argparse
import html
import json
import math
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.environ.get("SUITE_OUT", "/Volumes/MAC/arc-build/suite")
RES = os.path.join(OUT, "results")
CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"

LANGS = ["arc", "rust", "go", "java", "python"]
LANG_NAME = {"arc": "Arc", "rust": "Rust", "go": "Go", "java": "Java", "python": "Python"}
# validated categorical palette (dataviz reference, slots 1-5, light surface)
COLOR = {"arc": "#2a78d6", "rust": "#eb6834", "go": "#1baf7a", "java": "#eda100", "python": "#e87ba4"}
WORKLOAD_TH = {
    "fib": ("ฟีโบนักชีแบบเรียกซ้ำ", "fib(38) แบบเรียกตัวเองซ้ำ วัดต้นทุนการเรียกฟังก์ชัน"),
    "sieve": ("ตะแกรงเอราทอสเทนีส", "หาจำนวนเฉพาะไม่เกิน 50,000,000 วัดการเข้าถึงอาร์เรย์และลูปจำนวนเต็ม"),
    "nbody": ("จำลองการเคลื่อนที่ N-body", "ดาว 5 ดวง 5,000,000 ขั้นเวลา วัดคณิตศาสตร์ทศนิยมบนออบเจกต์"),
    "binary_trees": ("ต้นไม้ทวิภาค", "สร้างและตรวจต้นไม้ความลึก 18 (รวม ~68 ล้านโหนด) วัดการจองหน่วยความจำและ GC"),
    "wordcount": ("นับคำด้วยตารางแฮช", "สร้างคีย์สตริง 3,000,000 ครั้งแล้วนับในแฮชแมป วัดสตริงและแฮชแมป"),
    "sort": ("เรียงลำดับจำนวนเต็ม", "สร้างและเรียงจำนวนเต็ม 5,000,000 ค่าด้วยไลบรารีมาตรฐาน"),
    "matmul": ("คูณเมทริกซ์", "เมทริกซ์ทศนิยม 400×400 แบบลูปซ้อนสามชั้น วัดอาร์เรย์สองมิติ"),
    "mandelbrot": ("เซตแมนเดลบรอต", "ภาพ 2000×2000 จุด วนสูงสุด 100 รอบ วัดทศนิยมที่มีการแตกแขนง"),
    "fannkuch": ("Fannkuch-redux", "เรียงสับเปลี่ยนทั้งหมดของ n = 10 (3,628,800 แบบ) วัดอาร์เรย์ขนาดเล็ก"),
    "text": ("ประมวลผลข้อความ CSV", "สร้างข้อมูล 1,000,000 บรรทัด แล้วแยก แปลงตัวเลข และสรุปผล วัดงานสตริง"),
}
THAI_DIGITS = str.maketrans("0123456789", "๐๑๒๓๔๕๖๗๘๙")


def th(s):
    return str(s).translate(THAI_DIGITS)


def esc(s):
    return html.escape(str(s))


def load(name):
    with open(os.path.join(RES, name)) as f:
        return json.load(f)


def fmt_s(x):
    if x >= 10:
        return f"{x:.2f}"
    if x >= 1:
        return f"{x:.3f}"
    return f"{x:.4f}"


def fmt_rel(x):
    return f"{x:.2f}×" if x < 10 else f"{x:.1f}×"


# ------------------------------------------------------------------ charts --

def log_ticks(lo, hi):
    ticks = []
    d = 10 ** math.floor(math.log10(lo))
    while d <= hi * 1.0001:
        for m in (1, 2, 5):
            v = d * m
            if lo * 0.999 <= v <= hi * 1.001:
                ticks.append(v)
        d *= 10
    return ticks


def tick_label(v, unit):
    if unit == "x":
        return f"{v:g}×"
    if unit == "ms":
        return f"{v:g}"
    if unit == "MB":
        return f"{v:g}"
    return f"{v:g}"


def dot_plot(rows, unit, width=330, title="", show_values=False):
    """Horizontal dot plot on a log axis. rows: [(lang, median, lo, hi)]."""
    row_h, top, left, right, bottom = 22, 24 if title else 8, 52, 14, 34
    h = top + row_h * len(rows) + bottom
    vals = [v for _, m, lo, hi in rows for v in (m, lo, hi)]
    lo_d = 10 ** math.floor(math.log10(min(vals) * 0.98))
    hi_d = 10 ** math.ceil(math.log10(max(vals) * 1.02))
    span = math.log10(hi_d) - math.log10(lo_d)
    pw = width - left - right

    def x(v):
        return left + (math.log10(v) - math.log10(lo_d)) / span * pw

    o = [f'<svg viewBox="0 0 {width} {h}" width="{width}" height="{h}" role="img" aria-label="{esc(title)}">']
    if title:
        o.append(f'<text x="0" y="14" class="ct">{esc(title)}</text>')
    for t in log_ticks(lo_d, hi_d):
        xx = x(t)
        o.append(f'<line x1="{xx:.1f}" y1="{top - 4}" x2="{xx:.1f}" y2="{h - bottom + 2}" class="grid"/>')
        o.append(f'<text x="{xx:.1f}" y="{h - bottom + 14}" class="tick" text-anchor="middle">{tick_label(t, unit)}</text>')
    for i, (lang, m, lo, hi) in enumerate(rows):
        cy = top + i * row_h + row_h / 2
        o.append(f'<text x="{left - 8}" y="{cy + 4:.1f}" class="cat" text-anchor="end">{LANG_NAME[lang]}</text>')
        if hi > lo:
            o.append(f'<line x1="{x(lo):.1f}" y1="{cy:.1f}" x2="{x(hi):.1f}" y2="{cy:.1f}" stroke="{COLOR[lang]}" stroke-width="2" stroke-linecap="round"/>')
        o.append(f'<circle cx="{x(m):.1f}" cy="{cy:.1f}" r="5" fill="{COLOR[lang]}" stroke="#fcfcfb" stroke-width="2"/>')
        if show_values:
            o.append(f'<text x="{x(m) + 9:.1f}" y="{cy + 4:.1f}" class="val">{esc(fmt_rel(m) if unit == "x" else fmt_s(m))}</text>')
    unit_th = {"s": "วินาที (สเกลลอการิทึม)", "x": "เท่าของภาษาที่เร็วที่สุด (สเกลลอการิทึม)",
               "MB": "เมกะไบต์ (สเกลลอการิทึม)", "ms": "มิลลิวินาที (สเกลลอการิทึม)"}[unit]
    o.append(f'<text x="{left + pw / 2:.1f}" y="{h - 2}" class="axis" text-anchor="middle">{unit_th}</text>')
    o.append("</svg>")
    return "".join(o)


# ------------------------------------------------------------------ report --

def build_html(analysis_html):
    env = load("environment.json")
    summ = load("summary.json")
    summary, geo = summ["summary"], summ["geomean_relative"]
    runs = load("runs.json")
    build = load("build.json")
    startup = load("startup.json")
    outputs = load("outputs.json")
    workloads = list(env["workloads"].keys())
    rounds = env["rounds"]
    n_runs = len([r for r in runs if r["round"] > 0])

    ranked = sorted(LANGS, key=lambda l: geo[l])

    def rows_for(w, key):
        out = []
        for lang in LANGS:
            d = summary[w][lang][key]
            out.append((lang, d["median"], d["min"], d["max"]))
        return out

    parts = []
    P = parts.append

    # ---------------------------------------------------------------- cover
    P(f'''
<section class="cover">
  <div class="docno">เอกสารรายงานฉบับที่ ARC-BM {th("1/2569")}</div>
  <div class="cover-title">รายงานผลการทดสอบสมรรถนะเชิงเปรียบเทียบ</div>
  <div class="cover-sub">ภาษาโปรแกรม Arc เทียบกับ Rust, Go, Java และ Python</div>
  <div class="cover-sub2">ด้วยโจทย์เดียวกัน {th(len(workloads))} โจทย์ ภายใต้เงื่อนไขการทดสอบที่เท่าเทียมกัน</div>
  <table class="cover-meta">
    <tr><td>หน่วยงานผู้จัดทำ</td><td>คณะทำงานพัฒนาภาษาโปรแกรม Arc</td></tr>
    <tr><td>วันที่ทดสอบ</td><td>{th("18")} พฤศจิกายน พ.ศ. {th("2569")}</td></tr>
    <tr><td>เครื่องทดสอบ</td><td>{esc(env["cpu"])} · {th(env["cores"])} แกน · หน่วยความจำ {th(int(env["ram_gb"]))} GB · macOS {esc(env["macos"])}</td></tr>
    <tr><td>จำนวนการวัด</td><td>{th(rounds)} รอบต่อคู่ (โจทย์, ภาษา) รวม {th(n_runs)} ครั้ง ไม่นับรอบอุ่นเครื่อง</td></tr>
  </table>
</section>
''')

    # ---------------------------------------------------------------- executive summary
    gl = "".join(
        f'<tr><td class="c">{th(i + 1)}</td><td><span class="sw" style="background:{COLOR[l]}"></span>{LANG_NAME[l]}</td>'
        f'<td class="r">{fmt_rel(geo[l])}</td>'
        f'<td class="r">{th(sum(1 for w in workloads if summary[w][l]["relative"] == 1.0))}</td></tr>'
        for i, l in enumerate(ranked))
    P(f'''
<section class="page">
<h1 class="center">บทสรุปสำหรับผู้บริหาร</h1>
{analysis_html["summary"]}
<div class="pair"><table class="data narrow">
<caption>ตารางที่ ๑ อันดับภาพรวม (ค่าเฉลี่ยเรขาคณิตของเวลาเมื่อเทียบกับภาษาที่เร็วที่สุดในแต่ละโจทย์ ยิ่งน้อยยิ่งดี)</caption>
<thead><tr><th>อันดับ</th><th>ภาษา</th><th>ค่าเฉลี่ยเรขาคณิต</th><th>จำนวนโจทย์ที่เร็วที่สุด</th></tr></thead>
<tbody>{gl}</tbody></table>
<figure>{dot_plot([(l, geo[l], geo[l], geo[l]) for l in LANGS], "x", width=300, show_values=True)}
<figcaption>ภาพที่ ๑ ค่าเฉลี่ยเรขาคณิตของเวลา<br>เทียบกับภาษาที่เร็วที่สุด (1× = เร็วที่สุดทุกโจทย์)</figcaption></figure></div>
</section>
''')

    # ---------------------------------------------------------------- 1-4 background, objectives, scope, method
    wl_rows = "".join(
        f'<tr><td class="c">{th(i + 1)}</td><td>{WORKLOAD_TH[w][0]}<br><span class="mono small">{w}</span></td>'
        f'<td>{WORKLOAD_TH[w][1]}</td><td class="mono c">{esc(" ".join(env["workloads"][w]))}</td></tr>'
        for i, w in enumerate(workloads))
    flags = env["flags"]
    tool_rows = "".join(
        f'<tr><td>{LANG_NAME[l]}</td><td class="mono small">{esc(v)}</td><td class="small">{esc(flags[l])}</td></tr>'
        for l, v in [("arc", env["arc"] + " / " + env["c_backend"]), ("rust", env["rustc"]), ("go", env["go"]),
                     ("java", env["java"]), ("python", env["python"])])
    P(f'''
<section class="page">
<h1>๑. หลักการและเหตุผล</h1>
{analysis_html["background"]}
<h1>๒. วัตถุประสงค์</h1>
<ol class="th">
<li>เพื่อวัดสมรรถนะด้านเวลาประมวลผลของภาษา Arc เทียบกับภาษาที่ใช้งานจริงในอุตสาหกรรม ได้แก่ Rust, Go, Java และ Python</li>
<li>เพื่อวัดการใช้หน่วยความจำ เวลาเริ่มต้นโปรแกรม เวลาคอมไพล์ และขนาดไฟล์ที่ได้ ภายใต้เงื่อนไขเดียวกัน</li>
<li>เพื่อระบุจุดแข็งและจุดที่ต้องปรับปรุงของภาษา Arc อย่างเป็นกลางและตรวจสอบได้</li>
</ol>
<h1>๓. ขอบเขตการทดสอบ</h1>
<p class="keep">๓.๑ ภาษาและเครื่องมือที่ทดสอบ</p>
<table class="data">
<thead><tr><th>ภาษา</th><th>เวอร์ชัน</th><th>การตั้งค่าการคอมไพล์/การรัน</th></tr></thead>
<tbody>{tool_rows}</tbody></table>
<p class="keep">๓.๒ โจทย์ทดสอบ ({th(len(workloads))} โจทย์ บวกการวัดเวลาเริ่มต้นโปรแกรม)</p>
<table class="data">
<thead><tr><th>ที่</th><th>โจทย์</th><th>รายละเอียดและสิ่งที่วัด</th><th>อาร์กิวเมนต์</th></tr></thead>
<tbody>{wl_rows}</tbody></table>
</section>
<section>
<h1>๔. วิธีการทดสอบ</h1>
{analysis_html["method"]}
</section>
''')

    # ---------------------------------------------------------------- 5 results
    P('<section class="page"><h1>๕. ผลการทดสอบ</h1>')
    P(f'<p class="indent">๕.๑ <b>เวลาประมวลผลรายโจทย์</b> ภาพที่ ๒ แสดงค่ามัธยฐานของเวลาจริง (จุด) และช่วงค่าต่ำสุดถึงสูงสุดจาก {th(rounds)} รอบ (เส้น) บนสเกลลอการิทึม ค่าที่แน่นอนอยู่ในตารางที่ ๒</p>')
    P('<div class="multiples">')
    for w in workloads:
        P(f'<div class="panel">{dot_plot(rows_for(w, "wall"), "s", width=330, title=WORKLOAD_TH[w][0] + " (" + w + ")")}</div>')
    P('</div><p class="figcap">ภาพที่ ๒ เวลาประมวลผลรายโจทย์ (มัธยฐาน และช่วงต่ำสุด–สูงสุด) หน่วยวินาที สเกลลอการิทึม</p></section>')

    # table 2: per workload medians and relative
    head = "".join(f"<th>{LANG_NAME[l]}</th>" for l in LANGS)
    body = ""
    for w in workloads:
        best = min(LANGS, key=lambda l: summary[w][l]["wall"]["median"])
        cells = ""
        for l in LANGS:
            d = summary[w][l]
            cls = ' class="r best"' if l == best else ' class="r"'
            cells += f'<td{cls}>{fmt_s(d["wall"]["median"])}<br><span class="small">{fmt_rel(d["relative"])}</span></td>'
        body += f'<tr><td>{WORKLOAD_TH[w][0]}<br><span class="mono small">{w}</span></td>{cells}</tr>'
    geo_cells = "".join(f'<td class="r"><b>{fmt_rel(geo[l])}</b></td>' for l in LANGS)
    body += f'<tr class="total"><td>ค่าเฉลี่ยเรขาคณิต</td>{geo_cells}</tr>'
    P(f'''<section class="page">
<table class="data compact">
<caption>ตารางที่ ๒ มัธยฐานของเวลาจริง (วินาที) และอัตราส่วนเทียบกับภาษาที่เร็วที่สุดในโจทย์นั้น (ตัวหนา = เร็วที่สุด)</caption>
<thead><tr><th>โจทย์</th>{head}</tr></thead><tbody>{body}</tbody></table>
{analysis_html["results_time"]}
</section>''')

    # memory
    P('<section class="page">')
    P('<p class="indent">๕.๒ <b>หน่วยความจำสูงสุด (peak RSS)</b> ซึ่งวัดจากระบบปฏิบัติการด้วย <span class="mono">wait4</span> และรวมหน่วยความจำของรันไทม์ทั้งหมด (เช่น JVM และอินเทอร์พรีเตอร์ Python)</p>')
    P('<div class="multiples">')
    for w in workloads:
        P(f'<div class="panel">{dot_plot(rows_for(w, "maxrss_mb"), "MB", width=330, title=WORKLOAD_TH[w][0] + " (" + w + ")")}</div>')
    P('</div><p class="figcap">ภาพที่ ๓ หน่วยความจำสูงสุดรายโจทย์ (มัธยฐาน และช่วงต่ำสุด–สูงสุด) หน่วยเมกะไบต์ สเกลลอการิทึม</p>')
    P('</section>')
    mem_body = ""
    for w in workloads:
        best = min(LANGS, key=lambda l: summary[w][l]["maxrss_mb"]["median"])
        mem_body += f'<tr><td>{WORKLOAD_TH[w][0]}</td>' + "".join(
            f'<td class="r{" best" if l == best else ""}">{summary[w][l]["maxrss_mb"]["median"]:.1f}</td>' for l in LANGS) + "</tr>"
    P(f'''<section class="page">
<table class="data">
<caption>ตารางที่ ๓ มัธยฐานของหน่วยความจำสูงสุด (MB) (ตัวหนา = ใช้น้อยที่สุด)</caption>
<thead><tr><th>โจทย์</th>{head}</tr></thead><tbody>{mem_body}</tbody></table>
{analysis_html["results_memory"]}
''')

    # startup
    st_rows = []
    for l in LANGS:
        xs = sorted(r["wall_s"] * 1000 for r in startup[l])
        med = xs[len(xs) // 2] if len(xs) % 2 else (xs[len(xs) // 2 - 1] + xs[len(xs) // 2]) / 2
        st_rows.append((l, med, xs[0], xs[-1]))
    st_table = "".join(
        f'<tr><td>{LANG_NAME[l]}</td><td class="r">{m:.1f}</td><td class="r">{lo:.1f}</td><td class="r">{hi:.1f}</td>'
        f'<td class="r">{statistics_median([r["maxrss_bytes"] / 1048576 for r in startup[l]]):.1f}</td></tr>'
        for l, m, lo, hi in st_rows)
    P(f'''<p class="indent">๕.๓ <b>เวลาเริ่มต้นโปรแกรม</b> วัดจากโปรแกรมที่พิมพ์คำว่า hello เพียงบรรทัดเดียว ภาษาละ {th(len(startup["arc"]))} ครั้ง สลับลำดับแบบสุ่ม</p>
<figure>{dot_plot(st_rows, "ms", width=600)}<figcaption>ภาพที่ ๔ เวลาเริ่มต้นโปรแกรม (มัธยฐาน และช่วงต่ำสุด–สูงสุด) หน่วยมิลลิวินาที</figcaption></figure>
<table class="data narrow"><caption>ตารางที่ ๔ เวลาเริ่มต้นโปรแกรม (มิลลิวินาที) และหน่วยความจำ (MB)</caption>
<thead><tr><th>ภาษา</th><th>มัธยฐาน</th><th>ต่ำสุด</th><th>สูงสุด</th><th>หน่วยความจำ</th></tr></thead><tbody>{st_table}</tbody></table>
</section>''')

    # compile, size, loc
    def bsum(l, key):
        vals = [build[f"{w}/{l}"][key] for w in workloads if build[f"{w}/{l}"][key] is not None]
        return vals
    cb = ""
    for l in LANGS:
        ct = bsum(l, "compile_s")
        sz = bsum(l, "size_bytes")
        lc = bsum(l, "loc")
        ct_s = f"{sum(ct) / len(ct):.3f}" if ct else "– (ไม่ต้องคอมไพล์)"
        size_s = f"{sum(sz) / len(sz) / 1024:.1f}"
        cb += f'<tr><td>{LANG_NAME[l]}</td><td class="r">{ct_s}</td><td class="r">{size_s}</td><td class="r">{sum(lc)}</td></tr>'
    P(f'''<section>
<p class="indent">๕.๔ <b>เวลาคอมไพล์ ขนาดไฟล์ และขนาดโค้ด</b> เวลาคอมไพล์เป็นมัธยฐานจากการคอมไพล์ใหม่ทั้งหมด ๓ ครั้งต่อโปรแกรมโดยไม่ใช้แคชของโปรแกรมนั้น</p>
<table class="data narrow"><caption>ตารางที่ ๕ เวลาคอมไพล์เฉลี่ยต่อโปรแกรม ขนาดไฟล์ที่ได้เฉลี่ย และจำนวนบรรทัดโค้ดรวมทั้ง {th(len(workloads))} โจทย์</caption>
<thead><tr><th>ภาษา</th><th>เวลาคอมไพล์ (วินาที)</th><th>ขนาดไฟล์ (KB)</th><th>บรรทัดโค้ดรวม</th></tr></thead><tbody>{cb}</tbody></table>
<p class="note">หมายเหตุ: ขนาดไฟล์ของ Java คือไฟล์ .class (ต้องใช้ JVM ประมาณ 300 MB ในการรัน) ขนาดของ Python คือไฟล์ซอร์สโค้ด (ต้องใช้อินเทอร์พรีเตอร์) ส่วน Arc, Rust และ Go เป็นไฟล์ปฏิบัติการที่รันได้ด้วยตัวเอง บรรทัดโค้ดนับเฉพาะบรรทัดที่ไม่ว่างและไม่ใช่หมายเหตุ</p>
{analysis_html["results_build"]}
</section>''')

    # variability
    var_body = ""
    for w in workloads:
        var_body += f'<tr><td>{WORKLOAD_TH[w][0]}</td>' + "".join(
            f'<td class="r">{summary[w][l]["wall"]["cv_pct"]:.1f}</td>' for l in LANGS) + "</tr>"
    # sensitivity: the aggregate under other choices of metric and workload set
    def geo_of(key, exclude=()):
        ws = [w for w in workloads if w not in exclude]
        out = {}
        for l in LANGS:
            acc = 0.0
            for w in ws:
                best_v = min(summary[w][k][key]["median"] for k in LANGS)
                acc += math.log(summary[w][l][key]["median"] / best_v)
            out[l] = math.exp(acc / len(ws))
        return out
    variants = [
        (f"เวลาจริง ทั้ง {th(len(workloads))} โจทย์ (เกณฑ์หลัก)", geo_of("wall")),
        (f"เวลาจริง ไม่รวมโจทย์ sort", geo_of("wall", ("sort",))),
        (f"เวลา CPU (user + system) ทั้ง {th(len(workloads))} โจทย์", geo_of("cpu")),
        (f"หน่วยความจำสูงสุด ทั้ง {th(len(workloads))} โจทย์", geo_of("maxrss_mb")),
    ]
    sens_body = ""
    for label, g in variants:
        best = min(LANGS, key=lambda l: g[l])
        order = sorted(LANGS, key=lambda l: g[l])
        sens_body += f'<tr><td>{label}</td>' + "".join(
            f'<td class="r{" best" if l == best else ""}">{fmt_rel(g[l])}<br><span class="small">อันดับ {th(order.index(l) + 1)}</span></td>'
            for l in LANGS) + "</tr>"
    P(f'''<section>
<p class="indent">๕.๕ <b>ความแปรปรวนของการวัด</b> สัมประสิทธิ์การแปรผัน (CV) ของเวลาจริงจาก {th(rounds)} รอบ ค่ายิ่งต่ำยิ่งแสดงว่าผลวัดคงที่และเชื่อถือได้</p>
<table class="data"><caption>ตารางที่ ๖ สัมประสิทธิ์การแปรผันของเวลาจริง (ร้อยละ)</caption>
<thead><tr><th>โจทย์</th>{head}</tr></thead><tbody>{var_body}</tbody></table>
<p class="indent">๕.๖ <b>การวิเคราะห์ความอ่อนไหวของผลรวม</b> เพื่อตรวจว่าอันดับภาพรวมขึ้นกับการเลือกเกณฑ์หรือโจทย์ใดโจทย์หนึ่งหรือไม่ ตารางที่ ๗ คำนวณค่าเฉลี่ยเรขาคณิตใหม่ภายใต้เกณฑ์อื่น โดยวิธีเดียวกับตารางที่ ๑</p>
<table class="data"><caption>ตารางที่ ๗ ค่าเฉลี่ยเรขาคณิตเทียบกับค่าที่ดีที่สุดในแต่ละโจทย์ ภายใต้เกณฑ์ต่าง ๆ (ยิ่งน้อยยิ่งดี ตัวหนา = ดีที่สุด)</caption>
<thead><tr><th>เกณฑ์</th>{head}</tr></thead><tbody>{sens_body}</tbody></table>
</section>
<section>
<h1>๖. การวิเคราะห์และอภิปรายผล</h1>
{analysis_html["discussion"]}
</section>
<section>
<h1>๗. ข้อจำกัดของการทดสอบ</h1>
{analysis_html["limitations"]}
<h1>๘. สรุปและข้อเสนอแนะ</h1>
{analysis_html["conclusion"]}
<div class="sign">
  <div><p>ลงชื่อ ....................................... ผู้จัดทำรายงาน</p>
  <p>(.......................................)</p>
  <p>วันที่ ...... เดือน .................. พ.ศ. ........</p></div>
  <div><p>ลงชื่อ ....................................... ผู้ตรวจสอบ</p>
  <p>(.......................................)</p>
  <p>วันที่ ...... เดือน .................. พ.ศ. ........</p></div>
</div>
</section>''')

    # ---------------------------------------------------------------- appendices
    P('<section class="page"><h1 class="center">ภาคผนวก ก<br>ข้อมูลสถิติฉบับเต็ม</h1>')
    for w in workloads:
        rows = ""
        for l in LANGS:
            d = summary[w][l]
            wv, cv, mv = d["wall"], d["cpu"], d["maxrss_mb"]
            rows += (f'<tr><td>{LANG_NAME[l]}</td><td class="r">{fmt_s(wv["median"])}</td><td class="r">{fmt_s(wv["mean"])}</td>'
                     f'<td class="r">{fmt_s(wv["min"])}</td><td class="r">{fmt_s(wv["max"])}</td><td class="r">{wv["stdev"]:.4f}</td>'
                     f'<td class="r">±{wv["ci95_half"]:.4f}</td><td class="r">{fmt_s(cv["median"])}</td><td class="r">{mv["median"]:.1f}</td></tr>')
        P(f'''<table class="data tiny"><caption>{WORKLOAD_TH[w][0]} ({w}) — n = {th(rounds)} ต่อภาษา</caption>
<thead><tr><th>ภาษา</th><th>มัธยฐาน (s)</th><th>ค่าเฉลี่ย</th><th>ต่ำสุด</th><th>สูงสุด</th><th>ส่วนเบี่ยงเบน</th><th>ช่วงเชื่อมั่น 95%</th><th>เวลา CPU</th><th>RSS (MB)</th></tr></thead>
<tbody>{rows}</tbody></table>''')
    P('</section>')

    out_rows = "".join(
        f'<tr><td>{WORKLOAD_TH[w][0]}</td><td class="mono small pre">{esc(outputs[f"{w}/arc"].strip())}</td></tr>' for w in workloads)
    P(f'''<section class="page"><h1 class="center">ภาคผนวก ข<br>ผลลัพธ์ที่ใช้ตรวจสอบความถูกต้อง</h1>
<p class="indent">ทุกโจทย์ต้องให้ผลลัพธ์ตรงกันทุกตัวอักษรในทั้ง ๕ ภาษา จึงจะเข้าสู่ขั้นตอนการวัด ตารางนี้แสดงผลลัพธ์ที่ทั้ง ๕ ภาษาพิมพ์ออกมาตรงกัน</p>
<table class="data"><thead><tr><th>โจทย์</th><th>ผลลัพธ์ (เหมือนกันทั้ง ๕ ภาษา)</th></tr></thead><tbody>{out_rows}</tbody></table>
</section>
<section class="page"><h1 class="center">ภาคผนวก ค<br>สภาพแวดล้อมและการทำซ้ำ</h1>
{analysis_html["appendix_env"]}
<table class="data small"><tbody>
<tr><td>วันเวลาเริ่มวัด</td><td class="mono">{esc(env["date"])}</td></tr>
<tr><td>วันเวลาสิ้นสุด</td><td class="mono">{esc(env.get("finished", ""))}</td></tr>
<tr><td>หน่วยประมวลผล</td><td>{esc(env["cpu"])} ({th(env["cores"])} แกน)</td></tr>
<tr><td>ระบบปฏิบัติการ</td><td class="mono">{esc(env["os"])}</td></tr>
<tr><td>แหล่งพลังงาน</td><td class="mono">{esc(env["power"])}</td></tr>
<tr><td>โหมดประหยัดพลังงาน</td><td class="mono">{esc(env["low_power_mode"])}</td></tr>
<tr><td>ภาระระบบ (load average) ก่อน/หลัง</td><td class="mono">{esc(", ".join(f"{x:.2f}" for x in env["load_avg"]))} / {esc(", ".join(f"{x:.2f}" for x in env.get("load_avg_end", [])))}</td></tr>
<tr><td>Random seed ของลำดับการรัน</td><td class="mono">{esc(env["seed"])}</td></tr>
</tbody></table>
</section>''')

    body = "".join(parts)
    return TEMPLATE.replace("{{BODY}}", body)


def statistics_median(xs):
    xs = sorted(xs)
    n = len(xs)
    return xs[n // 2] if n % 2 else (xs[n // 2 - 1] + xs[n // 2]) / 2


TEMPLATE = r'''<!doctype html>
<html lang="th"><head><meta charset="utf-8"><title>รายงานผลการทดสอบสมรรถนะภาษา Arc</title>
<style>
@page { size: A4; margin: 2.5cm 2cm 2cm 3cm;
  @top-center { content: "- " counter(page, thai) " -"; font-family: "TH Sarabun New"; font-size: 16pt; }
}
@page :first { @top-center { content: none; } }
html { font-family: "TH Sarabun New", "Sarabun", "Thonburi", sans-serif; font-size: 16pt; color: #0b0b0b; background: #fff; }
body { margin: 0; line-height: 1.32; }
.page { break-before: page; }
h1 { font-size: 18pt; font-weight: bold; margin: 14pt 0 6pt; }
h1.center { text-align: center; }
h1, .keep { break-after: avoid; }
.pair { display: flex; align-items: flex-start; justify-content: center; gap: 18pt; break-inside: avoid; }
.pair table.data.narrow { margin: 6pt 0 0; }
.pair figure { margin: 0; }
table.data.compact { font-size: 12.5pt; line-height: 1.15; }
table.data.compact td, table.data.compact th { padding: 1pt 5pt; }
.pair td { white-space: nowrap; }
.sign { break-inside: avoid; }
p { margin: 0 0 6pt; text-align: left; }
p.indent, .indent { text-indent: 2.5em; }
ol.th, ul.th { margin: 0 0 6pt 1.2em; padding-left: 1.4em; }
ol.th { list-style-type: thai; }
li { margin-bottom: 2pt; }
b { font-weight: bold; }
.mono { font-family: Menlo, monospace; font-size: 10.5pt; }
.small { font-size: 13pt; }
.mono.small { font-size: 9pt; }
.pre { white-space: pre-wrap; }
.note { font-size: 14pt; color: #333; }
/* cover */
.cover { text-align: center; padding-top: 0; }
.docno { text-align: right; font-size: 14pt; margin-bottom: 6cm; }
.cover-title { font-size: 26pt; font-weight: bold; }
.cover-sub { font-size: 22pt; font-weight: bold; margin-top: 8pt; }
.cover-sub2 { font-size: 18pt; margin-top: 10pt; }
.cover-meta { margin: 3.5cm auto 0; border-collapse: collapse; font-size: 16pt; text-align: left; }
.cover-meta td { padding: 3pt 12pt; vertical-align: top; }
.cover-meta td:first-child { font-weight: bold; white-space: nowrap; }
/* tables */
table.data { width: 100%; border-collapse: collapse; margin: 6pt 0 10pt; font-size: 14pt; break-inside: avoid; }
table.data.narrow { width: auto; margin-left: auto; margin-right: auto; }
table.data.tiny { font-size: 12.5pt; line-height: 1.15; }
table.data.tiny td, table.data.tiny th { padding: 1pt 5pt; }
table.data caption { caption-side: top; text-align: left; font-weight: bold; padding-bottom: 4pt; font-size: 15pt; }
table.data th, table.data td { border: 0.75pt solid #555; padding: 2pt 6pt; vertical-align: top; }
table.data th { background: #efefef; font-weight: bold; text-align: center; }
td.r { text-align: right; white-space: nowrap; }
td.c { text-align: center; }
td.best { font-weight: bold; background: #f3f3f3; }
tr.total td { font-weight: bold; border-top: 1.5pt solid #333; }
.sw { display: inline-block; width: 10pt; height: 10pt; border-radius: 50%; margin-right: 5pt; vertical-align: -1pt; }
/* figures */
figure { margin: 8pt 0 10pt; text-align: center; break-inside: avoid; }
figcaption, .figcap { font-size: 14pt; text-align: center; margin-top: 2pt; font-weight: bold; }
.multiples { display: grid; grid-template-columns: 1fr 1fr; gap: 4pt 14pt; }
.panel { break-inside: avoid; }
svg { font-family: "TH Sarabun New", sans-serif; overflow: visible; }
svg .ct { font-size: 14pt; font-weight: bold; fill: #0b0b0b; }
svg .cat { font-size: 13pt; fill: #0b0b0b; }
svg .tick { font-size: 11pt; fill: #52514e; }
svg .axis { font-size: 11pt; fill: #52514e; }
svg .val { font-size: 13pt; fill: #0b0b0b; font-weight: bold; }
svg .grid { stroke: #e2e1dc; stroke-width: 1; }
.sign { margin-top: 18pt; display: flex; justify-content: space-between; gap: 12pt; }
.sign div { text-align: center; }
.sign p { text-align: center; margin: 0 0 2pt; }
</style></head><body>
{{BODY}}
</body></html>
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pdf", default=os.path.join(HERE, "Arc_Benchmark_Report.pdf"))
    a = ap.parse_args()
    analysis = {}
    current = None
    for line in open(os.path.join(HERE, "analysis_th.html"), encoding="utf-8"):
        if line.startswith("@@"):
            current = line.strip()[2:]
            analysis[current] = ""
        elif current:
            analysis[current] += line
    doc = build_html(analysis)
    html_path = os.path.join(RES, "report.html")
    with open(html_path, "w", encoding="utf-8") as f:
        f.write(doc)
    profile = os.path.join(OUT, "tmp", "chrome-profile")
    os.makedirs(profile, exist_ok=True)
    subprocess.run([CHROME, "--headless=new", "--disable-gpu", "--no-pdf-header-footer", f"--user-data-dir={profile}",
                    f"--print-to-pdf={a.pdf}", "file://" + html_path], check=True, capture_output=True)
    print("wrote", a.pdf)


if __name__ == "__main__":
    main()
