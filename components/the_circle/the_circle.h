/**
 * @file the_circle.h
 * @brief TheCircleComponent – main ESPHome component for The Circle.
 *
 * Responsibilities:
 *   - Holds profiles and manages active profile selection
 *   - Owns AddressableLight references for 3 strips
 *   - Runs the render loop (composites layers → strips)
 *   - Registers HA services for profile/layer configuration
 *   - Subscribes to HA entity states for dynamic binding
 *
 * Ref: ESPHome Component lifecycle
 *   https://developers.esphome.io/architecture/overview/
 * Ref: CustomAPIDevice for HA interaction
 *   https://esphome.io/components/api/ (homeassistant_states, custom_services)
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/core/color.h"
#include "esphome/core/preferences.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/core/string_ref.h"

#include "primitive.h"
#include "profile.h"
#include "renderer.h"
#include "storage.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace esphome {
namespace the_circle {

// ── Max LED count across all strips (for work buffer allocation) ─────────
static const int MAX_LEDS = 300;

/**
 * TheCircleComponent
 * Inherits Component (lifecycle) + CustomAPIDevice (HA services/subscriptions).
 *
 * YAML config wires in the AddressableLight pointers and profile/layer counts
 * via the Python codegen in __init__.py.
 */
class TheCircleComponent : public Component, public api::CustomAPIDevice {
 public:
  // ── Setup methods called from codegen ──────────────────────────────────

  /**
   * Set a strip reference. Called from Python to_code().
   * @param index  0=Inner Aura, 1=Outer Aura, 2=Inner Glow
   * @param light  Pointer to the LightState (we extract AddressableLight from it)
   */
  void set_strip(int index, light::LightState *light) {
    if (index >= 0 && index < MAX_STRIPS) {
      this->light_states_[index] = light;
    }
  }

  void set_num_profiles(int n) { this->num_profiles_ = n; }
  void set_layers_per_strip(int n) { this->layers_per_strip_ = n; }

  // ── Component lifecycle ────────────────────────────────────────────────

  /**
   * Component priority: run after lights are set up.
   * Ref: ESPHome component priorities – AFTER_WIFI = 250
   */
  float get_setup_priority() const override {
    return setup_priority::AFTER_CONNECTION;
  }

