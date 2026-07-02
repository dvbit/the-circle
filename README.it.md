# The Circle – Componente ESPHome per Anelli LED Componibili

## Panoramica

The Circle è un componente esterno ESPHome che trasforma 3 strip LED circolari concentriche in un sistema componibile di notifiche e orologio. Gli effetti visivi sono costruiti da 13 **primitive** riusabili, composte in **profili** con fino a 6 layer per strip, e collegabili dinamicamente a entità Home Assistant a runtime — senza ricompilare.

## Installazione

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dvbit/the-circle
    components: [the_circle]
```

## Primitive Disponibili

| # | Tipo | Descrizione | Parametri |
|---|------|-------------|-----------|
| 1 | dot | Punto/cluster ad un angolo | angolo, spread |
| 2 | arc | Arco da inizio a fine | inizio, fine |
| 3 | trail | Barra di progresso 0°→angolo | angolo finale |
| 4 | solid | Strip intera di un colore | — |
| 5 | gradient | Sfumatura tra 2 colori | inizio, fine, 2 colori |
| 6 | segment | 4 segmenti proporzionali | 4 valori, 4 colori |
| 7 | pulse | Effetto respiro | velocità, min/max luminosità |
| 8 | spin | Punto rotante | spread, RPM, direzione |
| 9 | rainbow | Arcobaleno rotante | RPM |
| 10 | strobe | Lampeggio | frequenza, duty cycle |
| 11 | sparkle | LED casuali | densità, velocità |
| 12 | comet | Punto con coda sfumata | RPM, lunghezza coda, direzione |
| 13 | threshold | Colore per soglie di valore | modalità, spread |

## Esempi d'Uso

### Orologio con 3 lancette

```yaml
# Punto ore (rosso, spread=3) su Inner Aura
- action: esphome.the_circle_configure_layer
  data:
    profile: 0
    strip: 0
    layer: 0
    type: 1           # dot
    color_r: 255
    color_g: 0
    color_b: 0
    param1: 3          # spread
    entity_id: "sensor.the_circle_clock_hour_angle"
    value_min: 0
    value_max: 360
    intensity: 255
```

### Progresso Forno con Soglie Colore

```yaml
# Trail con threshold verde→giallo→rosso
- action: esphome.the_circle_configure_layer
  data:
    profile: 4
    strip: 0
    layer: 0
    type: 3            # trail
    color_r: 0
    color_g: 255
    color_b: 0
    entity_id: "sensor.forno_program_progress"
    value_min: 0
    value_max: 100
    intensity: 255

- action: esphome.the_circle_set_threshold
  data:
    profile: 4
    strip: 0
    layer: 0
    enabled: 1
    threshold1: 33
    threshold2: 66
    r0: 0
    g0: 255
    b0: 0
    r1: 255
    g1: 255
    b1: 0
    r2: 255
    g2: 0
    b2: 0
```

### Rinominare un Profilo

```yaml
service: esphome.the_circle_rename_profile
data:
  profile: 0
  name: "Orologio"
```

## Flusso di Editing via Web UI

1. Seleziona **Modifica Profilo** → quale profilo editare
2. Seleziona **Modifica Strip** → Inner Aura / Outer Aura / Inner Glow
3. Seleziona **Modifica Livello** → quale layer (1–6)
4. Seleziona **Tipo Primitiva** → crea la primitiva
5. Imposta **Param 0–3** → angolo, spread, velocità, ecc.
6. Imposta **Colore R/G/B** e **Slot Colore** (0–3)
7. Imposta **Intensità** (0–255)
8. Inserisci **ID Entità** → collega a entità HA
9. Seleziona **Profilo Attivo** → switcha il profilo in rendering
10. Chiama `save_profiles` per persistere su flash

## Servizi HA

| Servizio | Descrizione |
|----------|-------------|
| `set_profile` | Seleziona profilo attivo |
| `configure_layer` | Configura una primitiva |
| `set_layer_color` | Imposta colore slot 0–3 |
| `set_threshold` | Configura modificatore soglia |
| `bind_entity` | Collega entità HA a un layer |
| `clear_layer` | Rimuovi un layer |
| `rename_profile` | Rinomina un profilo |
| `save_profiles` | Salva su flash |

## Licenza

MIT
