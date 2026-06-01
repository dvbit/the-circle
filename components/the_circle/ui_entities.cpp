/**
 * @file ui_entities.cpp
 * @brief Implementation of The Circle UI entities (select, number, text).
 *
 * Each entity reads/writes from the "edit cursor" on TheCircleComponent:
 *   edit_profile_ / edit_strip_ / edit_layer_ / edit_color_index_
 *
 * Ref: ESPHome 2025.11+ – select options stored in flash via codegen.
 *   C++ only calls publish_state() / publish_index(); no set_options().
 * Ref: ESPHome Select::control(), Number::control(), Text::control()
 */

#include "ui_entities.h"
#include "the_circle.h"

namespace esphome {
namespace the_circle {

// ═══════════════════════════════════════════════════════════════════════════
// Navigation selects
// Options are set by Python codegen (stored in flash).
// setup() publishes initial state; control() updates the edit cursor.
// ═══════════════════════════════════════════════════════════════════════════

// ── EditProfileSelect ────────────────────────────────────────────────────

void EditProfileSelect::setup() {
  // Publish initial selection: "Profile 1"
  this->publish_state("Profile 1");
}

void EditProfileSelect::control(const std::string &value) {
  this->publish_state(value);
  if (!this->circle_) return;
  // Parse "Profile N" → N-1
  int idx = 0;
  if (value.size() > 8) {
    idx = atoi(value.c_str() + 8) - 1;
  }
  this->circle_->set_edit_profile(idx);
  ESP_LOGD("the_circle", "Edit cursor: profile=%d", idx);
}

// ── EditStripSelect ──────────────────────────────────────────────────────

void EditStripSelect::setup() {
  this->publish_state("Inner Aura");
}

void EditStripSelect::control(const std::string &value) {
  this->publish_state(value);
  if (!this->circle_) return;
  int idx = 0;
  if (value == "Outer Aura") idx = 1;
  else if (value == "Inner Glow") idx = 2;
  this->circle_->set_edit_strip(idx);
  ESP_LOGD("the_circle", "Edit cursor: strip=%d", idx);
}

// ── EditLayerSelect ──────────────────────────────────────────────────────

void EditLayerSelect::setup() {
  this->publish_state("Layer 1");
}

void EditLayerSelect::control(const std::string &value) {
  this->publish_state(value);
  if (!this->circle_) return;
  int idx = 0;
  if (value.size() > 6) {
    idx = atoi(value.c_str() + 6) - 1;
  }
  this->circle_->set_edit_layer(idx);
  ESP_LOGD("the_circle", "Edit cursor: layer=%d", idx);
}

// ── PrimitiveTypeSelect ──────────────────────────────────────────────────

void PrimitiveTypeSelect::setup() {
  this->publish_state("None");
}

void PrimitiveTypeSelect::control(const std::string &value) {
  this->publish_state(value);
  if (!this->circle_) return;

  // Find matching type from names array
  PrimitiveType type = PRIM_NONE;
  for (int i = 0; i < NUM_PRIM_TYPE_NAMES; i++) {
    if (value == PRIM_TYPE_NAMES[i]) {
      type = static_cast<PrimitiveType>(i);
      break;
    }
  }

  this->circle_->set_edit_primitive(type);
  ESP_LOGD("the_circle", "Set primitive type: %s (%d)", value.c_str(), type);
}

// ── ActiveProfileSelect ──────────────────────────────────────────────────

void ActiveProfileSelect::setup() {
  int cur = this->circle_ ? this->circle_->get_current_profile() : 0;
  char buf[16];
  snprintf(buf, sizeof(buf), "Profile %d", cur + 1);
  this->publish_state(std::string(buf));
}

void ActiveProfileSelect::control(const std::string &value) {
  this->publish_state(value);
  if (!this->circle_) return;
  int idx = 0;
  if (value.size() > 8) {
    idx = atoi(value.c_str() + 8) - 1;
  }
  this->circle_->on_set_profile(idx);
}

// ═══════════════════════════════════════════════════════════════════════════
// Editing numbers
// ═══════════════════════════════════════════════════════════════════════════

void ParamNumber::setup() { this->publish_state(0.0f); }
void ParamNumber::control(float value) {
  this->publish_state(value);
  if (!this->circle_) return;
  auto *prim = this->circle_->get_edit_primitive();
  if (!prim || this->param_index_ < 0 || this->param_index_ >= 8) return;
  prim->params[this->param_index_] = value;
}

void ColorComponentNumber::setup() { this->publish_state(0.0f); }
void ColorComponentNumber::control(float value) {
  this->publish_state(value);
  if (!this->circle_) return;
  auto *prim = this->circle_->get_edit_primitive();
  if (!prim) return;
  int ci = this->circle_->get_edit_color_index();
  if (ci < 0 || ci >= MAX_COLORS) return;
  uint8_t v = (uint8_t)std::min(255.0f, std::max(0.0f, value));
  switch (this->channel_) {
    case 0: prim->colors[ci].r = v; break;
    case 1: prim->colors[ci].g = v; break;
    case 2: prim->colors[ci].b = v; break;
  }
}

void ColorIndexNumber::setup() { this->publish_state(0.0f); }
void ColorIndexNumber::control(float value) {
  this->publish_state(value);
  if (this->circle_) this->circle_->set_edit_color_index((int)value);
}

void IntensityNumber::setup() { this->publish_state(255.0f); }
void IntensityNumber::control(float value) {
  this->publish_state(value);
  if (!this->circle_) return;
  auto *prim = this->circle_->get_edit_primitive();
  if (!prim) return;
  prim->intensity = (uint8_t)std::min(255.0f, std::max(0.0f, value));
}

void ValueMapNumber::setup() { this->publish_state(this->is_max_ ? 100.0f : 0.0f); }
void ValueMapNumber::control(float value) {
  this->publish_state(value);
  if (!this->circle_) return;
  auto *prim = this->circle_->get_edit_primitive();
  if (!prim) return;
  if (this->is_max_) prim->value_map_max = value;
  else prim->value_map_min = value;
}

void ThresholdNumber::setup() { this->publish_state(this->threshold_index_ == 0 ? 33.0f : 66.0f); }
void ThresholdNumber::control(float value) {
  this->publish_state(value);
  if (!this->circle_) return;
  auto *prim = this->circle_->get_edit_primitive();
  if (!prim) return;
  if (this->threshold_index_ == 0) prim->threshold.threshold1 = value;
  else prim->threshold.threshold2 = value;
}

void ThresholdEnabledNumber::setup() { this->publish_state(0.0f); }
void ThresholdEnabledNumber::control(float value) {
  this->publish_state(value);
  if (!this->circle_) return;
  auto *prim = this->circle_->get_edit_primitive();
  if (!prim) return;
  prim->threshold.enabled = (value >= 0.5f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Editing text
// ═══════════════════════════════════════════════════════════════════════════

void EntityIdText::setup() { this->publish_state(""); }
void EntityIdText::control(const std::string &value) {
  this->publish_state(value);
  if (!this->circle_) return;
  auto *prim = this->circle_->get_edit_primitive();
  if (!prim) return;
  float vmin = prim->value_map_min;
  float vmax = prim->value_map_max;
  this->circle_->bind_edit_entity(value, vmin, vmax);
}

// ── ProfileNameText ──────────────────────────────────────────────────────

void ProfileNameText::setup() {
  const char *name = this->circle_ ? this->circle_->get_edit_profile_name() : "";
  this->publish_state(std::string(name));
}

void ProfileNameText::control(const std::string &value) {
  this->publish_state(value);
  if (!this->circle_) return;
  this->circle_->set_edit_profile_name(value);
}

}  // namespace the_circle
}  // namespace esphome