  /**
   * setup() – called once after all components are constructed.
   * Registers HA services and initializes profiles.
   */
  void setup() override {
    ESP_LOGI("the_circle", "Setting up The Circle component");

    // Resolve AddressableLight pointers from LightState
    for (int i = 0; i < MAX_STRIPS; i++) {
      if (this->light_states_[i] != nullptr) {
        // Get the output, which should be an AddressableLight
        auto *output = this->light_states_[i]->get_output();
        this->lights_[i] =
            static_cast<light::AddressableLight *>(output);
        if (this->lights_[i]) {
          this->num_leds_[i] = this->lights_[i]->size();
          ESP_LOGI("the_circle", "Strip %d: %d LEDs", i, this->num_leds_[i]);
        }
      }
    }

    // Initialize default profile names
    for (int p = 0; p < this->num_profiles_; p++) {
      char name[PROFILE_NAME_LEN];
      snprintf(name, PROFILE_NAME_LEN, "Profile %d", p + 1);
      this->profiles_[p].set_name(name);
    }

    // ── Register HA services ─────────────────────────────────────────────
    // Ref: CustomAPIDevice::register_service()
    // Requires api: { custom_services: true } in YAML

    // Service: the_circle.set_profile(profile_index)
    register_service(
        &TheCircleComponent::on_set_profile,
        "set_profile",
        {"profile_index"});

    // Service: the_circle.configure_layer(
    //   profile, strip, layer, type,
    //   color_r, color_g, color_b,
    //   param0..param3,
    //   entity_id, value_min, value_max,
    //   intensity, spread)
    register_service(
        &TheCircleComponent::on_configure_layer,
        "configure_layer",
        {"profile", "strip", "layer", "type",
         "color_r", "color_g", "color_b",
         "param0", "param1", "param2", "param3",
         "entity_id",
         "value_min", "value_max",
         "intensity"});

    // Service: the_circle.set_layer_color(profile, strip, layer, color_idx, r, g, b)
    register_service(
        &TheCircleComponent::on_set_layer_color,
        "set_layer_color",
        {"profile", "strip", "layer", "color_index", "r", "g", "b"});

    // Service: the_circle.set_threshold(profile, strip, layer,
    //   enabled, threshold1, threshold2, r0,g0,b0, r1,g1,b1, r2,g2,b2)
    register_service(
        &TheCircleComponent::on_set_threshold,
        "set_threshold",
        {"profile", "strip", "layer",
         "enabled", "threshold1", "threshold2",
         "r0", "g0", "b0", "r1", "g1", "b1", "r2", "g2", "b2"});

    // Service: the_circle.clear_layer(profile, strip, layer)
    register_service(
        &TheCircleComponent::on_clear_layer,
        "clear_layer",
        {"profile", "strip", "layer"});

    // Service: the_circle.bind_entity(profile, strip, layer, entity_id, value_min, value_max)
    register_service(
        &TheCircleComponent::on_bind_entity,
        "bind_entity",
        {"profile", "strip", "layer", "entity_id", "value_min", "value_max"});

    // Service: the_circle.rename_profile(profile, name)
    register_service(
        &TheCircleComponent::on_rename_profile,
        "rename_profile",
        {"profile", "name"});

    // Service: the_circle.save_profiles() – persist all profiles to flash
    register_service(
        &TheCircleComponent::on_save_profiles,
        "save_profiles",
        {});

    // ── Load saved profiles from flash ────────────────────────────────────
    // Ref: F5 – profiles survive reboot via NVS preferences
    int restored_profile = 0;
    if (load_from_flash(this->profiles_, this->num_profiles_, restored_profile)) {
      this->current_profile_ = restored_profile;
      ESP_LOGI("the_circle", "Restored active profile: %d", restored_profile);

      // Re-subscribe to HA entities for all loaded bindings
      resubscribe_all_entities_();
    }

    ESP_LOGI("the_circle", "Setup complete, %d profiles, %d layers/strip",
             this->num_profiles_, this->layers_per_strip_);
  }

  /**
   * loop() – called every ~16ms.
   * Renders active profile onto all strips.
   */
  void loop() override {
    uint32_t now = millis();

    // Throttle rendering to ~60fps (16ms)
    if (now - this->last_render_ms_ < 16) return;
    this->last_render_ms_ = now;

    Profile &active = this->profiles_[this->current_profile_];

    for (int s = 0; s < MAX_STRIPS; s++) {
      if (this->lights_[s] == nullptr) continue;

      // Render layers into work buffer, then copy to strip
      render_strip(active, s, this->lights_[s], now,
                   this->work_buffer_, this->num_leds_[s]);

      // Schedule a write to the physical strip
      this->lights_[s]->schedule_show();
    }
  }

 private:
  // ── Strip references ───────────────────────────────────────────────────
  light::LightState *light_states_[MAX_STRIPS]{nullptr, nullptr, nullptr};
  light::AddressableLight *lights_[MAX_STRIPS]{nullptr, nullptr, nullptr};
  int num_leds_[MAX_STRIPS]{0, 0, 0};

  // ── Profiles ───────────────────────────────────────────────────────────
  Profile profiles_[MAX_PROFILES_DEFAULT];
  int current_profile_{0};
  int num_profiles_{MAX_PROFILES_DEFAULT};
  int layers_per_strip_{MAX_LAYERS_DEFAULT};

  // ── Edit cursor (F6) ──────────────────────────────────────────────────
  // Determines which layer the UI entities are reading/writing.
  int edit_profile_{0};
  int edit_strip_{0};
  int edit_layer_{0};
  int edit_color_index_{0};  // which of the 4 color slots is being edited

