/**
 * @file primitive.h
 * @brief Base class and concrete primitives for The Circle LED composition.
 *
 * Each primitive renders onto a circular LED buffer using angular coordinates.
 * Strip layout: 0° = 12-o'clock, clockwise to 360°.
 * All angles are in degrees (0–360 float).
 *
 * Threshold modifier: any primitive can optionally override its color based on
 * the bound HA entity value crossing configurable thresholds.
 *
 * Ref: ESPHome addressable light buffer API
 *   https://esphome.io/components/light/index.html#addressable-lambda
 */

#pragma once

#include "esphome/core/color.h"
#include <cmath>
#include <cstdint>
#include <string>

namespace esphome {
namespace the_circle {

// ── Maximum constants ────────────────────────────────────────────────────────
static const int MAX_COLORS = 4;

// ── Primitive type enum ──────────────────────────────────────────────────────
enum PrimitiveType : uint8_t {
  PRIM_NONE = 0,
  PRIM_DOT,
  PRIM_ARC,
  PRIM_TRAIL,
  PRIM_SOLID,
  PRIM_GRADIENT,
  PRIM_SEGMENT,
  PRIM_PULSE,
  PRIM_SPIN,
  PRIM_RAINBOW,
  PRIM_STROBE,
  PRIM_SPARKLE,
  PRIM_COMET,
  PRIM_THRESHOLD,
  PRIM_MAX
};

// ── Threshold modifier config ────────────────────────────────────────────────
// Applied on top of any primitive: overrides color based on bound value.
struct ThresholdConfig {
  bool enabled{false};
  float threshold1{33.0f};   // below this → color[0]
  float threshold2{66.0f};   // below this → color[1], above → color[2]
  Color colors[3]{
      Color(0, 255, 0),   // green  – low zone
      Color(255, 255, 0), // yellow – mid zone
      Color(255, 0, 0)    // red    – high zone
  };
};

// ── Helper: angle → LED index ────────────────────────────────────────────────
// Maps a degree (0–360) to a LED index on a strip of `num_leds`.
// Wraps around at 360° = 0°.
static inline int angle_to_led(float angle_deg, int num_leds) {
  // Normalize to [0, 360)
  float a = fmodf(angle_deg, 360.0f);
  if (a < 0) a += 360.0f;
  return ((int)(a * num_leds / 360.0f)) % num_leds;
}

// ── Base Primitive ───────────────────────────────────────────────────────────
/**
 * Abstract base for all visual primitives.
 * Subclasses implement render() which writes into a Color buffer.
 * The renderer calls render() for each active layer; painter's algorithm
 * means later layers overwrite earlier ones on the same LED.
 */
struct Primitive {
  PrimitiveType type{PRIM_NONE};

  // ── Visual params ──────────────────────────────────────────────────────
  Color colors[MAX_COLORS]{
      Color(255, 255, 255),
      Color(0, 0, 0),
      Color(0, 0, 0),
      Color(0, 0, 0)
  };
  uint8_t intensity{255};       // global brightness scaler 0–255
  float params[8]{0};           // meaning depends on subclass

  // ── HA entity binding ──────────────────────────────────────────────────
  std::string ha_entity_id;     // e.g. "sensor.forno_program_progress"
  float ha_value{0.0f};         // updated by subscription callback
  std::string ha_state_str;     // raw string state from HA (for non-numeric)
  float value_map_min{0.0f};    // HA value range min (maps to 0°/0%)
  float value_map_max{100.0f};  // HA value range max (maps to 360°/100%)
  bool ha_bound{false};         // true when entity_id is set

  // ── Threshold modifier ─────────────────────────────────────────────────
  ThresholdConfig threshold;

  // ── Runtime helpers ────────────────────────────────────────────────────

  /**
   * Map the raw HA value to a normalized 0–1 range using value_map_min/max.
   */
  float mapped_value() const {
    if (value_map_max == value_map_min) return 0.0f;
    float v = (ha_value - value_map_min) / (value_map_max - value_map_min);
    return fmaxf(0.0f, fminf(1.0f, v));
  }

