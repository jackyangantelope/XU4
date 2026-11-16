# Release validation

- The November 11, 2026 waitexit1 hardware run recorded 845 movement frames in 35.207737 seconds, averaging 24.0004065 FPS with zero missed slots. The user confirmed sound, picture and movement speed. The middle audio window recorded eight padded frames.
- The public Fruit Jam source built from an independent clone. Flash use was 1,256,372 bytes and main SRAM use was 299,984 bytes (61.03% of the 480 KiB region). The UF2 SHA-256 was `526d3b92656ae4cebdc88119eea1515aa0e2826e36d9baf59e936cd2b02c78ef`.
- Host ASan/UBSan checks passed for action-wait exits, presentation scheduling and UART queue behavior. The audio preparation tools generated sidecars from the title sound, gate sound and MIDI fanfare. MIDI timbre depends on the supplied soundfont.
- The release uses SD OGG decoding and the existing runtime effect cache in place of embedded game audio. The graphics diagnostic compares a CRC against its reference. Fruit Jam physical performance, first-use sound loading, audio and longer gameplay are pending.