 public:
  // ── F6: Edit cursor accessors ─────────────────────────────────────────
  int get_edit_profile() const { return edit_profile_; }
  int get_edit_strip() const { return edit_strip_; }
  int get_edit_layer() const { return edit_layer_; }
  int get_edit_color_index() const { return edit_color_index_; }
  int get_current_profile() const { return current_profile_; }
  int get_num_profiles() const { return num_profiles_; }
  int get_layers_per_strip() const { return layers_per_strip_; }

  void set_edit_profile(int v) { edit_profile_ = v; }
  void set_edit_strip(int v) { edit_strip_ = v; }
  void set_edit_layer(int v) { edit_layer_ = v; }
  void set_edit_color_index(int v) { edit_color_index_ = v; }

  // ── F6: Profile name accessors ────────────────────────────────────────
  // Public because they are called by ProfileNameText in ui_entities.cpp.
  // Ref: F6 Layer Editor UI – profile name text entity.

  /**
   * Get the name of the profile at the edit cursor.
   */
  const char *get_edit_profile_name() {
    if (edit_profile_ < 0 || edit_profile_ >= num_profiles_) return "";
    return profiles_[edit_profile_].name;
  }

  /**
   * Set the name of the profile at the edit cursor.
   */
  void set_edit_profile_name(const std::string &name) {
    if (edit_profile_ < 0 || edit_profile_ >= num_profiles_) return;
    profiles_[edit_profile_].set_name(name.c_str());
    ESP_LOGI("the_circle", "Profile %d renamed to: %s", edit_profile_, name.c_str());
  }

  /**
   * Get the Primitive at the current edit cursor position.
   * Returns nullptr if no primitive is set at that layer.
   */
  Primitive *get_edit_primitive() {
    if (edit_profile_ < 0 || edit_profile_ >= num_profiles_) return nullptr;
    if (edit_strip_ < 0 || edit_strip_ >= MAX_STRIPS) return nullptr;
    if (edit_layer_ < 0 || edit_layer_ >= layers_per_strip_) return nullptr;
    auto &ly = profiles_[edit_profile_].layers[edit_strip_][edit_layer_];
    return ly.primitive;
  }

  /**
   * Set a new primitive at the current edit cursor position.
   * Handles binding cleanup/creation.
   */
  void set_edit_primitive(PrimitiveType type) {
    // Remove old bindings
    remove_bindings_for(edit_profile_, edit_strip_, edit_layer_);
    // Create new primitive
    Primitive *prim = create_primitive(type);
    profiles_[edit_profile_].set_layer(edit_strip_, edit_layer_, prim);
  }

  /**
   * Bind the current edit cursor's primitive to an HA entity.
   */
  void bind_edit_entity(const std::string &entity_id, float vmin, float vmax) {
    auto *prim = get_edit_primitive();
    if (!prim) return;
    remove_bindings_for(edit_profile_, edit_strip_, edit_layer_);
    prim->ha_entity_id = entity_id;
    prim->value_map_min = vmin;
    prim->value_map_max = vmax;
    prim->ha_bound = !entity_id.empty();
    if (prim->ha_bound) {
      subscribe_to_entity(edit_profile_, edit_strip_, edit_layer_, entity_id);
    }
  }

  // Expose profile array for save/load
  Profile *get_profiles() { return profiles_; }

  /**
   * Switch active profile (public, called by ActiveProfileSelect and service).
   */
  // NB #14470: i parametri int dei servizi registrati via register_service()
  // devono essere int32_t — ESPHome definisce get_execute_arg_value/
  // to_service_arg_type solo per <int32_t>, non <int>. Vedi README.
  void on_set_profile(int32_t profile_index) {
    if (profile_index < 0 || profile_index >= this->num_profiles_) {
      ESP_LOGW("the_circle", "Invalid profile index %d", (int) profile_index);
      return;
    }
    this->current_profile_ = profile_index;
    ESP_LOGI("the_circle", "Active profile: %d (%s)",
             (int) profile_index, this->profiles_[profile_index].name);
  }