  /**
   * Map the raw HA value to an angle in degrees (0–360).
   */
  float mapped_angle() const {
    return mapped_value() * 360.0f;
  }

  /**
   * Get the effective color considering threshold modifier.
   * If threshold is enabled, returns the threshold color matching the current
   * HA value; otherwise returns colors[color_index].
   */
  Color effective_color(int color_index = 0) const {
    Color c = colors[color_index];
    if (threshold.enabled && ha_bound) {
      float v = ha_value;  // use raw value for threshold comparison
      if (v < threshold.threshold1)
        c = threshold.colors[0];
      else if (v < threshold.threshold2)
        c = threshold.colors[1];
      else
        c = threshold.colors[2];
    }
    // Apply intensity scaling
    return Color(
        (uint8_t)((uint16_t)c.r * intensity / 255),
        (uint8_t)((uint16_t)c.g * intensity / 255),
        (uint8_t)((uint16_t)c.b * intensity / 255)
    );
  }

  /**
   * Render this primitive into a Color buffer.
   * @param buffer   Array of Color, one per LED
   * @param num_leds Total LEDs in the strip
   * @param now_ms   Current time in milliseconds (for animated primitives)
   *
   * Only non-BLACK pixels are written (painter's algorithm):
   * a primitive only writes the LEDs it "owns", leaving others untouched.
   */
  virtual void render(Color *buffer, int num_leds, uint32_t now_ms) = 0;

  virtual ~Primitive() = default;
};

// ═══════════════════════════════════════════════════════════════════════════
// Concrete Primitives – F1: dot, arc, trail, solid
// ═══════════════════════════════════════════════════════════════════════════

/**
 * DOT – Illuminates a single point (or small cluster) at a given angle.
 *
 * params[0] = angle (degrees), overridden by mapped_angle() if ha_bound
 * params[1] = spread (number of additional LEDs on each side, 0 = single LED)
 *
 * colors[0] = dot color
 */
struct DotPrimitive : Primitive {
  DotPrimitive() { type = PRIM_DOT; }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    // Determine angle: use HA-mapped angle if bound, else static param
    float angle = ha_bound ? mapped_angle() : params[0];
    int center = angle_to_led(angle, num_leds);
    int spread = (int)params[1];
    Color c = effective_color(0);

    // Write center + spread LEDs on each side
    for (int d = -spread; d <= spread; d++) {
      int idx = (center + d + num_leds) % num_leds;
      buffer[idx] = c;
    }
  }
};

/**
 * ARC – Illuminates an arc from start_angle to end_angle.
 *
 * params[0] = start angle (degrees)
 * params[1] = end angle (degrees)
 * When ha_bound: end angle = start + mapped_angle()
 *
 * colors[0] = arc color
 */
struct ArcPrimitive : Primitive {
  ArcPrimitive() { type = PRIM_ARC; }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float start = params[0];
    float end = ha_bound ? (start + mapped_angle()) : params[1];
    Color c = effective_color(0);

    int led_start = angle_to_led(start, num_leds);
    int led_end = angle_to_led(end, num_leds);

    // Handle wrap-around: iterate from start to end clockwise
    if (led_start <= led_end) {
      for (int i = led_start; i <= led_end; i++)
        buffer[i] = c;
    } else {
      // Wrap: start→end_of_strip, then 0→end
      for (int i = led_start; i < num_leds; i++)
        buffer[i] = c;
      for (int i = 0; i <= led_end; i++)
        buffer[i] = c;
    }
  }
};

/**
 * TRAIL – Progress bar from 0° up to a given angle.
 *         Equivalent to ARC with start=0° but semantically "fill from top".
 *
 * params[0] = end angle (degrees), overridden by mapped_angle() if ha_bound
 *
 * colors[0] = trail color
 */
struct TrailPrimitive : Primitive {
  TrailPrimitive() { type = PRIM_TRAIL; }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float end_angle = ha_bound ? mapped_angle() : params[0];
    int led_end = angle_to_led(end_angle, num_leds);
    Color c = effective_color(0);

