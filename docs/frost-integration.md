# Frost Integration

Frost Rime is the default schema package for FluentPinyin v0.1.

The integration rules are:

- Install Frost as a managed package.
- Do not modify upstream Frost files in place.
- Store user customizations under `%APPDATA%\FluentPinyin\Rime`.
- Generate FluentPinyin-owned aggregate dictionaries for imported lexicons.
- Backup before every deploy or update.
- Roll back when deploy fails.

## Managed Package Layout

```text
%LOCALAPPDATA%\FluentPinyin\Packages\frost\
  current\
  previous\
  staging\
  manifest.json
```

## User Data Layout

```text
%APPDATA%\FluentPinyin\Rime\
  default.custom.yaml
  rime_frost.custom.yaml
  fp_user_dicts.txt
  fp_frost.dict.yaml
  <imported>.dict.yaml
```

## Initial MVP

MVP only exposes Frost full-pinyin. Double-pinyin and auxiliary-code controls
are intentionally not part of the GUI.

Custom lexicon import accepts RIME `.dict.yaml` files only. Users can convert
other dictionary formats with imewlconverter before importing.
