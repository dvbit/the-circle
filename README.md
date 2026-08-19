# The Circle – ESPHome Composable LED Ring Component

## Overview / Panoramica

**EN:** The Circle is an ESPHome external component that turns 3 concentric circular LED strips into a composable notification and clock display system. Visual effects are built from 13 reusable **primitives** (dot, arc, trail, gradient, etc.), composed into **profiles** with up to 6 layers per strip, and dynamically bound to Home Assistant entities at runtime — no recompilation needed.

**IT:** The Circle è un componente esterno ESPHome che trasforma 3 strip LED circolari concentriche in un sistema componibile di notifiche e orologio. Gli effetti visivi sono costruiti da 13 **primitive** riusabili (dot, arc, trail, gradient, ecc.), composte in **profili** con fino a 6 layer per strip, e collegabili dinamicamente a entità Home Assistant a runtime — senza ricompilare.

---

## Hardware

| Strip | Chip | LEDs | GPIO | Index |
|-------|------|------|------|-------|
| Inner Aura | SK9822 | 263 | D16/D17 (SPI) | 0 |
| Outer Aura | APA102 | 132 | D19/D18 (SPI) | 1 |
| Inner Glow | SK6812 RGBW | 111 | D26 | 2 |

All strips start at 12-o'clock (0°) and run clockwise to 360°.

Additional: LD2410 (presence), BH1750 (light), buzzer (GPIO23), 4× capacitive touch, BLE proxy.

---

## Installation / Installazione

**EN:** Add to your ESPHome YAML:

**IT:** Aggiungi al tuo YAML ESPHome:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dvbit/the-circle
    components: [the_circle]
```

Or for local development:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [the_circle]
```

---

## Architecture / Architettura

```
┌─────────────────────────────────────────────────┐
│ Home Assistant                                   │
│  ├─ entity states ──→ subscribe_homeassistant    │
│  ├─ services ──────→ register_service            │
│  └─ template sensors (clock angles)              │
└─────────────┬───────────────────────┬────────────┘
              │                       │
         HA values              configure_layer
              │                  set_profile
              ▼                  bind_entity ...
┌─────────────────────────────────────────────────┐
│ TheCircleComponent (ESP32)                       │
│  ├─ Profiles[6]                                  │
│  │   └─ Layers[3 strips][6 layers]               │
│  │       └─ Primitive (dot/arc/trail/solid/...)   │
│  │           ├─ colors[4], intensity, params[8]   │
│  │           ├─ ha_entity_id → ha_value           │
│  │           └─ threshold modifier                │
│  ├─ Renderer (painter's algorithm, ~60fps)        │
│  ├─ Flash persistence (NVS)                      │
│  └─ UI entities (select/number/text)             │
└─────────────────────────────────────────────────┘
```

---

## Primitives / Primitive

| # | Type | ID | Description | Key Params |
|---|------|----|-------------|------------|
| 1 | dot | 1 | Point/cluster at angle | angle, spread |
| 2 | arc | 2 | Arc from start to end | start, end |
| 3 | trail | 3 | Progress bar 0°→angle | end angle |
| 4 | solid | 4 | Entire strip one color | — |
| 5 | gradient | 5 | Color gradient on arc | start, end, 2 colors |
| 6 | segment | 6 | 4 proportional segments | 4 values, 4 colors |
| 7 | pulse | 7 | Breathing solid | speed, min/max brightness |
| 8 | spin | 8 | Rotating dot/arc | spread, RPM, direction |
| 9 | rainbow | 9 | Rotating rainbow | RPM |
| 10 | strobe | 10 | Flashing on/off | frequency, duty cycle |
| 11 | sparkle | 11 | Random flickering | density, speed |
| 12 | comet | 12 | Moving dot with tail | RPM, tail length, direction |
| 13 | threshold | 13 | Color by value zones | mode (solid/trail/dot), spread |

