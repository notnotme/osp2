# TODO_6 — Plugin configuration + persisted user settings (INI)

> Last in the sequence — it touches everything: `applyTheme` + the Settings menu (TODO_3), the callback/Application layer (TODO_1), and the real FileBrowser's start path (TODO_4).

## Task chunks (implement, verify, and commit one at a time)

- [x] **6a — Settings domain + user settings**: `src/settings/Settings.{h,cpp}` (INI parser/writer, load/save, getters/setters), config-path helper, startup load with defaults-on-first-run, `UiActions::onThemeChange` change flow (apply + set + save), `default_folder` feeding the browser start path. CMakeLists gains `src/settings/Settings.cpp`. Verify: first run creates `osp2.ini`; theme change persists across restart; hand-edited `default_folder` is honored, invalid path falls back.
- [ ] **6b — Plugin settings backend**: `src/player/PluginSetting.h`, the two `PlayerPlugin` virtuals with safe defaults, `PlayerController::applyPluginSetting` (under `m_mutex`) + `getPluginSettings()`, OpenMptPlugin `stereo_separation`/`interpolation`, persisted `[plugin.X]` values pushed at startup. No UI yet — verify by hand-editing the INI and hearing/logging the effect.
- [ ] **6c — Settings UI**: generic widget loop in the Settings menu (`SliderInt`/`Combo` via visit, `PushID` per row), `UiActions::onPluginSettingChange`, plugin-settings list in `UiState`, save-on-change. Verify: change stereo separation while a track plays — immediate, no crash, INI rewritten.

Each chunk ends with green desktop + Switch builds, docs updated (`docs/settings.md`, `docs/audio.md`, `docs/ui.md`, `docs/application.md` as touched), user verification, then a commit.

## Context

Nothing persists between runs today: theme is applied at init only, the browser start path is compile-time, and plugins expose no tunables. TODO_6 adds a hand-editable INI settings file plus a generic plugin-configuration mechanism surfaced in the Settings menu:

- **Format: INI** — sections + `key = value`, hand-rolled ~60-line parser, zero dependencies (devkitPro-safe).
- **Location: next to the executable** — desktop: `SDL_GetBasePath() + "osp2.ini"` (lands in the build dir; already git-ignored); Switch: `sdmc:/switch/osp2.ini` (romfs is read-only).
- **Plugin config: generic schema** — plugins publish setting descriptors; the UI auto-generates widgets; persistence is uniform. New plugins get config UI for free.
- Persisted settings include the **color theme** and the **default folder**; the UI edits plugin config and user settings (at least theme); **default_folder is hand-edit only** — never shown in the UI.

## The file — osp2.ini

```ini
[user]
theme = dark              # dark | light
default_folder = /path/to/music   # hand-edit only; empty/invalid -> TODO_4 default start path

[plugin.openmpt]
stereo_separation = 100
interpolation = 3         # index into the plugin's enum options
```

Created with defaults on first launch (so the user can find and hand-edit it). Rewritten on every change (crash-safe for Switch homebrew). Unknown sections/keys are preserved on round-trip; comments are NOT preserved by the writer (documented limitation). Malformed lines: `SDL_Log` + skip.

## src/settings/Settings.{h,cpp} (new domain)

- Storage: `std::map<std::string, std::map<std::string, std::string>>` (ordered → deterministic output).
- `void load(const std::filesystem::path &)` — missing file → defaults + immediate `save()`; keeps the path for saving.
- `void save() const` — writes all sections via `std::ofstream`.
- `[[nodiscard]] std::string getString(section, key, fallback) const`, `getInt(...)`; `void setString(...)`, `setInt(...)` — setters mutate only; callers call `save()` after a batch.
- Main thread only; no SDL dependency beyond `SDL_Log`.
- Config path helper in main.cpp: `#ifdef __SWITCH__` → `"sdmc:/switch/osp2.ini"`, else `SDL_GetBasePath()` + `"osp2.ini"`.

## src/player/PluginSetting.h (new, header-only)

