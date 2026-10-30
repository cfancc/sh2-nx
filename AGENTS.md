# sh2-nx Chinese maintenance

Read `docs/MAINTENANCE_ZH.md` before updating, building or packaging the Chinese port.

- This is the active maintained source repository. Its `tools/`, `tests/local/`,
  maintenance documentation and this file are functional maintenance inputs.
  Treat them separately from disposable build output when cleaning up.
- Follow an upstream update by identifying its corresponding source tag/commit,
  migrating the small Chinese adaptation, regenerating and rebuilding. Do not
  describe the workflow as automatically decompiling or patching an arbitrary NRO.
- Keep upstream audio/render/input fixes. Review the resource path, Japanese-slot
  pause resource, font/width loader, initialization and texture upload hooks.
- On v1.02 and later prefer upstream ee_fonts_init() and its PNG/HD glyph cache.
  Initialize the legacy TGA overlay only when font_px is NULL; never activate both
  paths. Keep ee_lang.c's executable-string loader and the parser/pause wrappers
  in ee.c. The old rt_path e-to-j alias alone did not load any menu strings and
  was removed in 1.02-zh2; do not restore it or rename the pack's _j.mes files.
  After changes to the PNG path, run the additional native PNG checks documented
  in docs/CHINESE.md; the standard helper's TGA tests exercise the fallback only.
- Guest addresses in ee_font.c target the supported PC exe, not NRO file offsets.
  Verify the PC executable size and SHA-1 before using those addresses.
- Use `tools/build_zh.ps1 -CheckOnly` for readiness checks, then the full helper for
  generation, native resource/font checks and the Switch build. Default to full
  regeneration after upstream changes. The helper handles Windows CRLF shell files.
- Use `tools/convert_ee_font.py` only when a correct matching Chinese TGA is absent
  or needs regenerating from PNG. Preserve alpha and the matching fontwdata.bin.
- Use `tools/package_zh.py` for a new minimal package. Default to only NRO + lang;
  language.ini is opt-in. For v1.02, the original PNG pack needs no generated TGA;
  --no-zip produces an unpacked folder. Keep its original r_menu_j.res for the
  custom loader. Do not overwrite a current package while preparing one.
- Game exe, fonts, translations, saves and other game assets are private inputs.
  Never stage or publish game/, generated src/recomp/gen/, build/, .venv/ or dist/.
- Native tests do not prove Switch behavior. Report hardware validation separately.
- Keep any existing user edits. Do not publish commits or create a PR merely as a
  side effect of maintenance; follow the user's authorization for those actions.
- Historical diagnostic archives are intentionally removed. No maintenance step
  should depend on the deleted sh2-nx-diagnosis directory.
