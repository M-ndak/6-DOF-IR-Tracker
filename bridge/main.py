# tracker_udp_bridge.py
import cv2
import numpy as np
import serial
import threading
import time
import socket
import json
import sys

# ── CONFIG ────────────────────────────────────────────────────────────────────
SERIAL_PORT   = "COM13"
BAUD_RATE     = 115200
CAMERA_INDEX  = 0
USE_SERIAL    = True
USE_CAMERA    = True

UE5_IP        = "127.0.0.1"   # change if UE5 runs on another machine
UE5_PORT      = 12345          # must match the UE5 Blueprint UDP port

# ── CAMERA CALIBRATION ────────────────────────────────────────────────────────
camera_matrix = np.array([[800, 0, 320],
                           [0, 800, 240],
                           [0,   0,   1]], dtype=np.float32)
dist_coeffs   = np.zeros((4, 1))

# ── LED CONSTELLATION (mm, object space) ──────────────────────────────────────
OBJECT_POINTS = np.array([
    [ 0.0,  40.0, 0.0],
    [-30.0, -20.0, 0.0],
    [ 30.0, -20.0, 0.0],
], dtype=np.float32)

# ── SHARED STATE ──────────────────────────────────────────────────────────────
state_lock = threading.Lock()
state = {
    "tracking": False,
    "tvec":     None,
    "rvec":     None,
    "led_world": None,
    "roll": 0.0, "pitch": 0.0, "yaw": 0.0,
    "accX": 0.0, "accY": 0.0, "accZ": 0.0,
    "demo_t": 0.0,
}

# ── UDP SOCKET ────────────────────────────────────────────────────────────────
udp_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_udp(payload: dict):
    try:
        data = json.dumps(payload).encode("utf-8")
        udp_sock.sendto(data, (UE5_IP, UE5_PORT))
    except Exception as e:
        print(f"[udp] Send error: {e}")