    // Fill from LED 0 (12-o'clock) to led_end
    for (int i = 0; i <= led_end && i < num_leds; i++) {
      buffer[i] = c;
    }
  }
};

/**
 * SOLID – Entire strip filled with one color.
 *
 * No angle params needed.
 * colors[0] = fill color
 */
struct SolidPrimitive : Primitive {
  SolidPrimitive() { type = PRIM_SOLID; }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    Color c = effective_color(0);
    for (int i = 0; i < num_leds; i++) {
      buffer[i] = c;
    }
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Concrete Primitives – F2: gradient, segment, pulse, spin, rainbow,
//                           strobe, sparkle, comet
// ═══════════════════════════════════════════════════════════════════════════

/**
 * GRADIENT – Color gradient across an arc.
 *
 * params[0] = start angle (degrees)
 * params[1] = end angle (degrees)
 * When ha_bound: end = start + mapped_angle()
 *
 * colors[0] = start color
 * colors[1] = end color
 *
 * Ref: Spec – "sfumatura tra due colori su un arco"
 */
struct GradientPrimitive : Primitive {
  GradientPrimitive() { type = PRIM_GRADIENT; }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float start = params[0];
    float end = ha_bound ? (start + mapped_angle()) : params[1];

    int led_start = angle_to_led(start, num_leds);
    int led_end = angle_to_led(end, num_leds);

    // Calculate total span (handling wrap-around)
    int span = (led_end >= led_start)
                   ? (led_end - led_start)
                   : (num_leds - led_start + led_end);
    if (span == 0) return;

    Color c_start = effective_color(0);
    // End color: apply intensity but not threshold (threshold acts on primary)
    Color c_end = Color(
        (uint8_t)((uint16_t)colors[1].r * intensity / 255),
        (uint8_t)((uint16_t)colors[1].g * intensity / 255),
        (uint8_t)((uint16_t)colors[1].b * intensity / 255));

    // Interpolate across the arc
    for (int i = 0; i <= span; i++) {
      int idx = (led_start + i) % num_leds;
      float t = (float)i / (float)span;
      buffer[idx] = Color(
          (uint8_t)(c_start.r + t * (c_end.r - c_start.r)),
          (uint8_t)(c_start.g + t * (c_end.g - c_start.g)),
          (uint8_t)(c_start.b + t * (c_end.b - c_start.b)));
    }
  }
};

/**
 * SEGMENT – Up to 4 proportional segments filling the strip.
 *
 * params[0..3] = values for segments 1–4 (relative proportions)
 * colors[0..3] = colors for segments 1–4
 *
 * When ha_bound: params[0] is overridden by ha_value (useful for dynamic
 * first segment, e.g. consumption vs production).
 *
 * Ref: Spec – "fino a 4 segmenti proporzionali (tipo torta lineare)"
 */
struct SegmentPrimitive : Primitive {
  SegmentPrimitive() { type = PRIM_SEGMENT; }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float vals[4];
    vals[0] = ha_bound ? ha_value : params[0];
    vals[1] = params[1];
    vals[2] = params[2];
    vals[3] = params[3];

    float total = vals[0] + vals[1] + vals[2] + vals[3];
    if (total <= 0.0f) return;

    // Calculate LED counts per segment
    int led_pos = 0;
    for (int seg = 0; seg < 4; seg++) {
      int seg_leds = (int)(vals[seg] * num_leds / total);
      if (seg_leds <= 0) continue;

      Color c = effective_color(seg);
      for (int i = 0; i < seg_leds && led_pos < num_leds; i++) {
        buffer[led_pos++] = c;
      }
    }
  }
};

/**
 * PULSE – Solid color that breathes (sinusoidal fade in/out).
 *
 * params[0] = speed (cycles per minute, default 30)
 * params[1] = min brightness fraction (0.0–1.0, default 0.1)
 * params[2] = max brightness fraction (0.0–1.0, default 1.0)
 *
 * colors[0] = pulse color
 *
 * Ref: Spec – "solid che respira (fade in/out ciclico)"
 */