```cpp
struct IntRange   { int min; int max; };
struct EnumOptions { std::vector<std::string> labels; };   // value = index

struct PluginSetting {
    std::string key;                             // INI key, e.g. "stereo_separation"
    std::string label;                           // UI label, e.g. "Stereo separation"
    std::variant<IntRange, EnumOptions> shape;   // drives the widget
    int value;                                   // current value
};
```

## PlayerPlugin / PlayerController

- `PlayerPlugin` gains two virtuals with safe defaults (plugins without config override nothing):
  - `[[nodiscard]] virtual std::vector<PluginSetting> getSettings() const { return {}; }`
  - `virtual void applySetting(const std::string &key, int value) {}`
- **Threading**: `applySetting` may touch the live decoder, so it is only called via `PlayerController::applyPluginSetting(pluginName, key, value)` which takes `m_mutex` (same contract as decode/open). `getSettings()` returns plain cached values — main thread, no decoder access.
- `PlayerController` also gains `[[nodiscard]] std::vector<std::pair<std::string, std::vector<PluginSetting>>> getPluginSettings() const` (plugin name + descriptors, under lock) for the UI, and pushes persisted values at startup.
- **OpenMptPlugin** initial settings: `stereo_separation` (IntRange 0–200, default 100 → `set_render_param(RENDER_STEREOSEPARATION_PERCENT)`) and `interpolation` (EnumOptions default/none/linear/cubic/sinc → filter lengths 0/1/2/4/8 → `RENDER_INTERPOLATIONFILTER_LENGTH`). Values live in members, applied to the module in `open()` and immediately in `applySetting()` when a module is open.

## Wiring — main.cpp + Application

Startup order (main.cpp): `settings.load(configPath())` → `gui.applyTheme` from `[user] theme` (TODO_3's applyTheme) → `player.create()` → push every `[plugin.X] key` via `player.applyPluginSetting(X, key, value)` → `file_system.create(...)` start path = `default_folder` if set and a valid directory, else TODO_4's default (cwd / `sdmc:/`).

Change flow (Application, which gains a `Settings &` reference): UI callback → apply (theme or `player.applyPluginSetting`) → `settings.set...` → `settings.save()`.

## Settings UI (generic loop)

In TODO_3's Settings menu on the top bar:

- **User section**: theme radio Dark/Light (already drawn by TODO_3 — now it persists via the callback).
- **Per plugin**: `SeparatorText(pluginName)`, then one widget per descriptor — `IntRange` → `SliderInt`, `EnumOptions` → `Combo` — via `std::visit` on `shape`, `PushID(key)` per row. Zero plugin-specific UI code.
- New `UiActions` members: `onThemeChange(Theme)` and `onPluginSettingChange(pluginName, key, value)`; `UiState` gains the plugin-settings list (from `player.getPluginSettings()`, fetched in `makeUiState`).

## Docs / build

- **`docs/settings.md`** (new domain doc): classDiagram (Settings, PluginSetting, IntRange/EnumOptions) + notes: INI grammar, known keys, write-on-change, comments-not-preserved, main-thread-only, default_folder hand-edit rule.
- **`docs/audio.md`**: PlayerPlugin settings virtuals + the applySetting-under-mutex rule.
- **`docs/ui.md`**: generic settings widget loop.
- **`docs/application.md`**: Settings reference + change flow.
- **CMakeLists.txt**: add `src/settings/Settings.cpp`.

## Coordination

- **Requires TODO_1** (UiActions/UiState members, Application change flow), **TODO_3** (applyTheme + Settings menu), **TODO_4** (start path to override with `default_folder`).
- **Future plugins** (libgme/libsidplayfp/libsc68): publish descriptors, get UI + persistence for free.

## Verification

- Desktop + Switch builds green (per CLAUDE.md).
- First run creates `osp2.ini` with defaults next to the binary; changing theme/plugin settings in the UI rewrites it (verify with cat).
- Hand-edit `default_folder` → restart → browser starts there; invalid path → falls back to default start path.
- Change stereo separation / interpolation while a track plays → takes effect immediately without crash (mutex-protected).
- Malformed INI line → logged and skipped; app still starts. Unknown keys survive a settings rewrite.
