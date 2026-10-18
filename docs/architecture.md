# Architecture and data contract

`core/learning.*` implements spaced review and debounced release-based clicks.
`core/offline.*` holds the book/unit/session state, per-word progress, favorites,
wrong markers and daily totals. `snapshot_store.*` writes two independently
validated, chunked banks, committing a CRC/length manifest last.

`firmware/main/runtime_ui.*` renders native monochrome frames. Regular cards use
16/20/28/36px subsets; the dashboard has its own digits/text subset. Long answers
paginate; grading is enabled only on their final page. Interactive updates use
full refresh; automatic clock ticks use partial refresh only inside the time row.

`data/books.json` is the authoritative build registry. An installed book references
a JSON with `words`, `word_count`, `status` and `source_manifest`. Each word has:

```json
{
  "word_id": "persistent_identity",
  "lemma": "example",
  "ipa": null,
  "pos": "n.",
  "gloss": "示例",
  "units": ["unit-1"],
  "source_pages": [],
  "content_status": "user_supplied_draft",
  "review_flags": []
}
```

A registry entry's `unit_ids` corresponds to `units` labels after the first
“All units” label. At most 16 subdivisions per book and 10,000 global IDs are
currently supported. Shared IDs must have exactly matching lemma/IPA/POS/gloss.
Different senses require different persistent IDs. Unknown pages are allowed and
never shown as textbook page numbers on the device.

The generator validates then reads datasets without changing them. It emits
ignored `firmware/main/generated/` files. Fonts are selected by local environment
variables; no rasterized fonts are committed.

Content signature changes rebuild the active session while retaining matched
per-word progress. Retired IDs are not archived forever; keep pre-update progress
backups. A long answer's subpage position is RAM-only and starts at its first
answer page after restart; card face and progress remain snapshot-backed.

All currently installed text is compiled into the application. There is no
on-device content installer, cloud synchronization, OTA, audio playback or speech
scoring yet. Full snapshot writes and candidate-model copies require real-hardware
latency/heap/wear validation for larger catalogs.