struct PulsePrimitive : Primitive {
  PulsePrimitive() {
    type = PRIM_PULSE;
    params[0] = 30.0f;  // 30 cycles/min = 2s period
    params[1] = 0.1f;   // min brightness 10%
    params[2] = 1.0f;   // max brightness 100%
  }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float speed = (params[0] > 0.0f) ? params[0] : 30.0f;
    float min_b = params[1];
    float max_b = params[2];

    // Sinusoidal oscillation: period = 60/speed seconds
    float period_ms = 60000.0f / speed;
    float phase = fmodf((float)now_ms, period_ms) / period_ms;
    // sin produces -1..1, map to min_b..max_b
    float breath = min_b + (max_b - min_b) * (0.5f + 0.5f * sinf(phase * 2.0f * M_PI));

    Color c = effective_color(0);
    Color dimmed = Color(
        (uint8_t)(c.r * breath),
        (uint8_t)(c.g * breath),
        (uint8_t)(c.b * breath));

    for (int i = 0; i < num_leds; i++) {
      buffer[i] = dimmed;
    }
  }
};

/**
 * SPIN – Dot or arc that rotates continuously around the strip.
 *
 * params[0] = spread (LEDs on each side, like dot)
 * params[1] = speed (RPM, revolutions per minute; default 10)
 * params[2] = direction (0 = CW, 1 = CCW)
 *
 * colors[0] = spin color
 *
 * Ref: Spec – "dot/arc rotante, velocità, direzione (CW/CCW)"
 */
struct SpinPrimitive : Primitive {
  SpinPrimitive() {
    type = PRIM_SPIN;
    params[0] = 0.0f;   // spread
    params[1] = 10.0f;  // 10 RPM
    params[2] = 0.0f;   // CW
  }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float spread = params[0];
    float rpm = (params[1] > 0.0f) ? params[1] : 10.0f;
    bool ccw = (params[2] != 0.0f);

    // Calculate current angle from time
    // angle = (time_in_minutes * rpm) * 360° mod 360°
    float angle = fmodf((float)now_ms / 60000.0f * rpm * 360.0f, 360.0f);
    if (ccw) angle = 360.0f - angle;

    int center = angle_to_led(angle, num_leds);
    int s = (int)spread;
    Color c = effective_color(0);

    for (int d = -s; d <= s; d++) {
      int idx = (center + d + num_leds) % num_leds;
      buffer[idx] = c;
    }
  }
};

/**
 * RAINBOW – Rotating rainbow across the entire strip.
 *
 * params[0] = speed (RPM, default 5)
 *
 * Intensity applies as global brightness.
 * Threshold modifier is ignored (rainbow has its own colors).
 *
 * Ref: Spec – "arcobaleno rotante su tutta la strip"
 */
struct RainbowPrimitive : Primitive {
  RainbowPrimitive() {
    type = PRIM_RAINBOW;
    params[0] = 5.0f;
  }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float rpm = (params[0] > 0.0f) ? params[0] : 5.0f;
    // Offset in hue-space that shifts over time
    float offset = fmodf((float)now_ms / 60000.0f * rpm * 360.0f, 360.0f);

    for (int i = 0; i < num_leds; i++) {
      // Hue: distribute 360° across strip, add time offset
      float hue = fmodf((float)i * 360.0f / (float)num_leds + offset, 360.0f);
      Color c = hsv_to_rgb(hue, 1.0f, (float)intensity / 255.0f);
      buffer[i] = c;
    }
  }

 private:
  /**
   * HSV to RGB conversion.
   * @param h Hue 0–360
   * @param s Saturation 0–1
   * @param v Value 0–1
   * Ref: Standard HSV→RGB algorithm
   */
  static Color hsv_to_rgb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r, g, b;

    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    return Color(
        (uint8_t)((r + m) * 255),
        (uint8_t)((g + m) * 255),
        (uint8_t)((b + m) * 255));
  }
};

/**
 * STROBE – Flashing on/off at a configurable frequency.
 *
 * params[0] = frequency in Hz (flashes per second, default 2)
 * params[1] = duty cycle 0.0–1.0 (fraction of period that is ON, default 0.5)
 *
 * colors[0] = strobe color
 *
 * Ref: Spec – "lampeggio on/off, colore, frequenza"
 */
