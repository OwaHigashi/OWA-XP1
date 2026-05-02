"""Probe device state across BUTTON C transitions."""
import time, serial

s = serial.Serial("COM3", 115200, timeout=0.1)
time.sleep(2.0)
s.reset_input_buffer()

def send(cmd, wait=0.5):
    s.write((cmd + "\n").encode("ascii"))
    s.flush()
    print(f">>> {cmd}")
    time.sleep(wait)
    end = time.time() + wait
    out = b""
    while time.time() < end:
        out += s.read(4096)
    text = out.decode("utf-8", "replace").strip()
    if text:
        print(text)
    return text

send("MODE PLAY", wait=0.4)
send("STATUS",    wait=0.4)
send("BUTTON C",  wait=3.0)
send("STATUS",    wait=0.4)

s.close()
