# Working on this repository

Read README.md, docs/architecture.md and data/SOURCES.json first.

- Target: black-and-white NOTE4 / ESP32-S3 / 400×300 / 1 BPP.
- Preserve stable word IDs, snapshot compatibility and the current partition layout.
- Build resources from data/books.json. Do not regenerate source datasets implicitly.
- Do not invent missing IPA/POS/translations or mark draft data as official/certified.
- Never commit .local, device identifiers, backup images, credentials, original
  PDF/XLSX files, build output, or locally generated font assets.
- Do not disable watchdogs or reintroduce broad interactive partial refreshes
  without real panel tests. Keep 12KiB main and 6KiB input task stack headroom.
- Run tools/audit_registered_vocab.py, tools/check-offline.sh and the firmware build.
- If schema/content changes, explain ID migration and verify snapshots across versions.
- Flash only with explicit user authorization. Prefer same-partition application-only
  update after backing up progress; first-install erases learning data.
- Code MIT licensing does not relicense vocabulary data or third-party assets.
