"""Capture screenshots of the SMF Player screens.

Procedure:
1. Boot/idle drain.
2. Enter PLAY mode and capture (already exists 00-play.ppm — re-confirm).
3. Press BUTTON C → enters SMF Player (no song selected yet on real device,
   but on first entry we have 95 songs scanned). Capture.
4. Press BUTTON B → start playing (MIDI flows). Capture mid-playback.
5. Press BUTTON C LONG → exit SMF Player back to PLAY.
"""
import sys, time, struct, serial

PORT = "COM3"
BAUD = 115200

def read_until(s, marker, timeout=5.0):
    end = time.time() + timeout
    out = b""
    while time.time() < end:
        chunk = s.read(4096)
        if chunk:
            out += chunk
            if marker in out:
                return out
    return out

def screenshot(s, outpath):
    s.reset_input_buffer()
    s.write(b"SCREENSHOT PPM\n")
    s.flush()
    # Read header line ending with \n
    header = b""
    end = time.time() + 5.0
    while time.time() < end and b"\n" not in header:
        header += s.read(1)
    line = header.decode("utf-8", "replace").strip()
    print("HDR:", line)
    sys.stdout.flush()
    if not line.startswith("OK SCREENSHOT"):
        print("  unexpected header; aborting capture")
        return False
    parts = dict(p.split("=", 1) for p in line.split() if "=" in p)
    nbytes = int(parts.get("bytes", "0"))
    if nbytes <= 0:
        return False
    # Read body
    data = b""
    end = time.time() + 10.0
    while time.time() < end and len(data) < nbytes:
        data += s.read(min(4096, nbytes - len(data)))
    if len(data) != nbytes:
        print(f"  short read {len(data)}/{nbytes}")
        return False
    # Drain "OK SCREENSHOT_DONE"
    read_until(s, b"OK SCREENSHOT_DONE", timeout=2.0)
    with open(outpath, "wb") as f:
        f.write(data)
    print(f"  wrote {outpath} ({nbytes} bytes)")
    return True

def send(s, cmd, wait=0.5):
    s.write((cmd + "\n").encode("ascii"))
    s.flush()
    print(f">>> {cmd}")
    sys.stdout.flush()
    time.sleep(wait)

def main():
    s = serial.Serial(PORT, BAUD, timeout=0.1)
    time.sleep(2.0)
    s.reset_input_buffer()

    # 1) Reach PLAY first
    send(s, "MODE PLAY", wait=0.4)
    s.reset_input_buffer()

    # 2) Enter SMF Player
    send(s, "BUTTON C", wait=2.0)  # scanning takes time
    screenshot(s, "screenshots/09-smf-stop.ppm")

    # 3) Start playing (B = play/stop toggle)
    send(s, "BUTTON B", wait=2.0)
    screenshot(s, "screenshots/10-smf-playing.ppm")

    # 4) Stop and exit
    send(s, "BUTTON B", wait=0.5)  # stop
    send(s, "BUTTON C LONG", wait=1.0)
    s.close()

if __name__ == "__main__":
    main()
