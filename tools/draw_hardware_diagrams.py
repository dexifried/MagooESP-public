"""Render the wiring guide's label-based, editable SVG diagrams."""
from html import escape
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "docs" / "images"
OUT.mkdir(parents=True, exist_ok=True)


def start(title, subtitle, height=600):
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{height}" viewBox="0 0 1200 {height}" role="img" aria-label="{escape(title)}">',
        '<rect width="100%" height="100%" rx="22" fill="#101317"/>',
        f'<text x="44" y="54" fill="#fff" font-family="Arial,sans-serif" font-size="30" font-weight="bold">{escape(title)}</text>',
        f'<text x="44" y="84" fill="#b8c1cb" font-family="Arial,sans-serif" font-size="18">{escape(subtitle)}</text>',
    ]


def box(s, x, y, w, h, title, sub=""):
    s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="14" fill="#1d242c" stroke="#566575" stroke-width="2"/>')
    label(s, x + 20, y + 35, title, 22, "#fff", "bold")
    if sub:
        label(s, x + 20, y + 62, sub, 16, "#bac5cf")


def label(s, x, y, value, size=19, color="#dce5ec", weight="normal", anchor="start"):
    s.append(f'<text x="{x}" y="{y}" text-anchor="{anchor}" fill="{color}" font-family="Arial,sans-serif" font-size="{size}" font-weight="{weight}">{escape(value)}</text>')


def wire(s, points, color, width=5, dashed=False):
    p = " ".join(f"{x},{y}" for x, y in points)
    dash = ' stroke-dasharray="9 7"' if dashed else ""
    s.append(f'<polyline points="{p}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"{dash}/>')


def dot(s, x, y, color):
    s.append(f'<circle cx="{x}" cy="{y}" r="6" fill="{color}"/>')


def finish(s, filename):
    s.append("</svg>")
    (OUT / filename).write_text("\n".join(s) + "\n", encoding="utf-8")


red, blue, green, grey, yellow = "#fb6472", "#70baff", "#6ee3b2", "#c3cbd3", "#f5ce70"

s = start("1  |  Power the dashboard", "The display is built into the original IdeaSpark board.", 420)
box(s, 65, 145, 300, 145, "USB 5 V SUPPLY", "Computer or USB power adapter")
box(s, 785, 125, 340, 185, "ESP32 + DISPLAY", "IdeaSpark integrated screen")
wire(s, [(365, 217), (785, 217)], red, 6)
label(s, 575, 197, "USB cable", 22, "#fff", "bold", "middle")
label(s, 575, 252, "connect to board USB port", 17, "#bac5cf", anchor="middle")
label(s, 65, 365, "External display? Use the display pin table in the guide.", 20, yellow)
finish(s, "01-esp.svg")

s = start("2  |  Connect the tilt sensor", "Match the printed pin names; the diagram does not show physical pin order.", 500)
box(s, 55, 120, 300, 275, "ESP32", "3.3 V logic")
box(s, 845, 120, 300, 275, "MPU SENSOR", "3.3 V-compatible breakout")
for y, left, right, color in [(210, "3V3", "VCC", red), (260, "GND", "GND", grey), (310, "D21 / GPIO21", "SDA", blue), (360, "D22 / GPIO22", "SCL", green)]:
    label(s, 330, y+7, left, 20, color, "bold", "end")
    wire(s, [(355, y), (845, y)], color)
    label(s, 870, y+7, right, 20, color, "bold")
label(s, 600, 453, "Mount Y front-to-back and X side-to-side; then set level.", 19, yellow, anchor="middle")
finish(s, "02-imu.svg")

