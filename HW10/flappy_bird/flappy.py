import pgzrun
import serial
import serial.tools.list_ports
import threading

# ── window ────────────────────────────────────────────────────────────────────
WIDTH  = 480
HEIGHT = 480
TITLE  = "Flappy Pico"

# ── serial ────────────────────────────────────────────────────────────────────
# change this to your Pico's port e.g. "COM3" on Windows or "/dev/tty.usbmodem..." on Mac
SERIAL_PORT = "/dev/tty.usbmodem2101"
BAUD        = 115200

button_pressed = False
ser = None

def find_pico_port():
    """Try to auto-detect the Pico's serial port."""
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if "usbmodem" in p.device or "usbserial" in p.device or "COM" in p.device:
            return p.device
    return SERIAL_PORT

def serial_thread():
    """Read serial in background thread, update button_pressed."""
    global button_pressed, ser
    try:
        port = find_pico_port()
        ser = serial.Serial(port, BAUD, timeout=1)
        print(f"Connected to Pico on {port}")
        while True:
            line = ser.readline().decode("utf-8").strip()
            if line == "1":
                button_pressed = True
            elif line == "0":
                button_pressed = False
    except Exception as e:
        print(f"Serial error: {e}")
        print("Running without Pico — use SPACE key instead")

threading.Thread(target=serial_thread, daemon=True).start()

# ── game state ────────────────────────────────────────────────────────────────
GRAVITY     =  0.5
FLAP_FORCE  = -8.0
PIPE_SPEED  =  3.0
PIPE_GAP    = 140
PIPE_WIDTH  =  60
PIPE_EVERY  = 90   # frames between pipes

bird_x  = 100
bird_y  = HEIGHT // 2
bird_vy = 0.0

pipes   = []   # list of dicts: {x, gap_y}
score   = 0
hi_score = 0
frame   = 0
alive   = True
flap_consumed = False   # so one press = one flap

import random

def reset():
    global bird_y, bird_vy, pipes, score, frame, alive, flap_consumed
    bird_y        = HEIGHT // 2
    bird_vy       = 0.0
    pipes         = []
    score         = 0
    frame         = 0
    alive         = True
    flap_consumed = False

reset()

# ── helpers ───────────────────────────────────────────────────────────────────
def spawn_pipe():
    gap_y = random.randint(100, HEIGHT - 100)
    pipes.append({"x": WIDTH + PIPE_WIDTH, "gap_y": gap_y})

def bird_rect():
    return Rect(bird_x - 14, int(bird_y) - 14, 28, 28)

def pipe_rects(p):
    top    = Rect(p["x"], 0, PIPE_WIDTH, p["gap_y"] - PIPE_GAP // 2)
    bottom = Rect(p["x"], p["gap_y"] + PIPE_GAP // 2, PIPE_WIDTH, HEIGHT)
    return top, bottom

def check_flap():
    """Return True if flap should fire this frame."""
    global button_pressed, flap_consumed
    # physical button
    if button_pressed and not flap_consumed:
        flap_consumed = True
        return True
    if not button_pressed:
        flap_consumed = False
    return False

# ── pgz callbacks ─────────────────────────────────────────────────────────────
def update():
    global bird_y, bird_vy, pipes, score, frame, alive, hi_score

    if not alive:
        return

    frame += 1

    # flap
    if check_flap():
        bird_vy = FLAP_FORCE

    # gravity
    bird_vy += GRAVITY
    bird_y  += bird_vy

    # spawn pipes
    if frame % PIPE_EVERY == 0:
        spawn_pipe()

    # move pipes + score
    for p in pipes:
        p["x"] -= PIPE_SPEED
        # score when bird passes pipe center
        if p["x"] + PIPE_WIDTH < bird_x < p["x"] + PIPE_WIDTH + PIPE_SPEED + 1:
            score += 1
            if score > hi_score:
                hi_score = score

    # remove off-screen pipes
    pipes[:] = [p for p in pipes if p["x"] + PIPE_WIDTH > 0]

    # collision with floor/ceiling
    if bird_y < 0 or bird_y > HEIGHT:
        alive = False
        return

    # collision with pipes
    br = bird_rect()
    for p in pipes:
        top, bottom = pipe_rects(p)
        if br.colliderect(top) or br.colliderect(bottom):
            alive = False
            return

def draw():
    # sky
    screen.fill((135, 206, 235))

    # pipes
    for p in pipes:
        top, bottom = pipe_rects(p)
        screen.draw.filled_rect(top,    (34, 139, 34))
        screen.draw.filled_rect(bottom, (34, 139, 34))
        # pipe outlines
        screen.draw.rect(top,    (0, 100, 0))
        screen.draw.rect(bottom, (0, 100, 0))
        # pipe caps
        cap_w = PIPE_WIDTH + 8
        screen.draw.filled_rect(
            Rect(p["x"] - 4, top.bottom - 12, cap_w, 12), (34, 139, 34))
        screen.draw.filled_rect(
            Rect(p["x"] - 4, bottom.top, cap_w, 12), (34, 139, 34))

    # ground
    screen.draw.filled_rect(Rect(0, HEIGHT - 20, WIDTH, 20), (210, 180, 100))

    # bird (circle since pgz can't draw filled polygons easily)
    screen.draw.filled_circle((bird_x, int(bird_y)), 14, (255, 220, 50))
    screen.draw.circle((bird_x, int(bird_y)), 14, (200, 160, 0))
    # eye
    screen.draw.filled_circle((bird_x + 6, int(bird_y) - 4), 4, (255, 255, 255))
    screen.draw.filled_circle((bird_x + 7, int(bird_y) - 4), 2, (0, 0, 0))

    # score
    screen.draw.text(str(score), center=(WIDTH // 2, 40),
                     fontsize=48, color=(255, 255, 255), shadow=(1,1))

    # game over screen
    if not alive:
        screen.draw.text("GAME OVER",
                         center=(WIDTH // 2, HEIGHT // 2 - 40),
                         fontsize=52, color=(255, 60, 60), shadow=(2, 2))
        screen.draw.text(f"Score: {score}   Best: {hi_score}",
                         center=(WIDTH // 2, HEIGHT // 2 + 10),
                         fontsize=28, color=(255, 255, 255), shadow=(1, 1))
        screen.draw.text("Press SPACE or button to restart",
                         center=(WIDTH // 2, HEIGHT // 2 + 50),
                         fontsize=22, color=(220, 220, 220), shadow=(1, 1))

def on_key_down(key):
    """Space bar as fallback if no Pico connected."""
    global button_pressed
    if key == keys.SPACE:
        button_pressed = True
        if not alive:
            reset()

def on_key_up(key):
    global button_pressed
    if key == keys.SPACE:
        button_pressed = False

def on_mouse_down():
    """Also allow mouse click as fallback."""
    global button_pressed
    button_pressed = True
    if not alive:
        reset()

def on_mouse_up():
    global button_pressed
    button_pressed = False

pgzrun.go()