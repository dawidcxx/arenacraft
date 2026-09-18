# Custom folder

Legacy shim: `custom_script_loader.cpp` forwards `AddCustomScripts()` to
`AddArenacraftScripts()` in `src/game/Arenacraft/`. That experiment is slated
for removal — do not add new code there or here.

New custom gameplay belongs directly in the core (`src/game/`). When you need
event hooks, use the script registries in
`src/game/Scripting/ScriptDefines/` (PlayerScript, AllCreatureScript,
SpellScriptLoader, ...) and register scripts via
`src/scripts/ScriptLoader.cpp`.