struct StrobePrimitive : Primitive {
  StrobePrimitive() {
    type = PRIM_STROBE;
    params[0] = 2.0f;  // 2 Hz
    params[1] = 0.5f;  // 50% duty
  }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float freq = (params[0] > 0.0f) ? params[0] : 2.0f;
    float duty = params[1];
    if (duty <= 0.0f || duty > 1.0f) duty = 0.5f;

    float period_ms = 1000.0f / freq;
    float phase = fmodf((float)now_ms, period_ms) / period_ms;

    // ON if within duty cycle portion
    if (phase < duty) {
      Color c = effective_color(0);
      for (int i = 0; i < num_leds; i++) {
        buffer[i] = c;
      }
    }
    // OFF phase: leave buffer untouched (black from clear, or lower layer)
  }
};

/**
 * SPARKLE – Random LEDs flicker on and off.
 *
 * params[0] = density (fraction of LEDs lit at any time, 0.0–1.0, default 0.1)
 * params[1] = speed (changes per second, default 10)
 *
 * colors[0] = sparkle color
 *
 * Uses a simple pseudo-random approach seeded by time to avoid
 * storing per-LED state. Each LED's on/off is determined by a hash
 * of its index and a time-based seed.
 *
 * Ref: Spec – "LED casuali che si accendono/spengono"
 */
struct SparklePrimitive : Primitive {
  SparklePrimitive() {
    type = PRIM_SPARKLE;
    params[0] = 0.1f;   // 10% density
    params[1] = 10.0f;  // 10 changes/sec
  }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float density = params[0];
    if (density <= 0.0f) return;
    if (density > 1.0f) density = 1.0f;

    float speed = (params[1] > 0.0f) ? params[1] : 10.0f;
    // Time-based seed: changes every (1000/speed) ms
    uint32_t seed = (uint32_t)(now_ms * speed / 1000.0f);

    Color c = effective_color(0);
    // Threshold for density: map 0–1 to 0–65535
    uint16_t thresh = (uint16_t)(density * 65535.0f);

    for (int i = 0; i < num_leds; i++) {
      // Simple hash: xorshift-like mixing of index + seed
      uint32_t h = (uint32_t)i * 2654435761u ^ seed * 2246822519u;
      h ^= h >> 16;
      h *= 0x45d9f3b;
      h ^= h >> 16;

      if ((h & 0xFFFF) < thresh) {
        buffer[i] = c;
      }
    }
  }
};

/**
 * COMET – A dot moving around the strip with a fading tail behind it.
 *
 * params[0] = speed (RPM, default 10)
 * params[1] = tail length (in LEDs, default 20)
 * params[2] = direction (0 = CW, 1 = CCW)
 *
 * colors[0] = comet head color (tail fades toward black)
 *
 * Ref: Spec – "dot con coda sfumata, velocità, lunghezza coda, direzione"
 */
struct CometPrimitive : Primitive {
  CometPrimitive() {
    type = PRIM_COMET;
    params[0] = 10.0f;  // 10 RPM
    params[1] = 20.0f;  // 20 LED tail
    params[2] = 0.0f;   // CW
  }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    float rpm = (params[0] > 0.0f) ? params[0] : 10.0f;
    int tail_len = (int)params[1];
    if (tail_len < 1) tail_len = 1;
    bool ccw = (params[2] != 0.0f);

    // Head position from time
    float angle = fmodf((float)now_ms / 60000.0f * rpm * 360.0f, 360.0f);
    if (ccw) angle = 360.0f - angle;
    int head = angle_to_led(angle, num_leds);

    Color c = effective_color(0);

    // Head LED at full brightness
    buffer[head] = c;

