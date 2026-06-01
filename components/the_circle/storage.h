/**
 * @file storage.h
 * @brief Flash persistence for The Circle profiles.
 *
 * ESPHome's preference system (NVS on ESP32) stores fixed-size POD structs.
 * Strings (entity_id) are stored as fixed-length char arrays.
 *
 * Storage layout:
 *   - 1 preference for global state (current_profile, version)
 *   - 1 preference per profile/strip/layer = up to 6×3×6 = 108 slots
 *
 * Each layer is serialized into a LayerData POD struct (~120 bytes).
 * Total flash usage: ~13 KB for 6 profiles × 3 strips × 6 layers.
 *
 * Ref: ESPHome preferences API
 *   https://esphome.io/api/core_2preferences_8h_source.html
 *   global_preferences->make_preference<T>(hash)
 */

#pragma once

#include "esphome/core/preferences.h"
#include "primitive.h"
#include "profile.h"

#include <cstring>

namespace esphome {
namespace the_circle {

// ── Storage version for migration support ────────────────────────────────
static const uint8_t STORAGE_VERSION = 1;

// ── Max entity_id length stored in flash ─────────────────────────────────
// Most HA entity_ids are under 64 chars (e.g. "sensor.forno_program_progress")
static const int ENTITY_ID_MAX_LEN = 64;

// ── Unique hash base for NVS key generation ──────────────────────────────
// Must be unique across all ESPHome components on this device.
// Using FNV-1a of "the_circle" = 0x7C9E3A1D
static const uint32_t STORAGE_HASH_BASE = 0x7C9E3A1D;

/**
 * Global state stored in flash.
 * Single preference slot.
 */
struct GlobalData {
  uint8_t version{STORAGE_VERSION};
  uint8_t current_profile{0};
  uint8_t num_profiles{6};
  uint8_t reserved[5]{0};  // padding for future use
} __attribute__((packed));

/**
 * Per-layer serializable data.
 * Fixed-size POD struct suitable for ESPHome preferences.
 *
 * Contains all primitive configuration needed to reconstruct
 * the layer after reboot.
 */
struct LayerData {
  // ── Primitive type ─────────────────────────────────────────────────
  uint8_t type{0};          // PrimitiveType enum value
  uint8_t enabled{0};       // 0 = disabled, 1 = enabled
  uint8_t intensity{255};

  // ── Colors (4 slots × RGBW) ────────────────────────────────────────
  uint8_t colors[MAX_COLORS][4]{
      {255, 255, 255, 0},
      {0, 0, 0, 0},
      {0, 0, 0, 0},
      {0, 0, 0, 0}
  };

  // ── Params (8 floats) ──────────────────────────────────────────────
  float params[8]{0};

  // ── HA binding ─────────────────────────────────────────────────────
  char entity_id[ENTITY_ID_MAX_LEN]{0};
  float value_map_min{0.0f};
  float value_map_max{100.0f};
  uint8_t ha_bound{0};

  // ── Threshold modifier ─────────────────────────────────────────────
  uint8_t threshold_enabled{0};
  float threshold1{33.0f};
  float threshold2{66.0f};
  uint8_t threshold_colors[3][3]{
      {0, 255, 0},     // green
      {255, 255, 0},   // yellow
      {255, 0, 0}      // red
  };