### Threshold Modifier

Any primitive can have a threshold modifier that overrides its color based on the HA entity value:
- Below `threshold1` → color 0 (default green)
- Between `threshold1` and `threshold2` → color 1 (default yellow)
- Above `threshold2` → color 2 (default red)

---

## YAML Configuration

### Minimal setup / Configurazione minima

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dvbit/the-circle
    components: [the_circle]

api:
  encryption:
    key: "YOUR_KEY"

light:
  - platform: fastled_spi
    id: i_a
    chipset: SK9822
    data_pin: GPIO16
    clock_pin: GPIO17
    num_leds: 263
    rgb_order: BGR
    name: "Inner Aura"
    default_transition_length: 0s

  - platform: fastled_spi
    id: o_a
    chipset: APA102
    data_pin: GPIO19
    clock_pin: GPIO18
    num_leds: 132
    rgb_order: BGR
    name: "Outer Aura"
    default_transition_length: 0s

  - platform: neopixelbus
    type: GRBW
    pin: GPIO26
    variant: SK6812
    num_leds: 111
    name: "Inner Glow"
    id: i_g
    default_transition_length: 0s

the_circle:
  id: circle
  strips:
    - light_id: i_a
    - light_id: o_a
    - light_id: i_g
  num_profiles: 6
  layers_per_strip: 6
```

### Web UI entities / Entità UI

```yaml
select:
  - platform: the_circle
    the_circle_id: circle
    edit_profile:
      name: "Edit Profile"
    edit_strip:
      name: "Edit Strip"
    edit_layer:
      name: "Edit Layer"
    primitive_type:
      name: "Primitive Type"
    active_profile:
      name: "Active Profile"

number:
  - platform: the_circle
    the_circle_id: circle
    param0:
      name: "Param 0"
    param1:
      name: "Param 1"
    param2:
      name: "Param 2"
    param3:
      name: "Param 3"
    color_r:
      name: "Color R"
    color_g:
      name: "Color G"
    color_b:
      name: "Color B"
    color_index:
      name: "Color Slot"
    intensity:
      name: "Intensity"
    value_min:
      name: "Value Min"
    value_max:
      name: "Value Max"
    threshold_enabled:
      name: "Threshold Enabled"
    threshold1:
      name: "Threshold 1"
    threshold2:
      name: "Threshold 2"

text:
  - platform: the_circle
    the_circle_id: circle
    entity_id_input:
      name: "Entity ID"
```

---

## HA Services / Servizi HA

| Service | Description | Key params |
|---------|-------------|------------|
| `esphome.the_circle_set_profile` | Switch active profile | `profile_index` |
| `esphome.the_circle_configure_layer` | Create/configure a primitive | `profile, strip, layer, type, color_r/g/b, param0-3, entity_id, value_min/max, intensity` |
| `esphome.the_circle_set_layer_color` | Set color slot 0–3 | `profile, strip, layer, color_index, r, g, b` |
| `esphome.the_circle_set_threshold` | Configure threshold modifier | `profile, strip, layer, enabled, threshold1/2, r0-2/g0-2/b0-2` |
| `esphome.the_circle_bind_entity` | Bind HA entity to layer | `profile, strip, layer, entity_id, value_min/max` |
| `esphome.the_circle_clear_layer` | Remove a layer | `profile, strip, layer` |
| `esphome.the_circle_save_profiles` | Persist to flash | — |

---

## Usage Examples / Esempi d'Uso

### Example 1: Clock / Orologio

**EN:** Three dots on Inner Aura — red hour (wide), green minute, blue second — bound to HA template sensors that compute angles from current time.

**IT:** Tre punti su Inner Aura — ora rossa (larga), minuto verde, secondo blu — collegati a template sensor HA che calcolano gli angoli dall'ora corrente.

Required HA template sensors (included in `ha_package_the_circle.yaml`):

```yaml
template:
  - sensor:
      - name: "The Circle Clock Hour Angle"
        state: "{{ ((now().hour % 12) * 30 + now().minute * 0.5) | round(1) }}"
      - name: "The Circle Clock Min Angle"
        state: "{{ (now().minute * 6 + now().second * 0.1) | round(1) }}"
      - name: "The Circle Clock Sec Angle"
        state: "{{ (now().second * 6) | round(1) }}"