 private:

  // ── Rendering ──────────────────────────────────────────────────────────
  Color work_buffer_[MAX_LEDS];
  uint32_t last_render_ms_{0};

  // ══════════════════════════════════════════════════════════════════════
  // HA Service handlers
  // ══════════════════════════════════════════════════════════════════════

  /**
   * Configure a layer with a primitive type and parameters.
   * Ref: service the_circle.configure_layer
   *
   * This is the main configuration entry point. It:
   *   1. Creates the appropriate Primitive subclass
   *   2. Sets its color, params, intensity
   *   3. Optionally binds to an HA entity
   */
  void on_configure_layer(int32_t profile, int32_t strip, int32_t layer, int32_t type,
                          int32_t color_r, int32_t color_g, int32_t color_b,
                          float param0, float param1,
                          float param2, float param3,
                          std::string entity_id,
                          float value_min, float value_max,
                          int32_t intensity) {
    if (!validate_indices(profile, strip, layer)) return;

    // Remove any existing bindings for this layer before reconfiguring
    remove_bindings_for(profile, strip, layer);

    // Create primitive by type
    auto prim_type = static_cast<PrimitiveType>(type);
    Primitive *prim = create_primitive(prim_type);
    if (!prim) {
      ESP_LOGW("the_circle", "Unknown primitive type %d", (int) type);
      return;
    }

    // Set primary color
    prim->colors[0] = Color(color_r, color_g, color_b);
    // Set params
    prim->params[0] = param0;
    prim->params[1] = param1;
    prim->params[2] = param2;
    prim->params[3] = param3;
    // Set intensity
    prim->intensity =
        (uint8_t) std::min<int32_t>(255, std::max<int32_t>(0, intensity));

    // Install into profile (transfers ownership, deletes old primitive)
    this->profiles_[profile].set_layer(strip, layer, prim);

    // HA entity binding (coordinate-based, after primitive is installed)
    if (!entity_id.empty()) {
      prim->ha_entity_id = entity_id;
      prim->value_map_min = value_min;
      prim->value_map_max = value_max;
      prim->ha_bound = true;
      subscribe_to_entity(profile, strip, layer, entity_id);
    }

    ESP_LOGI("the_circle", "Configured P%d S%d L%d: type=%d entity=%s",
             (int) profile, (int) strip, (int) layer, (int) type, entity_id.c_str());
  }

  /**
   * Set a specific color slot on a layer.
   * Ref: service the_circle.set_layer_color
   */
  void on_set_layer_color(int32_t profile, int32_t strip, int32_t layer,
                          int32_t color_index, int32_t r, int32_t g, int32_t b) {
    if (!validate_indices(profile, strip, layer)) return;
    if (color_index < 0 || color_index >= MAX_COLORS) return;

    auto &ly = this->profiles_[profile].layers[strip][layer];
    if (ly.primitive) {
      ly.primitive->colors[color_index] = Color(r, g, b);
    }
  }

  /**
   * Configure threshold modifier on a layer.
   * Ref: service the_circle.set_threshold
   */
  void on_set_threshold(int32_t profile, int32_t strip, int32_t layer,
                        int32_t enabled, float threshold1, float threshold2,
                        int32_t r0, int32_t g0, int32_t b0,
                        int32_t r1, int32_t g1, int32_t b1,
                        int32_t r2, int32_t g2, int32_t b2) {
    if (!validate_indices(profile, strip, layer)) return;

    auto &ly = this->profiles_[profile].layers[strip][layer];
    if (!ly.primitive) return;

    ly.primitive->threshold.enabled = (enabled != 0);
    ly.primitive->threshold.threshold1 = threshold1;
    ly.primitive->threshold.threshold2 = threshold2;
    ly.primitive->threshold.colors[0] = Color(r0, g0, b0);
    ly.primitive->threshold.colors[1] = Color(r1, g1, b1);
    ly.primitive->threshold.colors[2] = Color(r2, g2, b2);
  }

