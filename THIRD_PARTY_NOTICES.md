# Third-party code and data

- `firmware/components/zectrix_board` and `zectrix_epd`: ZECTRIX Lab NOTE4
  reference demo, MIT. Full notice: `licenses/ZECTRIX-MIT.txt`.
- `firmware/components/esp_codec_dev`: Espressif Systems, v1.5.11,
  Apache-2.0; its full license and upstream metadata remain in that directory.
  Upstream esp-adf commit: 73befa9ebffdd6e5065b7145329f115910e13ab5.
- ESP-IDF is an external toolchain and framework; it is not bundled here.
- Pillow and pyserial are external Python dependencies.
- Fonts are supplied locally by the builder. No font files or generated bitmap
  fonts are committed. Check your chosen fonts' bitmap redistribution terms.
- `data/` contains curated user-provided draft vocabularies, including material
  derived from PEP textbooks and Cambridge English lists, and third-party
  bilingual enrichments described as ECDICT-derived. `data/SOURCES.json` records
  provenance. No verified uniform redistribution license is supplied for these
  datasets. They are NOT relicensed under the code's MIT license; do not infer
  official endorsement, translation accuracy, exam completeness or commercial
  reuse permission from their inclusion.
- Cambridge, KET/PET, PEP and ZECTRIX names identify compatibility/source context;
  this is an independent project.
