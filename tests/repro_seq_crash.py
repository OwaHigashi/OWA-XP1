"""
Reproduce the crash when switching from INSTANT to SEQUENCE mode.

Procedure:
1. Open COM3 @ 115200, give the device a moment to boot.
2. Send `STATUS` to confirm it is alive.
3. Send `GROUP TRANSPOSE` to leave PLAY mode.
4. Cycle DIRECT -> KEY -> INSTANT -> SEQUENCE via MODE commands.
5. Capture all serial output, including any ESP32 panic dump after the
   reset that follows the hang.
"""
import sys
import time
import serial

PORT = "COM3"
BAUD = 115200


def drain(s, label, timeout=1.5):
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
    return text


def send(s, cmd, label=None, wait=0.5):
    s.write((cmd + "\n").encode("ascii"))
    s.flush()
    print(f">>> {cmd}")
    sys.stdout.flush()
    time.sleep(wait)
    return drain(s, label or f"after {cmd}")


def main():
    s = serial.Serial(PORT, BAUD, timeout=0.1)
    # Drain anything still in the buffer from boot or previous session.
    drain(s, "boot/idle", timeout=2.0)

    send(s, "STATUS", "STATUS")
    send(s, "GROUP TRANSPOSE", "after GROUP TRANSPOSE", wait=0.4)
    send(s, "MODE DIRECT", wait=0.4)
    send(s, "MODE KEY", wait=0.4)
    send(s, "MODE INSTANT", wait=0.4)
    # The suspected crash trigger:
    send(s, "MODE SEQUENCE", wait=0.4)

    # After SEQUENCE, the device may panic and reboot. Wait long enough
    # to capture the full panic dump and the new boot output.
    drain(s, "post-SEQUENCE (5s)", timeout=5.0)

    s.close()


if __name__ == "__main__":
    main()
