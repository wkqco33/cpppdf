# Performance Notes

The library keeps the input PDF in memory and parses indirect objects lazily. This
favors simple ownership and predictable access over minimum peak memory use.

## Current safeguards

- Indirect objects are cached by object number and generation.
- Decoded stream data is reused by the parsed stream object.
- No benchmark data is committed yet; optimization changes should be measurement-led.

## Follow-up work

The main candidates for measured optimization are repeated page content copies,
`PdfObject` dictionary/array copies, and CMap lookups for long text streams. Add a
representative benchmark and memory measurement before changing ownership or public API
behavior. PDF input is untrusted, so size and recursion limits take priority over
throughput optimizations.
