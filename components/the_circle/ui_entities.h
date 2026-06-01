/**
 * @file ui_entities.h
 * @brief Web UI / HA entities for The Circle layer editor.
 *
 * "Cursor-based" editing: 3 navigation selects (profile/strip/layer)
 * determine which layer is being edited. The editing entities
 * (type, params, colors, entity_id, threshold) reflect and modify
 * the currently selected layer.
 *
 * Entities registered:
 *   Navigation:
 *     - select: Edit Profile  (1–6)
 *     - select: Edit Strip    (Inner Aura / Outer Aura / Inner Glow)
 *     - select: Edit Layer    (1–6)
 *
 *   Editing:
 *     - select: Primitive Type (none/dot/arc/trail/solid/gradient/segment/...)
 *     - number: Param 0–3
 *     - number: Color R, Color G, Color B
 *     - number: Color Index (0–3)
 *     - number: Intensity (0–255)
 *     - text:   Entity ID
 *     - number: Value Min, Value Max
 *     - switch: Threshold Enabled  (via number 0/1)
 *     - number: Threshold 1, Threshold 2
 *
 * Ref: ESPHome Select/Number/Text component APIs
 *   https://esphome.io/components/select/
 *   https://esphome.io/components/number/
 *   https://esphome.io/components/text/
 */

#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include "esphome/core/component.h"

// Forward declaration – avoid circular include
namespace esphome {
namespace the_circle {
class TheCircleComponent;
}
}  // namespace esphome

namespace esphome {
namespace the_circle {

// ── Primitive type names for the select dropdown ─────────────────────────
// Order must match PrimitiveType enum values (0=none, 1=dot, ...)
static const char *const PRIM_TYPE_NAMES[] = {
    "None",       // 0 PRIM_NONE
    "Dot",        // 1
    "Arc",        // 2
    "Trail",      // 3
    "Solid",      // 4
    "Gradient",   // 5
    "Segment",    // 6
    "Pulse",      // 7
    "Spin",       // 8
    "Rainbow",    // 9
    "Strobe",     // 10
    "Sparkle",    // 11
    "Comet",      // 12
    "Threshold",  // 13
};
static const int NUM_PRIM_TYPE_NAMES = 14;

// ── Strip names for the select dropdown ──────────────────────────────────
static const char *const STRIP_NAMES[] = {
    "Inner Aura",
    "Outer Aura",
    "Inner Glow",
};

// ═══════════════════════════════════════════════════════════════════════════
// CircleSelect – base class for all The Circle select entities
// ═══════════════════════════════════════════════════════════════════════════

class CircleSelect : public select::Select, public Component {
 public:
  void set_circle(TheCircleComponent *circle) { this->circle_ = circle; }
  float get_setup_priority() const override {
    return setup_priority::AFTER_CONNECTION - 1.0f;
  }

 protected:
  TheCircleComponent *circle_{nullptr};
};

// ═══════════════════════════════════════════════════════════════════════════
// CircleNumber – base class for all The Circle number entities
// ═══════════════════════════════════════════════════════════════════════════

class CircleNumber : public number::Number, public Component {
 public:
  void set_circle(TheCircleComponent *circle) { this->circle_ = circle; }
  float get_setup_priority() const override {
    return setup_priority::AFTER_CONNECTION - 1.0f;
  }

 protected:
  TheCircleComponent *circle_{nullptr};
};

// ═══════════════════════════════════════════════════════════════════════════
// CircleText – base class for all The Circle text entities
// ═══════════════════════════════════════════════════════════════════════════

class CircleText : public text::Text, public Component {
 public:
  void set_circle(TheCircleComponent *circle) { this->circle_ = circle; }
  float get_setup_priority() const override {
    return setup_priority::AFTER_CONNECTION - 1.0f;
  }

