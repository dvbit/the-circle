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
static const int MAX_PROFILES_DEFAULT = 20;
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
 * Profile – named set of layers across all strips.
 */
struct Profile {
  char name[PROFILE_NAME_LEN]{"Empty"};
  Layer layers[MAX_STRIPS][MAX_LAYERS_DEFAULT];
  int layer_count[MAX_STRIPS]{0, 0, 0};  // active layers per strip

  /**
   * Set a layer on a specific strip.
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
   * Remove a layer, freeing the primitive.
   */
  void clear_layer(int strip_idx, int layer_idx) {
    set_layer(strip_idx, layer_idx, nullptr);
  }

  /**
   * Clear all layers on all strips.
   */
  void clear_all() {
    for (int s = 0; s < MAX_STRIPS; s++) {
      for (int l = 0; l < MAX_LAYERS_DEFAULT; l++) {
        clear_layer(s, l);
      }
    }
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
