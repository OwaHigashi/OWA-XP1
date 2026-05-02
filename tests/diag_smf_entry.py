"""Capture serial log around SMF Player entry."""
import sys, time, serial

PORT = "COM3"
BAUD = 115200

def drain(s, label, timeout=2.0):
    end = time.time() + timeout
    s.timeout = 0.1
    out = []
    while time.time() < end:
        chunk = s.read(4096)
        if chunk:
            out.append(chunk.decode("utf-8", "replace"))
            end = time.time() + timeout
    text = "".join(out)
    if text:
        print(f"--- {label} ---")
        print(text, end="" if text.endswith("\n") else "\n")
        sys.stdout.flush()

def send(s, cmd, label=None, wait=0.4):
    s.write((cmd + "\n").encode("ascii"))
    s.flush()
    print(f">>> {cmd}")
    sys.stdout.flush()
    time.sleep(wait)
    drain(s, label or f"after {cmd}", timeout=1.0)

def main():
    s = serial.Serial(PORT, BAUD, timeout=0.1)
    drain(s, "boot/idle", timeout=2.0)
    send(s, "STATUS")
    # The C-short hotkey isn't via MODE; force SMF entry via BUTTON injection
    # if available, or via touch (tested via direct entry). Try MODE first.
    send(s, "MODE PLAY", wait=0.4)
    drain(s, "after MODE PLAY", timeout=0.5)
    # Trigger C short via the BUTTON command if supported.
    send(s, "BUTTON C", wait=0.6)
    drain(s, "after BUTTON C (1)", timeout=1.5)
    s.close()

if __name__ == "__main__":
    main()