s = start("3  |  Read the receiver throttle", "The ESC stays connected; the ESP only listens to the throttle signal.", 590)
box(s, 45, 150, 270, 235, "RC RECEIVER", "Throttle channel")
box(s, 835, 120, 320, 140, "ESC")
box(s, 835, 330, 320, 150, "ESP32")
label(s, 285, 222, "SIGNAL", 19, blue, "bold", "end")
label(s, 285, 352, "GROUND", 19, grey, "bold", "end")
wire(s, [(315, 215), (570, 215), (835, 195)], blue)
wire(s, [(570, 215), (570, 395), (835, 395)], blue)
dot(s, 570, 215, blue)
wire(s, [(315, 345), (735, 345), (735, 445), (835, 445)], grey)
label(s, 855, 202, "THROTTLE SIGNAL", 18, blue, "bold")
label(s, 855, 402, "D16 / GPIO16 / RX2", 18, blue, "bold")
label(s, 855, 452, "GND", 18, grey, "bold")
label(s, 475, 185, "Y SPLIT", 18, "#fff", "bold")
label(s, 600, 375, "Level-shift this branch if needed", 17, yellow)
label(s, 600, 535, "Receiver and ESC retain their normal power; do not connect servo + to the ESP32.", 19, yellow, anchor="middle")
finish(s, "03-throttle.svg")

s = start("4  |  Connect the winch controller", "GPIO5 controls the controller; it does not power the motor.", 650)
box(s, 35, 155, 280, 230, "ESP32", "3.3 V signal output")
box(s, 465, 135, 310, 280, "WINCH CONTROLLER", "RC servo-style input")
box(s, 895, 180, 270, 160, "WINCH MOTOR", "Controller motor outputs")
box(s, 470, 475, 300, 100, "BATTERY / SUPPLY", "As specified by controller")
label(s, 290, 242, "D5 / GPIO5", 19, blue, "bold", "end")
label(s, 290, 322, "GND", 19, grey, "bold", "end")
wire(s, [(315, 235), (465, 235)], blue)
wire(s, [(315, 315), (465, 315)], grey)
label(s, 485, 242, "SIGNAL IN", 19, blue, "bold")
label(s, 485, 322, "SIGNAL GND", 19, grey, "bold")
wire(s, [(775, 260), (895, 260)], red, 6)
wire(s, [(620, 475), (620, 415)], red, 6)
label(s, 783, 245, "motor leads", 15, "#fff")
label(s, 638, 449, "power leads", 16, "#fff")
label(s, 600, 615, "Controller signal + wire: insulate unless its manual specifies a safe power arrangement.", 18, yellow, anchor="middle")
finish(s, "04-winch.svg")

s = start("5  |  Optional battery voltage sensing", "INA219 voltage-only connection. Check polarity and sensor ratings first.", 680)
box(s, 35, 125, 305, 265, "ESP32", "3.3 V logic")
box(s, 845, 125, 320, 385, "INA219", "Read labels on your board")
box(s, 35, 475, 305, 145, "BATTERY")
for y, left, right, color in [(205, "3V3", "VCC", red), (255, "GND", "GND", grey), (305, "D25 / GPIO25", "SDA", blue), (355, "D13 / GPIO13", "SCL", green)]:
    label(s, 315, y+7, left, 18, color, "bold", "end")
    wire(s, [(340, y), (845, y)], color)
    label(s, 865, y+7, right, 18, color, "bold")
wire(s, [(340, 525), (565, 525), (565, 425), (845, 425)], red)
label(s, 315, 532, "POSITIVE +", 18, red, "bold", "end")
label(s, 865, 432, "VIN−", 19, red, "bold")
label(s, 865, 482, "VIN+  unused", 18, "#b8c1cb")
s.append('<rect x="398" y="510" width="98" height="30" rx="6" fill="#101317" stroke="#fb6472" stroke-width="2"/>')
label(s, 447, 531, "FUSE", 17, red, "bold", "middle")
wire(s, [(340, 575), (655, 575), (655, 255)], grey)
dot(s, 655, 255, grey)
label(s, 315, 582, "NEGATIVE −", 18, grey, "bold", "end")
label(s, 600, 651, "VIN+ and VIN− are NOT battery + and −. The speedometer’s 12.6 V is fixed artwork.", 18, yellow, anchor="middle")
finish(s, "05-battery.svg")