    // Tail: LEDs behind the head, fading linearly to black
    for (int t = 1; t <= tail_len; t++) {
      // Behind = opposite direction of travel
      int idx;
      if (ccw) {
        idx = (head + t) % num_leds;            // tail trails CW if head moves CCW
      } else {
        idx = (head - t + num_leds) % num_leds;  // tail trails CCW if head moves CW
      }

      float fade = 1.0f - (float)t / (float)(tail_len + 1);
      buffer[idx] = Color(
          (uint8_t)(c.r * fade),
          (uint8_t)(c.g * fade),
          (uint8_t)(c.b * fade));
    }
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Concrete Primitives – F3: threshold as standalone primitive
// ═══════════════════════════════════════════════════════════════════════════

/**
 * THRESHOLD (standalone) – Fills the strip with a color determined by
 * the bound HA entity value relative to two thresholds.
 *
 * Can visualize as solid, trail, or dot depending on params[0]:
 *   params[0] = visualization mode (0 = solid, 1 = trail, 2 = dot)
 *   params[1] = spread (for dot mode)
 *
 * ThresholdConfig colors are used directly:
 *   threshold.colors[0] = below threshold1
 *   threshold.colors[1] = between threshold1 and threshold2
 *   threshold.colors[2] = above threshold2
 *
 * Ref: Spec – "primitiva threshold separata, cambia colore per soglie"
 */
struct ThresholdPrimitive : Primitive {
  ThresholdPrimitive() {
    type = PRIM_THRESHOLD;
    // Force threshold enabled by default for this primitive
    threshold.enabled = true;
  }

  void render(Color *buffer, int num_leds, uint32_t now_ms) override {
    if (!ha_bound) return;

    // Determine color from threshold zones
    Color c;
    if (ha_value < threshold.threshold1)
      c = threshold.colors[0];
    else if (ha_value < threshold.threshold2)
      c = threshold.colors[1];
    else
      c = threshold.colors[2];

    // Apply intensity
    c = Color(
        (uint8_t)((uint16_t)c.r * intensity / 255),
        (uint8_t)((uint16_t)c.g * intensity / 255),
        (uint8_t)((uint16_t)c.b * intensity / 255));

    int mode = (int)params[0];

    switch (mode) {
      case 1: {
        // Trail mode: fill 0° to mapped angle with threshold color
        float end_angle = mapped_angle();
        int led_end = angle_to_led(end_angle, num_leds);
        for (int i = 0; i <= led_end && i < num_leds; i++) {
          buffer[i] = c;
        }
        break;
      }
      case 2: {
        // Dot mode: single dot at mapped angle with threshold color
        float angle = mapped_angle();
        int center = angle_to_led(angle, num_leds);
        int spread = (int)params[1];
        for (int d = -spread; d <= spread; d++) {
          int idx = (center + d + num_leds) % num_leds;
          buffer[idx] = c;
        }
        break;
      }
      default: {
        // Solid mode: entire strip with threshold color
        for (int i = 0; i < num_leds; i++) {
          buffer[i] = c;
        }
        break;
      }
    }
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// Factory function – creates a Primitive subclass by type enum
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Allocate a new Primitive on the heap by type.
 * Returns nullptr for unknown/PRIM_NONE types.
 * Caller owns the pointer.
 */
inline Primitive *create_primitive(PrimitiveType type) {
  switch (type) {
    case PRIM_DOT:       return new DotPrimitive();
    case PRIM_ARC:       return new ArcPrimitive();
    case PRIM_TRAIL:     return new TrailPrimitive();
    case PRIM_SOLID:     return new SolidPrimitive();
    case PRIM_GRADIENT:  return new GradientPrimitive();
    case PRIM_SEGMENT:   return new SegmentPrimitive();
    case PRIM_PULSE:     return new PulsePrimitive();
    case PRIM_SPIN:      return new SpinPrimitive();
    case PRIM_RAINBOW:   return new RainbowPrimitive();
    case PRIM_STROBE:    return new StrobePrimitive();
    case PRIM_SPARKLE:   return new SparklePrimitive();
    case PRIM_COMET:     return new CometPrimitive();
    case PRIM_THRESHOLD: return new ThresholdPrimitive();
    default:             return nullptr;
  }
}

}  // namespace the_circle
}  // namespace esphome
