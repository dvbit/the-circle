/**
 * @file profile.h
 * @brief Profile – a named configuration of primitives across strips.
 *
 * A profile defines what each strip displays. Each strip can have up to
 * MAX_LAYERS primitives stacked with painter's algorithm (last layer wins).
 *
 * Profiles are stored in flash for persistence across reboots.
 *
 * Ref: Requirement spec – "un profilo = fino a 6 primitive (layer) per strip"
 */

#pragma once

#include "primitive.h"
#include <cstring>

namespace esphome {
namespace the_circle {

static const int MAX_STRIPS = 3;
static const int MAX_LAYERS_DEFAULT = 6;
static const int MAX_CONTROL_LAYERS = 4;  // buzzer, lux_gate, mmwave_gate, ha_presence_gate
static const int MAX_PROFILES_DEFAULT = 10;
static const int PROFILE_NAME_LEN = 16;

/**
 * Layer – wraps a Primitive pointer with an enabled flag.
 * The Profile owns the Primitive memory.
 */
struct Layer {
  Primitive *primitive{nullptr};
  bool enabled{false};

  ~Layer() {
    if (primitive) {
      delete primitive;
      primitive = nullptr;
    }
  }

  // No copy – move only (owns heap Primitive)
  Layer() = default;
  Layer(const Layer &) = delete;
  Layer &operator=(const Layer &) = delete;
};

/**
 * Profile – named set of layers across all strips + control layers.
 *
 * Visual layers: layers[3 strips][6 layers] — painter's algorithm per strip.
 * Control layers: control_layers[4] — profile-level gates and triggers.
 *   Slot 0: buzzer
 *   Slot 1: lux_gate
 *   Slot 2: mmwave_gate
 *   Slot 3: ha_presence_gate
 *
 * Ref: Spec – "4 layer aggiuntivi oltre i sei esistenti"
 */
struct Profile {
  char name[PROFILE_NAME_LEN]{"Empty"};
  Layer layers[MAX_STRIPS][MAX_LAYERS_DEFAULT];
  int layer_count[MAX_STRIPS]{0, 0, 0};  // active visual layers per strip

  // ── Control layers (profile-level, not per-strip) ──────────────────────
  Layer control_layers[MAX_CONTROL_LAYERS];

  /**
   * Set a visual layer on a specific strip.
   * @param strip_idx  0–2 (Inner Aura, Outer Aura, Inner Glow)
   * @param layer_idx  0–5
   * @param prim       Primitive pointer (ownership transferred to Profile)
   */
  void set_layer(int strip_idx, int layer_idx, Primitive *prim) {
    if (strip_idx < 0 || strip_idx >= MAX_STRIPS) return;
    if (layer_idx < 0 || layer_idx >= MAX_LAYERS_DEFAULT) return;

    // Delete existing primitive if any
    if (layers[strip_idx][layer_idx].primitive) {
      delete layers[strip_idx][layer_idx].primitive;
    }
    layers[strip_idx][layer_idx].primitive = prim;
    layers[strip_idx][layer_idx].enabled = (prim != nullptr);

    // Update active layer count
    int count = 0;
    for (int i = 0; i < MAX_LAYERS_DEFAULT; i++) {
      if (layers[strip_idx][i].enabled) count = i + 1;
    }
    layer_count[strip_idx] = count;
  }

  /**
   * Set a control layer.
   * @param ctrl_idx  0–3 (buzzer, lux_gate, mmwave_gate, ha_presence_gate)
   * @param prim      Primitive pointer (ownership transferred)
   */
  void set_control_layer(int ctrl_idx, Primitive *prim) {
    if (ctrl_idx < 0 || ctrl_idx >= MAX_CONTROL_LAYERS) return;
    if (control_layers[ctrl_idx].primitive) {
      delete control_layers[ctrl_idx].primitive;
    }
    control_layers[ctrl_idx].primitive = prim;
    control_layers[ctrl_idx].enabled = (prim != nullptr);
  }

  /**
   * Remove a visual layer, freeing the primitive.
   */
  void clear_layer(int strip_idx, int layer_idx) {
    set_layer(strip_idx, layer_idx, nullptr);
  }

  void clear_control_layer(int ctrl_idx) {
    set_control_layer(ctrl_idx, nullptr);
  }

  /**
   * Clear all layers (visual + control).
   */
  void clear_all() {
    for (int s = 0; s < MAX_STRIPS; s++) {
      for (int l = 0; l < MAX_LAYERS_DEFAULT; l++) {
        clear_layer(s, l);
      }
    }
    for (int c = 0; c < MAX_CONTROL_LAYERS; c++) {
      clear_control_layer(c);
    }
  }

  /**
   * Evaluate all gate control layers.
   * Returns false if ANY gate blocks the profile.
   */
  bool evaluate_gates() const {
    for (int c = 0; c < MAX_CONTROL_LAYERS; c++) {
      if (!control_layers[c].enabled || !control_layers[c].primitive) continue;
      if (!control_layers[c].primitive->is_control()) continue;
      if (!control_layers[c].primitive->evaluate_gate()) return false;
    }
    return true;
  }

  /**
   * Set profile name (truncated to PROFILE_NAME_LEN-1).
   */
  void set_name(const char *n) {
    strncpy(name, n, PROFILE_NAME_LEN - 1);
    name[PROFILE_NAME_LEN - 1] = '\0';
  }
};

}  // namespace the_circle
}  // namespace esphome
