# The Circle – Componente ESPHome per anelli LED componibili

> Versione inglese: [README.md](README.md)

## Panoramica

The Circle è un componente esterno ESPHome che trasforma 3 strip LED circolari
concentriche in un sistema componibile di notifiche e orologio. Gli effetti
visivi sono costruiti da **primitive** riusabili (dot, arc, trail, gradient,
ecc.), composte in **profili** con più layer per strip, e collegabili
dinamicamente a entità Home Assistant a runtime — senza ricompilare.

---

## Hardware

| Strip | Chip | LED | GPIO | Indice |
|-------|------|-----|------|--------|
| Inner Aura | SK9822 | 263 | GPIO16/GPIO17 (SPI) | 0 |
| Outer Aura | APA102 | 132 | GPIO19/GPIO18 (SPI) | 1 |
| Inner Glow | SK6812 RGBW | 111 | GPIO26 (RMT) | 2 |

Tutte le strip partono da ore 12 (0°) e ruotano in senso orario fino a 360°.

Altro hardware: LD2410 (presenza, UART su GPIO27/GPIO35), BH1750 (luminosità,
I2C), buzzer (GPIO23), 4 pad touch capacitivi, BLE proxy.

> **Nota pin:** GPIO27 è dedicato alla UART del radar LD2410. Il pad "Touch Esc"
> usa GPIO13 (canale touch T4) per evitare il conflitto.

---

## Installazione

Aggiungi al tuo YAML ESPHome:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dvbit/the-circle
    components: [the_circle]
```

Oppure, per sviluppo locale, copia la cartella `components/the_circle` accanto
al tuo file di configurazione e usa `type: local`.

### API richieste

Il componente registra servizi e si iscrive agli stati di Home Assistant,
quindi l'API deve abilitare:

```yaml
api:
  custom_services: true
  homeassistant_states: true
```

### Definizione delle strip

```yaml
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

  # Inner Glow: SK6812 RGBW clockless via RMT
  # (neopixelbus è deprecato su ESP32 da ESPHome 2026.6)
  - platform: esp32_rmt_led_strip
    id: i_g
    pin: GPIO26
    num_leds: 111
    chipset: SK6812
    rgb_order: GRB
    is_rgbw: true
    name: "Inner Glow"
    default_transition_length: 0s

the_circle:
  id: circle
  strips:
    - light_id: i_a
    - light_id: o_a
    - light_id: i_g
  num_profiles: 20
  layers_per_strip: 6
```

---

## Esempi d'uso

### 1. Orologio (dot ore/minuti/secondi) sull'Inner Aura

Configura un profilo con 3 layer `dot` sulla strip 0, ciascuno legato alla
sorgente temporale (ore/minuti/secondi) con colori diversi. La composizione
usa l'algoritmo del pittore: l'ultimo layer disegnato vince in caso di
sovrapposizione.

### 2. Avanzamento forno (gradient) come notifica

Lega un layer `gradient` (strip Inner Aura) al sensore
`sensor.forno_program_progress`, con `value_min: 0` e `value_max: 100`. La
barra si riempie in proporzione alla percentuale di avanzamento.

### 3. Presenza in casa (segment per persona)

Quattro layer `solid`/`segment`, ciascuno legato a `person.<nome>` con mappatura
stato stringa → valore (`home` → 1, `not_home` → 0), su settori distinti
dell'anello.

### 4. Modificatore a soglia (semaforo)

Su qualsiasi primitiva attiva il `threshold_enabled` e imposta `threshold1`/
`threshold2`: il colore passa da rosso → giallo → verde al superamento delle
soglie. Utile per timer e progressi.

I parametri di ogni layer si configurano:
- via **Web UI** ESPHome / entità in Home Assistant (select, number, text);
- via **touch** (navigazione profili, luminosità);
- via **servizi HA** (configurazione programmatica).

---

## Specifica di partenza

Il componente è stato progettato a partire dal requisito consolidato seguente:

- **3 strip LED circolari concentriche**, tutte da ore 12 (0°), orarie fino a 360°
- **13 primitive componibili**: dot, arc, trail, solid, gradient, segment,
  pulse, spin, rainbow, strobe, sparkle, comet, threshold
- **Profili** con più **layer per strip**, compositing ad algoritmo del pittore
- **Binding a entità HA a runtime** via
  `CustomAPIDevice::subscribe_homeassistant_state()` — senza ricompilare
- **Modificatore a soglia**: override colore in base a soglie sul valore
  dell'entità, usabile come primitiva autonoma o come modificatore
- **Mappatura stati stringa**: stati HA non numerici (home/not_home, on/off…)
  mappati automaticamente a valori float
- **Controlli**: touch capacitivo, Web UI, servizi HA
- **Persistenza**: le configurazioni dei profili sopravvivono al riavvio
  tramite NVS flash dell'ESP32
- **Rendering ~60fps** con throttle a 16ms

---

## Changelog

### v1.2
- **Guardia `dynamic_cast` per il binding delle strip.** Il componente risolve
  l'`AddressableLight` di ogni strip con `dynamic_cast` invece di `static_cast`.
  Se viene legato un light non-addressable, il cast restituisce `nullptr`, la
  strip viene segnalata come errore e disabilitata, e il render loop la salta in
  sicurezza (nessun comportamento indefinito).
- **Traduzioni etichette entità (EN/FR/IT/ES/DE).** Aggiunta la cartella
  `translations/` con la mappa di riferimento delle etichette per ogni entità
  visibile, più note d'uso (`translations/README.md`). ESPHome non localizza i
  `name:` a runtime, quindi sono mappe di riferimento per rinominare le entità in
  Home Assistant in modo coerente.

### v1.1
- **Inner Glow migrato da `neopixelbus` a `esp32_rmt_led_strip`.** Su ESP32
  `neopixelbus` è deprecato da ESPHome 2026.6 (rimozione non oltre 2027.1; non
  compila su ESP-IDF 6). Il percorso ufficiale per le strip clockless
  SK6812/WS2812 è `esp32_rmt_led_strip` (periferica RMT). Il componente
  `the_circle` è agnostico rispetto al backend (risolve un `AddressableLight`
  dal `LightState`), quindi la modifica è trasparente per tutte le primitive.
- **Risolto il conflitto sul pin GPIO27.** GPIO27 era assegnato sia al `tx_pin`
  della UART del LD2410 sia al pad "Touch Esc". Touch Esc spostato su GPIO13
  (canale touch T4); GPIO27 resta alla UART del radar.
- **Note di calibrazione `esp32_touch`.** Documentato il driver touch unificato
  (ESP-IDF v5.5, ESPHome 2026.3) e la semantica delle soglie su hardware V1 (il
  valore raw diminuisce al tocco sull'ESP32 classic), con procedura `setup_mode`.
- **Versioning canonico del firmware** via `esphome.project`
  (`dvbit.the-circle`, versione `1.1`).
- **Igiene YAML**: booleani in minuscolo, commenti con riferimenti alla doc
  ufficiale.
- Validato su **ESPHome 2026.6.2** (stabile attuale).

> **Nota tecnica:** il componente risolve il buffer addressable con
> `static_cast<light::AddressableLight *>`. Tutte e 3 le strip configurate sono
> addressable, quindi è sicuro; per legare un light non-addressable servirebbe
> un controllo con `dynamic_cast`.

---

## Licenza

Vedi [README.md](README.md) per i dettagli di licenza.
