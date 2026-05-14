#!/usr/bin/env python3

import os
import numpy as np
from datetime import datetime
from flask import Flask, request, jsonify
import cv2

app = Flask(__name__)
OUTPUT_DIR = "photobooth_output"
os.makedirs(OUTPUT_DIR, exist_ok=True)


@app.route("/photo", methods=["POST"])
def receive_photo():
    filter_name = request.args.get("filter", "none")
    jpeg_data   = request.data

    if not jpeg_data:
        return jsonify({"error": "No image data"}), 400

    img = cv2.imdecode(np.frombuffer(jpeg_data, dtype=np.uint8), cv2.IMREAD_COLOR)
    if img is None:
        return jsonify({"error": "Could not decode image"}), 400

    print(f"[SERVER] Received {img.shape} — filter: {filter_name}")

    result = apply_filter(img, filter_name)

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filepath  = f"{OUTPUT_DIR}/{timestamp}_{filter_name}.jpg"
    cv2.imwrite(filepath, result)
    print(f"[SERVER] Saved: {filepath}")

    cv2.imshow(f"Photobooth — {filter_name}", result)
    cv2.waitKey(3000)
    cv2.destroyAllWindows()

    return jsonify({"ok": True, "filter": filter_name, "file": filepath})


# ── Filters ──────────────────────────────────────────────────

def apply_filter(img: np.ndarray, name: str) -> np.ndarray:
    filters = {
        "none":       lambda i: i,
        "vintage":    vintage,
        "blackWhite": black_and_white,
        "colorPop":   color_pop,
        "faceFrame":  face_frame,
    }
    return filters.get(name, filters["none"])(img)


def vintage(img: np.ndarray) -> np.ndarray:
    f = img.astype(np.float32) / 255.0
    b, g, r = f[:,:,0], f[:,:,1], f[:,:,2]
    f[:,:,0] = np.clip(b*0.131 + g*0.534 + r*0.272, 0, 1)
    f[:,:,1] = np.clip(b*0.168 + g*0.686 + r*0.349, 0, 1)
    f[:,:,2] = np.clip(b*0.189 + g*0.769 + r*0.393, 0, 1)
    f  = vignette(f, strength=0.5)
    noise = np.random.normal(0, 0.04, f.shape).astype(np.float32)
    return (np.clip(f + noise, 0, 1) * 255).astype(np.uint8)


def black_and_white(img: np.ndarray) -> np.ndarray:
    gray  = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    clahe = cv2.createCLAHE(clipLimit=2.5, tileGridSize=(8, 8))
    gray  = clahe.apply(gray)
    out   = cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
    f     = vignette(out.astype(np.float32) / 255.0, strength=0.6)
    return (f * 255).astype(np.uint8)


def color_pop(img: np.ndarray) -> np.ndarray:
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV).astype(np.float32)
    hsv[:,:,1] = np.clip(hsv[:,:,1] * 1.8, 0, 255)
    hsv[:,:,2] = np.clip(hsv[:,:,2] * 1.1, 0, 255)
    boosted = cv2.cvtColor(hsv.astype(np.uint8), cv2.COLOR_HSV2BGR)
    return cv2.LUT(boosted, _s_curve_lut())


def face_frame(img: np.ndarray) -> np.ndarray:
    out      = img.copy()
    detector = cv2.CascadeClassifier(cv2.data.haarcascades + "haarcascade_frontalface_default.xml")
    faces    = detector.detectMultiScale(cv2.cvtColor(img, cv2.COLOR_BGR2GRAY), 1.1, 5)

    for (x, y, w, h) in faces:
        cv2.rectangle(out, (x-10, y-10), (x+w+10, y+h+10), (0, 200, 255), 3)
        cv2.putText(out, "CHEESE!", (x, y-20), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 200, 255), 2)

    h_img, w_img = out.shape[:2]
    border = 20
    cv2.rectangle(out, (border, border), (w_img-border, h_img-border), (255, 255, 255), border)
    return out


# ── Helpers ──────────────────────────────────────────────────

def vignette(img_f: np.ndarray, strength: float = 0.5) -> np.ndarray:
    h, w   = img_f.shape[:2]
    xx, yy = np.meshgrid(np.arange(w) - w/2, np.arange(h) - h/2)
    mask   = np.exp(-((xx**2)/(2*(w/2)**2) + (yy**2)/(2*(h/2)**2)))
    mask   = 1 - strength * (1 - mask)
    return img_f * mask[:,:,np.newaxis]


def _s_curve_lut() -> np.ndarray:
    x   = np.arange(256, dtype=np.float32) / 255.0
    lut = 1.0 / (1.0 + np.exp(-8.0 * (x - 0.5)))
    lut = ((lut - lut.min()) / (lut.max() - lut.min()) * 255).astype(np.uint8)
    return lut


if __name__ == "__main__":
    print("Photobooth filter server running on :5000")
    app.run(host="0.0.0.0", port=5000, debug=False)
