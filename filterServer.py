#!/usr/bin/env python3

import os
import requests
import numpy as np
from datetime import datetime
import cv2

ESP32_IP   = "172.18.188.140"  
ESP32_URL  = f"http://{ESP32_IP}/photo"
OUTPUT_DIR = "photobooth_output"
os.makedirs(OUTPUT_DIR, exist_ok=True)

FILTERS = {
    "f": "none",
    "v": "vintage",
    "b": "blackWhite",
    "c": "colorPop",
}

TRANSLATIONS = {
    "rock": "ROCK ✊",
    "paper": "PAPER ✋",
    "scissors": "SCISSORS ✌️",
    "none": "NONE 🚫"
}

def main():
    print("=== SenseCraft AI Photobooth ===")
    print("Keys: [f] no filter  [v] vintage  [b] B&W  [c] colorPop  [q] quit")
    print(f"ESP32 URL: {ESP32_URL}\n")

    while True:
        key = input("Filter > ").strip().lower()

        if key == "q":
            break

        if key not in FILTERS:
            print(f"Invalid key. Use: {list(FILTERS.keys())}")
            continue

        filter_name = FILTERS[key]
        take_photo(filter_name)


def take_photo(filter_name: str):
    print(f"[→] Requesting frame with filter: {filter_name}")

    try:
        response = requests.get(ESP32_URL, params={"filter": filter_name}, timeout=10)
        response.raise_for_status()
    except requests.exceptions.RequestException as e:
        print(f"[✗] Connection error: {e}")
        return

    gesture_raw = response.headers.get("X-Gesto", "none")
    confidence = response.headers.get("X-Confianza", "0")
    gesture_text = TRANSLATIONS.get(gesture_raw, "UNKNOWN")

    img = cv2.imdecode(np.frombuffer(response.content, dtype=np.uint8), cv2.IMREAD_COLOR)
    if img is None:
        print("[✗] Decode error")
        return

    print(f"[✓] Image received {img.shape} — Gesture: {gesture_text} ({confidence}%)")

    result = apply_filter(img, filter_name)

    cv2.putText(result, f"Gesture: {gesture_text}", (20, 40), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 255), 2, cv2.LINE_AA)

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filepath  = f"{OUTPUT_DIR}/{timestamp}_{gesture_raw}_{filter_name}.jpg"
    cv2.imwrite(filepath, result)
    print(f"[✓] Saved: {filepath}")

    cv2.imshow(f"Photobooth — {gesture_text}", result)
    cv2.waitKey(3000)
    cv2.destroyAllWindows()


def apply_filter(img: np.ndarray, name: str) -> np.ndarray:
    filters = {
        "none":       lambda i: i,
        "vintage":    vintage,
        "blackWhite": black_and_white,
        "colorPop":   color_pop,
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
    main()