```

Configuration via service:

```yaml
- action: esphome.the_circle_configure_layer
  data:
    profile: 0
    strip: 0      # Inner Aura
    layer: 0
    type: 1       # dot
    color_r: 255
    color_g: 0
    color_b: 0
    param0: 0     # angle from entity
    param1: 3     # spread = 3 LEDs each side
    entity_id: "sensor.the_circle_clock_hour_angle"
    value_min: 0
    value_max: 360
    intensity: 255
```

### Example 2: Oven Progress with Threshold / Avanzamento Forno

**EN:** A trail (progress bar) on Outer Aura that fills as cooking progresses, changing color at 33% and 66%.

**IT:** Un trail (barra di progresso) su Outer Aura che si riempie durante la cottura, cambiando colore al 33% e 66%.

```yaml
# Trail bound to oven sensor
- action: esphome.the_circle_configure_layer
  data:
    profile: 4
    strip: 1
    layer: 0
    type: 3           # trail
    color_r: 0
    color_g: 255
    color_b: 0
    entity_id: "sensor.forno_program_progress"
    value_min: 0
    value_max: 100
    intensity: 255

# Add threshold colors
- action: esphome.the_circle_set_threshold
  data:
    profile: 4
    strip: 1
    layer: 0
    enabled: 1
    threshold1: 33
    threshold2: 66
    r0: 0    # green
    g0: 255
    b0: 0
    r1: 255  # yellow
    g1: 255
    b1: 0
    r2: 255  # red
    g2: 0
    b2: 0
```

### Example 3: Energy Segments / Segmenti Energia

**EN:** 4-segment proportional display showing consumption, production, grid import, grid export.

**IT:** Visualizzazione proporzionale a 4 segmenti: consumo, produzione, prelievo, immissione.

```yaml
- action: esphome.the_circle_configure_layer
  data:
    profile: 3
    strip: 0
    layer: 0
    type: 6            # segment
    color_r: 255       # consumo: red
    color_g: 0
    color_b: 0
    entity_id: "sensor.consumo"
    value_min: 0
    value_max: 10000
    intensity: 255

# Set other segment colors
- action: esphome.the_circle_set_layer_color
  data: { profile: 3, strip: 0, layer: 0, color_index: 1, r: 0, g: 255, b: 0 }
- action: esphome.the_circle_set_layer_color
  data: { profile: 3, strip: 0, layer: 0, color_index: 2, r: 0, g: 0, b: 255 }
- action: esphome.the_circle_set_layer_color
  data: { profile: 3, strip: 0, layer: 0, color_index: 3, r: 255, g: 255, b: 0 }
