/**
 * @file renderer.h
 * @brief Renderer – composites primitive layers onto an LED strip buffer.
 *
 * Algorithm (painter's):
 *   1. Clear buffer to BLACK
 *   2. For each layer (0 → N-1), call primitive->render()
 *   3. Each primitive writes only the LEDs it uses
 *   4. Later layers overwrite earlier ones on same LED
 *   5. Copy final buffer to the AddressableLight
 *
 * Ref: Requirement – "painter's algorithm (ultimo layer vince su conflitto LED)"
 */

#pragma once

#include "profile.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/core/color.h"

namespace esphome {
namespace the_circle {

/**
 * Render one strip: composites all active layers of a profile onto a light.
 *
 * @param profile       Active profile
 * @param strip_idx     Which strip (0=Inner Aura, 1=Outer Aura, 2=Inner Glow)
 * @param light         The AddressableLight to write into
 * @param now_ms        Current time in ms (for animated primitives)
 * @param work_buffer   Temporary Color buffer, must be >= num_leds in size
 * @param num_leds      Number of LEDs on this strip
 */
inline void render_strip(Profile &profile, int strip_idx,
                         light::AddressableLight *light,
                         uint32_t now_ms,
                         Color *work_buffer, int num_leds) {
  // Step 1: Clear work buffer to black
  for (int i = 0; i < num_leds; i++) {
    work_buffer[i] = Color::BLACK;
  }

  // Step 2–4: Render each enabled layer in order (painter's algorithm)
  for (int l = 0; l < profile.layer_count[strip_idx]; l++) {
    Layer &layer = profile.layers[strip_idx][l];
    if (!layer.enabled || !layer.primitive) continue;

    // Each primitive writes its LEDs into work_buffer,
    // overwriting whatever was there (painter's)
    layer.primitive->render(work_buffer, num_leds, now_ms);
  }

  // Step 5: Copy work buffer into the AddressableLight
  // Ref: ESPHome AddressableLight API – operator[] returns ESPColorView
  for (int i = 0; i < num_leds; i++) {
    light->get(i) = work_buffer[i];
  }
}

}  // namespace the_circle
}  // namespace esphome