 protected:
  TheCircleComponent *circle_{nullptr};
};

// ═══════════════════════════════════════════════════════════════════════════
// Navigation selects
// ═══════════════════════════════════════════════════════════════════════════

/**
 * EditProfileSelect – selects which profile is being edited.
 * Note: this is separate from the "active" profile. You can edit
 * a profile while another one is rendering.
 */
class EditProfileSelect : public CircleSelect {
 public:
  void setup() override;
  void control(const std::string &value) override;
};

/**
 * EditStripSelect – selects which strip (0–2) is being edited.
 */
class EditStripSelect : public CircleSelect {
 public:
  void setup() override;
  void control(const std::string &value) override;
};

/**
 * EditLayerSelect – selects which layer (0–5) is being edited.
 */
class EditLayerSelect : public CircleSelect {
 public:
  void setup() override;
  void control(const std::string &value) override;
};

/**
 * PrimitiveTypeSelect – sets/shows the primitive type of the current layer.
 */
class PrimitiveTypeSelect : public CircleSelect {
 public:
  void setup() override;
  void control(const std::string &value) override;
};

/**
 * ActiveProfileSelect – selects which profile is currently rendering.
 */
class ActiveProfileSelect : public CircleSelect {
 public:
  void setup() override;
  void control(const std::string &value) override;
};

// ═══════════════════════════════════════════════════════════════════════════
// Editing numbers
// ═══════════════════════════════════════════════════════════════════════════

/**
 * ParamNumber – edits params[N] of the current layer's primitive.
 * The param_index is set via set_param_index().
 */
class ParamNumber : public CircleNumber {
 public:
  void set_param_index(int idx) { this->param_index_ = idx; }
  void setup() override;
  void control(float value) override;

 private:
  int param_index_{0};
};

/**
 * ColorComponentNumber – edits R, G, or B of a color slot.
 * channel: 0=R, 1=G, 2=B
 */
class ColorComponentNumber : public CircleNumber {
 public:
  void set_channel(int ch) { this->channel_ = ch; }
  void setup() override;
  void control(float value) override;

 private:
  int channel_{0};  // 0=R, 1=G, 2=B
};

/**
 * ColorIndexNumber – selects which color slot (0–3) is being edited.
 */
class ColorIndexNumber : public CircleNumber {
 public:
  void setup() override;
  void control(float value) override;
};

/**
 * IntensityNumber – edits intensity (0–255) of the current layer.
 */
class IntensityNumber : public CircleNumber {
 public:
  void setup() override;
  void control(float value) override;
};

/**
 * ValueMapNumber – edits value_map_min or value_map_max.
 * is_max: false = min, true = max
 */
class ValueMapNumber : public CircleNumber {
 public:
  void set_is_max(bool is_max) { this->is_max_ = is_max; }
  void setup() override;
  void control(float value) override;

 private:
  bool is_max_{false};
};

/**
 * ThresholdNumber – edits threshold1 or threshold2.
 * threshold_index: 0 = threshold1, 1 = threshold2
 */
class ThresholdNumber : public CircleNumber {
 public:
  void set_threshold_index(int idx) { this->threshold_index_ = idx; }
  void setup() override;
  void control(float value) override;

 private:
  int threshold_index_{0};
};

/**
 * ThresholdEnabledNumber – 0 or 1, enables/disables threshold modifier.
 */
class ThresholdEnabledNumber : public CircleNumber {
 public:
  void setup() override;
  void control(float value) override;
};

// ═══════════════════════════════════════════════════════════════════════════
// Editing text
// ═══════════════════════════════════════════════════════════════════════════

/**
 * EntityIdText – edits the HA entity_id binding of the current layer.
 */
class EntityIdText : public CircleText {
 public:
  void setup() override;
  void control(const std::string &value) override;
};

/**
 * ProfileNameText – edits the name of the currently selected edit profile.
 */
class ProfileNameText : public CircleText {
 public:
  void setup() override;
  void control(const std::string &value) override;
};

}  // namespace the_circle
}  // namespace esphome
