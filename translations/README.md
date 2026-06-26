# Entity label translations / Traduzioni etichette entità

EN/FR/IT/ES/DE label sets for every user-facing entity exposed by The Circle.

## Why these files exist

ESPHome entity `name:` values are fixed English strings compiled into the
firmware — ESPHome does not localize them at runtime. These JSON files provide
the canonical translation of each entity label so you can rename the entities in
Home Assistant (Settings → Devices & Services → the-circle → entity → Settings →
Name) in your own language, with a consistent reference.

Each file has the shape:

```json
{
  "language": "it",
  "entity_labels": {
    "Inner Aura": "Aura Interna",
    "...": "..."
  }
}
```

Key = the English entity name as emitted by the firmware. Value = the localized
label for that language.

## Available languages

| File | Language |
|------|----------|
| `en.json` | English (reference) |
| `it.json` | Italiano |
| `fr.json` | Français |
| `es.json` | Español |
| `de.json` | Deutsch |

## Note

These are reference label maps, not a Home Assistant custom-integration
`translations/` bundle (The Circle is an ESPHome firmware component, not a
Python integration). If you build a companion HA integration or Lovelace card,
reuse these same keys to keep naming consistent across the project.