  /**
   * Clear (remove) a layer.
   * Ref: service the_circle.clear_layer
   */
  void on_clear_layer(int32_t profile, int32_t strip, int32_t layer) {
    if (!validate_indices(profile, strip, layer)) return;
    // Remove bindings before clearing the layer
    remove_bindings_for(profile, strip, layer);
    this->profiles_[profile].clear_layer(strip, layer);
    ESP_LOGI("the_circle", "Cleared P%d S%d L%d", (int) profile, (int) strip, (int) layer);
  }

  /**
   * Bind (or rebind) an HA entity to an existing layer.
   * Ref: service the_circle.bind_entity
   */
  void on_bind_entity(int32_t profile, int32_t strip, int32_t layer,
                      std::string entity_id,
                      float value_min, float value_max) {
    if (!validate_indices(profile, strip, layer)) return;

    auto &ly = this->profiles_[profile].layers[strip][layer];
    if (!ly.primitive) {
      ESP_LOGW("the_circle", "No primitive at P%d S%d L%d", (int) profile, (int) strip, (int) layer);
      return;
    }

    // Remove old bindings for this layer
    remove_bindings_for(profile, strip, layer);

    ly.primitive->ha_entity_id = entity_id;
    ly.primitive->value_map_min = value_min;
    ly.primitive->value_map_max = value_max;
    ly.primitive->ha_bound = !entity_id.empty();

    if (ly.primitive->ha_bound) {
      subscribe_to_entity(profile, strip, layer, entity_id);
    }

    ESP_LOGI("the_circle", "Bound P%d S%d L%d -> %s [%.1f, %.1f]",
             (int) profile, (int) strip, (int) layer, entity_id.c_str(), value_min, value_max);
  }

  /**
   * Save all profiles to flash.
   * Ref: service the_circle.save_profiles
   * Ref: F5 – NVS persistence via storage.h
   */
  void on_save_profiles() {
    save_to_flash(this->profiles_, this->num_profiles_, this->current_profile_);
  }

  /**
   * Rename a profile.
   * Ref: service the_circle.rename_profile
   */
  void on_rename_profile(int32_t profile, std::string name) {
    if (profile < 0 || profile >= this->num_profiles_) return;
    this->profiles_[profile].set_name(name.c_str());
    ESP_LOGI("the_circle", "Profile %d renamed to: %s", (int) profile, name.c_str());
  }

  // ══════════════════════════════════════════════════════════════════════
  // Helpers
  // ══════════════════════════════════════════════════════════════════════

  /**
   * Validate profile/strip/layer indices.
   */
  bool validate_indices(int profile, int strip, int layer) {
    if (profile < 0 || profile >= this->num_profiles_ ||
        strip < 0 || strip >= MAX_STRIPS ||
        layer < 0 || layer >= this->layers_per_strip_) {
      ESP_LOGW("the_circle", "Invalid indices: P%d S%d L%d", profile, strip, layer);
      return false;
    }
    return true;
  }

  /**
   * Re-subscribe to all HA entities found in loaded profiles.
   * Called after load_from_flash() to restore dynamic bindings.
   *
   * Scans all profiles/strips/layers for ha_bound primitives and
   * creates coordinate-based bindings + HA subscriptions.
   *
   * Ref: F4 binding system + F5 persistence
   */
  void resubscribe_all_entities_() {
    int count = 0;
    for (int p = 0; p < this->num_profiles_; p++) {
      for (int s = 0; s < MAX_STRIPS; s++) {
        for (int l = 0; l < MAX_LAYERS_DEFAULT; l++) {
          auto &ly = this->profiles_[p].layers[s][l];
          if (!ly.enabled || !ly.primitive) continue;
          if (!ly.primitive->ha_bound) continue;
          if (ly.primitive->ha_entity_id.empty()) continue;

          subscribe_to_entity(p, s, l, ly.primitive->ha_entity_id);
          count++;
        }
      }
    }
    ESP_LOGI("the_circle", "Re-subscribed %d HA entity bindings", count);
  }

