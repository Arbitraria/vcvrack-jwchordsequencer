# Bar-Level Chord Sequencer — UI Specification (VCV Rack v2)

## 1. Scope and Assumptions
- **Platform**: VCV Rack v2
- **Function**: Sequence chord *root* and *type* **by bar**, not by note
- **Max length**: 32 bars
- **Clocking**: External clock (1 pulse = 1 beat)
- **Target module**: JW Quantizer (chord mode)
- **HP width**: 8 HP (assumed; geometry easily adjustable)
- **Design goal**: Minimal, performance-safe, low visual clutter

---

## 2. Panel Geometry
- **Width**: 8 HP (≈ 40.64 mm)
- **Height**: Standard Rack (128.5 mm)
- **Coordinate system**: mm from top-left

---

## 3. Parameters (Knobs)

### 3.1 Sequence Length
- **ID**: `LENGTH_PARAM`
- **Range**: 1–32 (integer, snapped)
- **Default**: 8
- **Control**: `RoundLargeBlackKnob`
- **Position**: (20.3 mm, 18 mm)
- **Label**: "LENGTH"
- **Display**: Numeric readout below knob

### 3.2 Bar Selector
- **ID**: `BAR_SELECT_PARAM`
- **Range**: 0–31 (integer, snapped)
- **Default**: 0
- **Behavior**: Selects which bar is being edited
- **Control**: `RoundLargeBlackKnob`
- **Position**: (20.3 mm, 42 mm)
- **Label**: "BAR"
- **Display**: Numeric (1–32) reflecting selected bar

### 3.3 Root Selector
- **ID**: `ROOT_PARAM`
- **Range**: 0–11 (C–B)
- **Default**: 0 (C)
- **Control**: `RoundBlackKnob`
- **Position**: (10.0 mm, 72 mm)
- **Label**: "ROOT"
- **Display**: Note name (C, C#, D, …)

### 3.4 Chord Type Selector
- **ID**: `CHORD_PARAM`
- **Range**: 0–N (JW Quantizer chord table)
- **Default**: 0
- **Control**: `RoundBlackKnob`
- **Position**: (30.6 mm, 72 mm)
- **Label**: "CHORD"
- **Display**: Short chord name (maj, min, dim, 7, m7, etc.)

---

## 4. Inputs (Jacks)

### 4.1 Clock Input
- **ID**: `CLOCK_INPUT`
- **Type**: Gate
- **Position**: (10.0 mm, 102 mm)
- **Label**: "CLK"
- **Behavior**: Rising edge advances beat counter

### 4.2 Reset Input
- **ID**: `RESET_INPUT`
- **Type**: Gate
- **Position**: (30.6 mm, 102 mm)
- **Label**: "RST"
- **Behavior**: Resets bar and beat counters to zero

---

## 5. Outputs (Jacks)

### 5.1 Root CV Output
- **ID**: `ROOT_OUTPUT`
- **Position**: (10.0 mm, 118 mm)
- **Label**: "ROOT"
- **Encoding**: 1 semitone = 1/12 V (0 V = C)
- **Quantization assumption**: Continuous CV, semitone-indexed
- **Update timing**: On bar change

### 5.2 Chord CV Output
- **ID**: `CHORD_OUTPUT`
- **Position**: (30.6 mm, 118 mm)
- **Label**: "CHORD"
- **Encoding**: **1 V per chord index**
- **Indexing model**:
  - 0 V → chord index 0
  - 1 V → chord index 1
  - 2 V → chord index 2
  - …
- **Expected behavior (JW Quantizer)**:
  - CV is internally rounded or floored to nearest integer index
  - Values outside valid chord table are clamped
- **Update timing**: On bar change

---

## 6. Lights / Indicators

### 6.1 Current Bar Indicator
- **ID**: `BAR_LIGHT`
- **Type**: LED
- **Position**: (20.3 mm, 58 mm)
- **Behavior**: Pulses briefly on bar advance

---

## 7. Displays (Custom Widgets)

### 7.1 Length Display
- **Source**: `LENGTH_PARAM`
- **Format**: Integer
- **Position**: (20.3 mm, 26 mm)

### 7.2 Bar Display
- **Source**: `BAR_SELECT_PARAM`
- **Format**: Integer (1-based)
- **Position**: (20.3 mm, 50 mm)

### 7.3 Root Name Display
- **Source**: `ROOT_PARAM`
- **Format**: Note name
- **Position**: (10.0 mm, 80 mm)

### 7.4 Chord Name Display
- **Source**: `CHORD_PARAM`
- **Format**: Short chord string
- **Position**: (30.6 mm, 80 mm)

---

## 8. Interaction Rules
- Editing **Root** or **Chord** writes values into the currently selected bar
- Sequence length clamps playback wrap but does not erase stored data
- Changing length mid-play clamps bar index safely
- All knobs snap to integers

---

## 9. JW Quantizer Compatibility Notes

### 9.1 Chord Table Assumptions
- JW Quantizer exposes a **discrete chord table** selectable by CV
- Chord selection is **index-based**, not pitch-based
- Sequencer outputs chord CV as an **absolute index**, not normalized

### 9.2 Implementation Contract
- Sequencer MUST:
  - Output stable CV for entire bar duration
  - Only change chord CV at bar boundaries
- Sequencer MUST NOT:
  - Slew or interpolate chord CV
  - Apply musical interpretation (no inversions, voicings, or tensions)

### 9.3 Defensive Encoding
- Internally clamp chord index to `[0, chordCount - 1]`
- Treat unknown JW chord table sizes as runtime constants

---

## 10. Future-Proofing (Non-UI)
- Internal arrays sized to 32 bars
- UI designed so additional CV inputs (Length CV, Bar CV) can be added later without panel redesign