  // ── Padding to round size ──────────────────────────────────────────
  uint8_t reserved[3]{0};
} __attribute__((packed));

/**
 * Profile name stored in flash.
 */
struct ProfileNameData {
  char name[PROFILE_NAME_LEN]{0};
} __attribute__((packed));

// ═══════════════════════════════════════════════════════════════════════════
// Serialization helpers: Primitive ↔ LayerData
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Serialize a Primitive into a LayerData POD struct.
 * @param prim   Source primitive (may be nullptr → disabled layer)
 * @param data   Destination POD struct
 */
inline void primitive_to_layer_data(const Primitive *prim, LayerData &data) {
  memset(&data, 0, sizeof(LayerData));

  if (!prim) {
    data.type = PRIM_NONE;
    data.enabled = 0;
    return;
  }

  data.type = prim->type;
  data.enabled = 1;
  data.intensity = prim->intensity;

  // Colors
  for (int c = 0; c < MAX_COLORS; c++) {
    data.colors[c][0] = prim->colors[c].r;
    data.colors[c][1] = prim->colors[c].g;
    data.colors[c][2] = prim->colors[c].b;
    data.colors[c][3] = prim->colors[c].w;
  }

  // Params
  for (int p = 0; p < 8; p++) {
    data.params[p] = prim->params[p];
  }

  // HA binding
  if (prim->ha_bound && !prim->ha_entity_id.empty()) {
    strncpy(data.entity_id, prim->ha_entity_id.c_str(), ENTITY_ID_MAX_LEN - 1);
    data.entity_id[ENTITY_ID_MAX_LEN - 1] = '\0';
    data.value_map_min = prim->value_map_min;
    data.value_map_max = prim->value_map_max;
    data.ha_bound = 1;
  }

  // Threshold
  data.threshold_enabled = prim->threshold.enabled ? 1 : 0;
  data.threshold1 = prim->threshold.threshold1;
  data.threshold2 = prim->threshold.threshold2;
  for (int t = 0; t < 3; t++) {
    data.threshold_colors[t][0] = prim->threshold.colors[t].r;
    data.threshold_colors[t][1] = prim->threshold.colors[t].g;
    data.threshold_colors[t][2] = prim->threshold.colors[t].b;
  }
}

/**
 * Deserialize a LayerData POD struct into a new Primitive.
 * @param data   Source POD struct
 * @return       Heap-allocated Primitive, or nullptr if type is NONE/disabled
 *
 * Caller owns the returned pointer.
 */
inline Primitive *layer_data_to_primitive(const LayerData &data) {
  if (data.type == PRIM_NONE || !data.enabled) return nullptr;

  auto prim_type = static_cast<PrimitiveType>(data.type);
  Primitive *prim = create_primitive(prim_type);
  if (!prim) return nullptr;

  prim->intensity = data.intensity;

  // Colors
  for (int c = 0; c < MAX_COLORS; c++) {
    prim->colors[c] = Color(
        data.colors[c][0],
        data.colors[c][1],
        data.colors[c][2],
        data.colors[c][3]);
  }

  // Params
  for (int p = 0; p < 8; p++) {
    prim->params[p] = data.params[p];
  }

  // HA binding
  if (data.ha_bound && data.entity_id[0] != '\0') {
    prim->ha_entity_id = std::string(data.entity_id);
    prim->value_map_min = data.value_map_min;
    prim->value_map_max = data.value_map_max;
    prim->ha_bound = true;
  }

  // Threshold
  prim->threshold.enabled = (data.threshold_enabled != 0);
  prim->threshold.threshold1 = data.threshold1;
  prim->threshold.threshold2 = data.threshold2;
  for (int t = 0; t < 3; t++) {
    prim->threshold.colors[t] = Color(
        data.threshold_colors[t][0],
        data.threshold_colors[t][1],
        data.threshold_colors[t][2]);
  }

  return prim;
}

// ═══════════════════════════════════════════════════════════════════════════
// Storage manager
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Compute a unique NVS hash for a specific slot.
 * Each slot (global, profile name, layer) gets a unique hash derived
 * from STORAGE_HASH_BASE + an offset.
 *
 * Layout:
 *   hash+0             = GlobalData
 *   hash+1..6          = ProfileNameData[0..5]
 *   hash+100+p*18+s*6+l = LayerData[profile][strip][layer]
 *
 * This ensures no collisions with other ESPHome components.
 */
inline uint32_t storage_hash(int offset) {
  return STORAGE_HASH_BASE + (uint32_t)offset;
}

inline uint32_t layer_hash(int profile, int strip, int layer) {
  return storage_hash(100 + profile * 18 + strip * 6 + layer);
}

/**
 * Save all profiles to flash.
 *
 * @param profiles      Array of profiles
 * @param num_profiles  Number of profiles to save
 * @param current_profile  Currently active profile index
 */
inline void save_to_flash(Profile *profiles, int num_profiles, int current_profile) {
  ESP_LOGI("the_circle", "Saving profiles to flash...");

  // Save global state
  GlobalData gd;
  gd.version = STORAGE_VERSION;
  gd.current_profile = (uint8_t)current_profile;
  gd.num_profiles = (uint8_t)num_profiles;

  auto global_pref = global_preferences->make_preference<GlobalData>(storage_hash(0));
  global_pref.save(&gd);

  // Save profile names
  for (int p = 0; p < num_profiles; p++) {
    ProfileNameData pnd;
    strncpy(pnd.name, profiles[p].name, PROFILE_NAME_LEN - 1);
    pnd.name[PROFILE_NAME_LEN - 1] = '\0';

    auto name_pref = global_preferences->make_preference<ProfileNameData>(storage_hash(1 + p));
    name_pref.save(&pnd);
  }

  // Save each layer
  int saved = 0;
  for (int p = 0; p < num_profiles; p++) {
    for (int s = 0; s < MAX_STRIPS; s++) {
      for (int l = 0; l < MAX_LAYERS_DEFAULT; l++) {
        LayerData ld;
        auto &ly = profiles[p].layers[s][l];
        primitive_to_layer_data(ly.primitive, ld);

        auto layer_pref = global_preferences->make_preference<LayerData>(layer_hash(p, s, l));
        layer_pref.save(&ld);

        if (ld.enabled) saved++;
      }
    }
  }

  ESP_LOGI("the_circle", "Saved %d active layers to flash", saved);
}

/**
 * Load all profiles from flash.
 *
 * @param profiles       Array of profiles to populate
 * @param num_profiles   Number of profiles to load
 * @param current_profile  Output: restored active profile index
 * @return true if valid data was found, false if flash was empty/corrupt
 *
 * On failure, profiles are left in their default state.
 */
inline bool load_from_flash(Profile *profiles, int num_profiles, int &current_profile) {
  ESP_LOGI("the_circle", "Loading profiles from flash...");

  // Load global state
  GlobalData gd;
  auto global_pref = global_preferences->make_preference<GlobalData>(storage_hash(0));
  if (!global_pref.load(&gd)) {
    ESP_LOGW("the_circle", "No saved data found in flash");
    return false;
  }

  // Version check
  if (gd.version != STORAGE_VERSION) {
    ESP_LOGW("the_circle", "Flash data version mismatch: got %d, expected %d",
             gd.version, STORAGE_VERSION);
    return false;
  }

  current_profile = gd.current_profile;

  // Load profile names
  for (int p = 0; p < num_profiles; p++) {
    ProfileNameData pnd;
    auto name_pref = global_preferences->make_preference<ProfileNameData>(storage_hash(1 + p));
    if (name_pref.load(&pnd)) {
      profiles[p].set_name(pnd.name);
    }
  }

  // Load layers
  int loaded = 0;
  for (int p = 0; p < num_profiles; p++) {
    for (int s = 0; s < MAX_STRIPS; s++) {
      for (int l = 0; l < MAX_LAYERS_DEFAULT; l++) {
        LayerData ld;
        auto layer_pref = global_preferences->make_preference<LayerData>(layer_hash(p, s, l));
        if (layer_pref.load(&ld)) {
          Primitive *prim = layer_data_to_primitive(ld);
          if (prim) {
            profiles[p].set_layer(s, l, prim);
            loaded++;
          }
        }
      }
    }
  }

  ESP_LOGI("the_circle", "Loaded %d active layers from flash", loaded);
  return true;
}

}  // namespace the_circle
}  // namespace esphome