  // ══════════════════════════════════════════════════════════════════════
  // F4: HA Entity Binding System
  // ══════════════════════════════════════════════════════════════════════
  //
  // Design:
  //   Bindings are stored as coordinate-based references (profile/strip/layer),
  //   NOT raw Primitive pointers. This solves lifetime issues when layers are
  //   reconfigured (new Primitive replaces old one).
  //
  //   Subscriptions to HA are deduplicated: we only call
  //   subscribe_homeassistant_state() once per unique entity_id. The callback
  //   fans out to all bindings for that entity.
  //
  //   String states (e.g. "home"/"not_home") are mapped to float via a
  //   configurable string→value map, or via a built-in default map.
  //
  // Ref: CustomAPIDevice::subscribe_homeassistant_state()
  //   https://esphome.io/components/api/ (homeassistant_states)

  /**
   * Coordinate-based binding: which profile/strip/layer listens to which entity.
   */
  struct EntityBinding {
    std::string entity_id;
    int profile;
    int strip;
    int layer;
  };

  /**
   * Track which entity_ids we have already subscribed to,
   * to avoid duplicate subscribe_homeassistant_state() calls.
   * (ESPHome does not deduplicate internally.)
   */
  std::vector<std::string> subscribed_entities_;

  /**
   * All active bindings (coordinate-based).
   */
  std::vector<EntityBinding> bindings_;

  /**
   * Built-in string→float mappings for common non-numeric HA states.
   * Used when the entity state cannot be parsed as a float.
   *
   * Ref: Spec – entities like person.* return "home"/"not_home"
   */
  static float map_string_state(const std::string &state) {
    // ── Presence / person states ──
    if (state == "home")      return 1.0f;
    if (state == "not_home")  return 0.0f;
    // ── Binary on/off ──
    if (state == "on")        return 1.0f;
    if (state == "off")       return 0.0f;
    // ── Boolean ──
    if (state == "true")      return 1.0f;
    if (state == "false")     return 0.0f;
    // ── Availability ──
    if (state == "unavailable" || state == "unknown") return -1.0f;
    // ── Lock states ──
    if (state == "locked")    return 1.0f;
    if (state == "unlocked")  return 0.0f;
    // ── Cover states ──
    if (state == "open")      return 1.0f;
    if (state == "closed")    return 0.0f;
    if (state == "opening")   return 0.5f;
    if (state == "closing")   return 0.5f;
    // ── Alarm states ──
    if (state == "disarmed")  return 0.0f;
    if (state == "armed_home") return 0.5f;
    if (state == "armed_away") return 1.0f;
    if (state == "triggered") return 2.0f;
    // ── HVAC modes ──
    if (state == "heat")      return 1.0f;
    if (state == "cool")      return 2.0f;
    if (state == "auto")      return 3.0f;
    if (state == "idle")      return 0.0f;
    // ── Fallback: unparseable string ──
    return 0.0f;
  }