```

---

## HA Package / Pacchetto HA

The file `ha_package_the_circle.yaml` contains:
- Template sensors for clock angles (hour/min/sec → 0–360°)
- Scripts to configure all 6 preset profiles
- Master script `the_circle_setup_all_presets` that runs all presets + saves to flash

Copy to `config/packages/the_circle.yaml` and run the master script once.

---

## Web UI Editing Workflow / Flusso di Editing

1. Select **Edit Profile** → which profile to edit
2. Select **Edit Strip** → Inner Aura / Outer Aura / Inner Glow
3. Select **Edit Layer** → which layer (1–6)
4. Select **Primitive Type** → creates the primitive
5. Set **Param 0–3** → angle, spread, speed, etc. (meaning depends on type)
6. Set **Color R/G/B** → primary color
7. Set **Color Slot** → switch to edit colors 1–3
8. Set **Intensity** → brightness 0–255
9. Type **Entity ID** → bind to HA entity
10. Set **Value Min/Max** → entity value range mapping
11. Set **Active Profile** → switch which profile is rendering
12. Call `save_profiles` to persist

---

## File Structure / Struttura File

```
the-circle/
├── README.md
├── the-circle.yaml                        # ESPHome config example
├── ha_package_the_circle.yaml             # HA package (templates + presets)
└── components/the_circle/
    ├── __init__.py                         # ESPHome component registration
    ├── select.py                           # Select entity platform
    ├── number.py                           # Number entity platform
    ├── text.py                             # Text entity platform
    ├── primitive.h                         # 13 primitive types + threshold
    ├── profile.h                           # Profile (3 strips × 6 layers)
    ├── renderer.h                          # Painter's algorithm compositor
    ├── storage.h                           # Flash persistence (NVS)
    ├── ui_entities.h                       # UI entity class declarations
    ├── ui_entities.cpp                     # UI entity implementations
    └── the_circle.h                        # Main component (lifecycle, services, binding)
```

---

## Specification Reference / Riferimento Specifica

This component was designed from the following consolidated requirement:

- **3 circular concentric LED strips**, all starting at 12-o'clock (0°), clockwise to 360°
- **13 composable primitives**: dot, arc, trail, solid, gradient, segment, pulse, spin, rainbow, strobe, sparkle, comet, threshold
- **6 profiles**, each with up to **6 layers per strip**, painter's algorithm compositing
- **Runtime HA entity binding** via `CustomAPIDevice::subscribe_homeassistant_state()` — no recompilation needed
- **Threshold modifier**: color override based on entity value thresholds, usable as standalone primitive or modifier on any primitive
- **String state mapping**: non-numeric HA states (home/not_home, on/off, etc.) automatically mapped to float values
- **Controls**: capacitive touch (profile navigation, brightness), web UI (full parameter config), HA services (programmatic configuration)
- **Persistence**: profile configurations survive reboot via ESP32 NVS flash preferences
- **~60fps rendering** with 16ms throttle

---

## Troubleshooting / Risoluzione problemi

### Linker error: `undefined reference to get_execute_arg_value<int>` / `to_service_arg_type<int>`

**EN** — This was caused by the component's HA service handlers declaring their
integer parameters as C++ `int`. ESPHome's API layer only defines the
`get_execute_arg_value` / `to_service_arg_type` specializations for `int32_t`,
`float` and `std::string` — never plain `int`. Even though `int` and `int32_t`
are the same width on ESP32, they are **distinct template arguments**, so
`register_service()` with `int` parameters instantiated
`UserServiceDynamic<int, …>` and the linker had no matching symbols
(ESPHome issue [#14470](https://github.com/esphome/esphome/issues/14470)).

**Fix (already applied since v1.4)**: all service-handler integer parameters use
`int32_t` instead of `int`, so the instantiations match the specializations
ESPHome provides. No YAML workaround is needed. If you write your own
`register_service()` handlers, use `int32_t` (and `float` / `std::string`) for
the argument types.

**IT** — L'errore era dovuto agli handler dei servizi HA che dichiaravano i
parametri interi come `int` C++. Il livello API di ESPHome definisce le
specializzazioni `get_execute_arg_value` / `to_service_arg_type` solo per
`int32_t`, `float` e `std::string`, mai per `int`. Pur avendo la stessa
larghezza su ESP32, `int` e `int32_t` sono **argomenti template distinti**:
`register_service()` con parametri `int` istanziava `UserServiceDynamic<int, …>`
senza simboli corrispondenti (issue
[#14470](https://github.com/esphome/esphome/issues/14470)).
**Soluzione (già applicata da v1.4)**: tutti i parametri interi degli handler
usano `int32_t`. Nessun workaround YAML necessario.

---

## License

MIT