# ── SERIAL READER THREAD ──────────────────────────────────────────────────────
def serial_thread_fn():
    if not USE_SERIAL:
        return
    print(f"[serial] Connecting to {SERIAL_PORT} ...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        time.sleep(2)
        ser.reset_input_buffer()
        print("[serial] Connected.")
        while True:
            if ser.in_waiting:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if line.startswith("TELEMETRY,"):
                    parts = line.split(",")
                    if len(parts) == 7:
                        try:
                            with state_lock:
                                state["roll"]  = float(parts[1])
                                state["pitch"] = float(parts[2])
                                state["yaw"]   = float(parts[3])
                                state["accX"]  = float(parts[4])
                                state["accY"]  = float(parts[5])
                                state["accZ"]  = float(parts[6])
                        except ValueError:
                            pass
    except Exception as e:
        print(f"[serial] Error: {e}")

# ── CAMERA / BLOB THREAD ──────────────────────────────────────────────────────
def camera_thread_fn():
    if not USE_CAMERA:
        return
    cap = cv2.VideoCapture(CAMERA_INDEX)
    if not cap.isOpened():
        print("[camera] Could not open camera.")
        return

    params = cv2.SimpleBlobDetector_Params()
    params.filterByColor       = True;  params.blobColor = 255
    params.filterByArea        = True;  params.minArea   = 4; params.maxArea = 400
    params.filterByCircularity = False
    params.filterByConvexity   = False
    params.filterByInertia     = False
    detector = cv2.SimpleBlobDetector_create(params)

    print("[camera] Running.")
    while True:
        ret, frame = cap.read()
        if not ret:
            time.sleep(0.01)
            continue

        gray      = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        keypoints = detector.detect(gray)
        img_pts   = sorted([kp.pt for kp in keypoints], key=lambda p: p[1])

        tracking  = False
        led_world = None
        tvec_out  = None
        rvec_out  = None

        if len(img_pts) == 3:
            img_pts_np = np.array(img_pts, dtype=np.float32)
            ok, rvec, tvec = cv2.solvePnP(
                OBJECT_POINTS, img_pts_np,
                camera_matrix, dist_coeffs,
                flags=cv2.SOLVEPNP_SQPNP)
            if ok:
                tracking = True
                rvec_out = rvec.ravel()
                tvec_out = tvec.ravel()
                R, _     = cv2.Rodrigues(rvec)
                led_world = np.array([
                    R @ OBJECT_POINTS[i] + tvec.ravel() for i in range(3)
                ])

        with state_lock:
            state["tracking"]  = tracking
            state["led_world"] = led_world
            state["tvec"]      = tvec_out
            state["rvec"]      = rvec_out

        # Optional preview
        for kp in keypoints:
            cv2.circle(frame, (int(kp.pt[0]), int(kp.pt[1])), 5, (0, 255, 0), -1)
        status = "TRACKING" if tracking else f"LOST {len(img_pts)}/3"
        cv2.putText(frame, status, (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7,
                    (0, 255, 0) if tracking else (0, 0, 255), 2)
        cv2.imshow("IR Camera", frame)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyAllWindows()

# ── DEMO MODE ─────────────────────────────────────────────────────────────────
def demo_tick(dt=0.033):
    with state_lock:
        t = state["demo_t"] + dt
        state["demo_t"] = t
        x = np.sin(t * 0.5) * 80
        y = np.cos(t * 0.4) * 50
        z = 300 + np.sin(t * 0.3) * 80
        roll  = np.sin(t * 0.6) * 40
        pitch = np.cos(t * 0.7) * 25
        yaw   = (t * 20) % 360
        state.update(roll=roll, pitch=pitch, yaw=yaw,
                     accX=float(np.sin(t)*2),
                     accY=float(np.cos(t)*1.5),
                     accZ=float(9.8 + np.sin(t*1.3)*0.5),
                     tracking=True,
                     tvec=np.array([x, y, z]))

# ── UDP SENDER LOOP ───────────────────────────────────────────────────────────
def udp_sender_loop(demo_mode=False):
    """Runs at ~60 Hz, assembles packet and sends to UE5."""
    print(f"[udp] Sending to {UE5_IP}:{UE5_PORT} at ~60 Hz")
    while True:
        t0 = time.time()

        if demo_mode:
            demo_tick()

        with state_lock:
            tracking  = state["tracking"]
            tvec      = state["tvec"]
            led_world = state["led_world"]
            roll      = state["roll"]
            pitch     = state["pitch"]
            yaw       = state["yaw"]
            accX      = state["accX"]
            accY      = state["accY"]
            accZ      = state["accZ"]

        # Build LED positions list (camera-space mm)
        leds = []
        if led_world is not None:
            for i in range(3):
                leds.append({
                    "x": float(led_world[i][0]),
                    "y": float(led_world[i][1]),
                    "z": float(led_world[i][2]),
                })

        packet = {
            "tracking": tracking,
            # Position from camera (mm) — convert to UE5 cm by /10
            "x":     float(tvec[0]) / 10.0 if tvec is not None else 0.0,
            "y":     float(tvec[1]) / 10.0 if tvec is not None else 0.0,
            "z":     float(tvec[2]) / 10.0 if tvec is not None else 0.0,
            # Orientation from IMU (degrees)
            "roll":  roll,
            "pitch": pitch,
            "yaw":   yaw,
            # Linear acceleration (m/s²)
            "accX":  accX,
            "accY":  accY,
            "accZ":  accZ,
            # Individual LED world positions (cm)
            "leds":  leds,
            # Timestamp
            "ts":    time.time(),
        }

        send_udp(packet)

        elapsed = time.time() - t0
        time.sleep(max(0, (1.0 / 60.0) - elapsed))

# ── MAIN ──────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    demo_mode = "--demo" in sys.argv or (not USE_SERIAL and not USE_CAMERA)

    if not demo_mode:
        threading.Thread(target=serial_thread_fn, daemon=True).start()
        threading.Thread(target=camera_thread_fn, daemon=True).start()
        time.sleep(1.5)   # let threads warm up

    print("=" * 50)
    print("  6DOF Tracker → UE5 UDP Bridge")
    print(f"  Target: {UE5_IP}:{UE5_PORT}")
    print(f"  Mode: {'DEMO' if demo_mode else 'LIVE'}")
    print("  Ctrl+C to quit")
    print("=" * 50)

    udp_sender_loop(demo_mode=demo_mode)