  /**
   * Subscribe to an HA entity state for a specific layer.
   * Coordinate-based: bindings reference (profile, strip, layer),
   * so they remain valid even when the Primitive is replaced.
   *
   * Deduplicates: only subscribes once per unique entity_id.
   */
  void subscribe_to_entity(int profile, int strip, int layer,
                           const std::string &entity_id) {
    // Add binding (coordinate-based)
    EntityBinding binding;
    binding.entity_id = entity_id;
    binding.profile = profile;
    binding.strip = strip;
    binding.layer = layer;
    this->bindings_.push_back(binding);

    // Subscribe only if we haven't already for this entity_id
    bool already_subscribed = false;
    for (const auto &eid : this->subscribed_entities_) {
      if (eid == entity_id) {
        already_subscribed = true;
        break;
      }
    }

    if (!already_subscribed) {
      subscribe_homeassistant_state(
          &TheCircleComponent::on_ha_state_changed_,
          entity_id);
      this->subscribed_entities_.push_back(entity_id);
      ESP_LOGD("the_circle", "Subscribed to HA entity: %s", entity_id.c_str());
    }
  }

  /**
   * Remove all bindings for a specific layer.
   * Called when a layer is cleared or reconfigured.
   */
  void remove_bindings_for(int profile, int strip, int layer) {
    bindings_.erase(
        std::remove_if(bindings_.begin(), bindings_.end(),
                       [profile, strip, layer](const EntityBinding &b) {
                         return b.profile == profile &&
                                b.strip == strip &&
                                b.layer == layer;
                       }),
        bindings_.end());
    // Note: we do NOT unsubscribe from the HA entity because
    // other layers may still use it, and ESPHome doesn't support
    // unsubscribe. Orphaned subscriptions are harmless (callback
    // finds no matching binding and does nothing).
  }

  /**
   * HA state change callback.
   * Dispatches to all primitives bound to this entity via coordinates.
   *
   * Handles both numeric and string states:
   *   - Tries std::stof first (covers sensors, numbers, percentages)
   *   - Falls back to map_string_state() for non-numeric (person, binary_sensor, etc.)
   *
   * Ref: subscribe_homeassistant_state callback: (std::string entity_id, std::string state)
   */
  // Callback stato HA – firma moderna (const std::string &, StringRef).
  // Migrata dalla vecchia (std::string, std::string) deprecata (rimozione
  // 2027.1.0). StringRef è null-terminated: .c_str() è sicuro; supporta ==
  // e .empty(). Ref: custom_api_device.h (overload StringRef).
  void on_ha_state_changed_(const std::string &entity_id, StringRef state) {
    // Parse value: numeric first, then string mapping
    float val = 0.0f;
    bool parsed = false;

    // Skip unavailable/unknown early (StringRef supporta ==)
    if (state == "unavailable" || state == "unknown") {
      val = -1.0f;
      parsed = true;
    }

    if (!parsed) {
      // Try numeric parse (StringRef è null-terminated → c_str() sicuro)
      const char *cstr = state.c_str();
      char *end = nullptr;
      float numeric = strtof(cstr, &end);
      if (end != cstr && (*end == '\0' || *end == ' ')) {
        val = numeric;
        parsed = true;
      }
    }

    // Costruzione della std::string solo se serve (mapping/salvataggio):
    // per i rami numerici sopra evitiamo l'allocazione.
    std::string state_str;
    if (!parsed) {
      // String state → float mapping (map_string_state richiede std::string)
      state_str = std::string(state.c_str());
      val = map_string_state(state_str);
    }

    ESP_LOGD("the_circle", "HA state: %s = %s → %.2f",
             entity_id.c_str(), state.c_str(), val);

    // Fan out to all bound primitives via coordinates
    for (const auto &b : this->bindings_) {
      if (b.entity_id != entity_id) continue;

      // Resolve primitive from coordinates
      if (b.profile < 0 || b.profile >= this->num_profiles_) continue;
      auto &ly = this->profiles_[b.profile].layers[b.strip][b.layer];
      if (!ly.enabled || !ly.primitive) continue;

      // Update the primitive's ha_value
      ly.primitive->ha_value = val;
      // Also store raw string for potential future use.
      // Riusa state_str se già costruita, altrimenti costruiscila una volta.
      if (state_str.empty()) state_str = std::string(state.c_str());
      ly.primitive->ha_state_str = state_str;
    }
  }
};

}  // namespace the_circle
}  // namespace esphome
