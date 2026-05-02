// MIDI Mapper standalone test harness
//
// Extracts the MIDI message manager (mapper) logic from
// M5Core2-MIDIXposeFilBT.ino verbatim and exercises it on the host PC,
// so the rule-matching / value-mapping / multi-rule behaviour and
// per-message latency can be verified without flashing the device.
//
// Build (Git Bash):
//   ./build_and_run.sh
// Or manually:
//   g++ -O2 -std=c++17 test_midi_mapper.cpp -o test_midi_mapper.exe
//   ./test_midi_mapper.exe

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <vector>

// ---------------------------------------------------------------
// Section 1: types and constants copied verbatim from the .ino
// ---------------------------------------------------------------

enum MidiMessageKind {
  MIDI_KIND_NOTE_OFF,
  MIDI_KIND_NOTE_ON,
  MIDI_KIND_KEY_PRESSURE,
  MIDI_KIND_PROGRAM_CHANGE,
  MIDI_KIND_CONTROL_CHANGE,
  MIDI_KIND_CHANNEL_PRESSURE,
  MIDI_KIND_PITCH_BEND,
  MIDI_KIND_SYSTEM_EXCLUSIVE,
  MIDI_KIND_MIDI_TIME_CODE,
  MIDI_KIND_SONG_POSITION,
  MIDI_KIND_SONG_SELECT,
  MIDI_KIND_TUNE_REQUEST,
  MIDI_KIND_CLOCK,
  MIDI_KIND_START,
  MIDI_KIND_CONTINUE,
  MIDI_KIND_STOP,
  MIDI_KIND_ACTIVE_SENSE,
  MIDI_KIND_SYSTEM_RESET,
  MIDI_KIND_COUNT
};

struct MidiMapperRule {
  bool enabled;
  MidiMessageKind srcKind;
  int8_t srcChannel;   // -1 = ALL
  int16_t srcData1;    // -1 = ANY
  int16_t srcMin;
  int16_t srcMax;
  MidiMessageKind dstKind;
  int8_t dstChannel;   // -1 = KEEP
  int16_t dstData1;    // -1 = KEEP
  int16_t dstMin;
  int16_t dstMax;
};

struct MidiMessage {
  uint8_t bytes[3];
  uint8_t length;
  MidiMessageKind kind;
  bool hasChannel;
  int8_t channel;
};

static const int MAX_MAPPER_RULES = 8;
MidiMapperRule midiMapperRules[MAX_MAPPER_RULES];
int midiMapperRuleCount = 0;
bool midiMapperBypass = false;

// ---------------------------------------------------------------
// Section 2: helper functions copied verbatim from the .ino
// ---------------------------------------------------------------

int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

const char* getMidiKindLabel(MidiMessageKind kind) {
  static const char* labels[MIDI_KIND_COUNT] = {
    "NoteOff", "NoteOn", "KeyPrs", "PrgChg", "CtrlChg", "ChPrs", "Bend",
    "SysEx", "MTC", "SongPos", "SongSel", "TuneReq",
    "Clock", "Start", "Cont", "Stop", "ActSn", "Reset"
  };
  if (kind < 0 || kind >= MIDI_KIND_COUNT) return "Unknown";
  return labels[kind];
}

bool midiKindHasChannel(MidiMessageKind kind) {
  return kind <= MIDI_KIND_PITCH_BEND;
}

uint8_t getMidiStatusForKind(MidiMessageKind kind, uint8_t channel) {
  switch (kind) {
    case MIDI_KIND_NOTE_OFF:         return 0x80 | (channel & 0x0F);
    case MIDI_KIND_NOTE_ON:          return 0x90 | (channel & 0x0F);
    case MIDI_KIND_KEY_PRESSURE:     return 0xA0 | (channel & 0x0F);
    case MIDI_KIND_CONTROL_CHANGE:   return 0xB0 | (channel & 0x0F);
    case MIDI_KIND_PROGRAM_CHANGE:   return 0xC0 | (channel & 0x0F);
    case MIDI_KIND_CHANNEL_PRESSURE: return 0xD0 | (channel & 0x0F);
    case MIDI_KIND_PITCH_BEND:       return 0xE0 | (channel & 0x0F);
    case MIDI_KIND_SYSTEM_EXCLUSIVE: return 0xF0;
    case MIDI_KIND_MIDI_TIME_CODE:   return 0xF1;
    case MIDI_KIND_SONG_POSITION:    return 0xF2;
    case MIDI_KIND_SONG_SELECT:      return 0xF3;
    case MIDI_KIND_TUNE_REQUEST:     return 0xF6;
    case MIDI_KIND_CLOCK:            return 0xF8;
    case MIDI_KIND_START:            return 0xFA;
    case MIDI_KIND_CONTINUE:         return 0xFB;
    case MIDI_KIND_STOP:             return 0xFC;
    case MIDI_KIND_ACTIVE_SENSE:     return 0xFE;
    case MIDI_KIND_SYSTEM_RESET:     return 0xFF;
    default:                         return 0x00;
  }
}

uint8_t getMidiMessageLengthForKind(MidiMessageKind kind) {
  switch (kind) {
    case MIDI_KIND_NOTE_OFF:
    case MIDI_KIND_NOTE_ON:
    case MIDI_KIND_KEY_PRESSURE:
    case MIDI_KIND_CONTROL_CHANGE:
    case MIDI_KIND_PITCH_BEND:
    case MIDI_KIND_SONG_POSITION:
      return 3;
    case MIDI_KIND_PROGRAM_CHANGE:
    case MIDI_KIND_CHANNEL_PRESSURE:
    case MIDI_KIND_MIDI_TIME_CODE:
    case MIDI_KIND_SONG_SELECT:
      return 2;
    default:
      return 1;
  }
}

bool midiKindSupportsData1(MidiMessageKind kind) {
  switch (kind) {
    case MIDI_KIND_NOTE_OFF:
    case MIDI_KIND_NOTE_ON:
    case MIDI_KIND_KEY_PRESSURE:
    case MIDI_KIND_CONTROL_CHANGE:
    case MIDI_KIND_PROGRAM_CHANGE:
    case MIDI_KIND_MIDI_TIME_CODE:
    case MIDI_KIND_SONG_SELECT:
      return true;
    default:
      return false;
  }
}

int getMidiValueMax(MidiMessageKind kind) {
  switch (kind) {
    case MIDI_KIND_PITCH_BEND:
    case MIDI_KIND_SONG_POSITION:
      return 16383;
    default:
      return 127;
  }
}

int getMidiPrimaryData(const MidiMessage& msg) {
  if (msg.length < 2) return 0;
  return msg.bytes[1];
}

int getMidiValueData(const MidiMessage& msg) {
  if (msg.kind == MIDI_KIND_PITCH_BEND || msg.kind == MIDI_KIND_SONG_POSITION) {
    if (msg.length < 3) return 0;
    return (msg.bytes[1] & 0x7F) | ((msg.bytes[2] & 0x7F) << 7);
  }
  if (msg.length >= 3) return msg.bytes[2];
  if (msg.length >= 2) return msg.bytes[1];
  return 0;
}

int mapMidiValueRange(int value, int srcMin, int srcMax, int dstMin, int dstMax) {
  srcMin = clampInt(srcMin, 0, 16383);
  srcMax = clampInt(srcMax, 0, 16383);
  dstMin = clampInt(dstMin, 0, 16383);
  dstMax = clampInt(dstMax, 0, 16383);

  if (srcMax < srcMin) srcMax = srcMin;
  if (dstMax < dstMin) dstMax = dstMin;

  value = clampInt(value, srcMin, srcMax);
  if (srcMax == srcMin) return dstMin;

  long num = (long)(value - srcMin) * (dstMax - dstMin);
  long den = (srcMax - srcMin);
  return dstMin + (int)(num / den);
}

bool midiMapperRuleMatches(const MidiMapperRule& rule, const MidiMessage& msg) {
  if (!rule.enabled) return false;
  if (rule.srcKind != msg.kind) return false;
  if (rule.srcChannel >= 0) {
    if (!msg.hasChannel) return false;
    if (rule.srcChannel != msg.channel) return false;
  }
  if (rule.srcData1 >= 0 && getMidiPrimaryData(msg) != rule.srcData1) return false;

  int value = getMidiValueData(msg);
  if (value < rule.srcMin || value > rule.srcMax) return false;
  return true;
}

MidiMessage buildMappedMidiMessage(const MidiMapperRule& rule, const MidiMessage& srcMsg) {
  MidiMessage dstMsg = srcMsg;
  dstMsg.kind = rule.dstKind;
  dstMsg.hasChannel = midiKindHasChannel(rule.dstKind);
  dstMsg.length = getMidiMessageLengthForKind(rule.dstKind);

  uint8_t outChannel = 0;
  if (dstMsg.hasChannel) {
    if (rule.dstChannel >= 0) outChannel = rule.dstChannel;
    else if (srcMsg.hasChannel) outChannel = srcMsg.channel;
  }
  dstMsg.channel = dstMsg.hasChannel ? outChannel : -1;
  dstMsg.bytes[0] = getMidiStatusForKind(rule.dstKind, outChannel);

  int srcPrimary = getMidiPrimaryData(srcMsg);
  int srcValue = getMidiValueData(srcMsg);
  int mappedValue = mapMidiValueRange(srcValue, rule.srcMin, rule.srcMax, rule.dstMin, rule.dstMax);
  int primaryValue = (rule.dstData1 >= 0) ? rule.dstData1 : srcPrimary;

  if (dstMsg.length == 2) {
    if (midiKindSupportsData1(rule.dstKind)) {
      dstMsg.bytes[1] = clampInt(primaryValue, 0, 127);
    } else {
      dstMsg.bytes[1] = clampInt(mappedValue, 0, 127);
    }
  } else if (dstMsg.length == 3) {
    if (rule.dstKind == MIDI_KIND_PITCH_BEND || rule.dstKind == MIDI_KIND_SONG_POSITION) {
      int v = clampInt(mappedValue, 0, 16383);
      dstMsg.bytes[1] = v & 0x7F;
      dstMsg.bytes[2] = (v >> 7) & 0x7F;
    } else {
      dstMsg.bytes[1] = clampInt(primaryValue, 0, 127);
      dstMsg.bytes[2] = clampInt(mappedValue, 0, 127);
    }
  }

  return dstMsg;
}

MidiMessage applyMidiMapper(const MidiMessage& srcMsg) {
  if (midiMapperBypass) return srcMsg;
  for (int i = 0; i < midiMapperRuleCount; i++) {
    if (midiMapperRuleMatches(midiMapperRules[i], srcMsg)) {
      return buildMappedMidiMessage(midiMapperRules[i], srcMsg);
    }
  }
  return srcMsg;
}

void normalizeMapperRule(MidiMapperRule& rule) {
  int srcMaxValue = getMidiValueMax(rule.srcKind);
  int dstMaxValue = getMidiValueMax(rule.dstKind);

  if (!midiKindSupportsData1(rule.srcKind)) rule.srcData1 = -1;
  if (!midiKindSupportsData1(rule.dstKind)) rule.dstData1 = -1;

  rule.srcMin = clampInt(rule.srcMin, 0, srcMaxValue);
  rule.srcMax = clampInt(rule.srcMax, 0, srcMaxValue);
  rule.dstMin = clampInt(rule.dstMin, 0, dstMaxValue);
  rule.dstMax = clampInt(rule.dstMax, 0, dstMaxValue);

  if (rule.srcMin > rule.srcMax) rule.srcMin = rule.srcMax;
  if (rule.dstMin > rule.dstMax) rule.dstMin = rule.dstMax;
}

// ---------------------------------------------------------------
// Section 3: test harness helpers
// ---------------------------------------------------------------

// Builds a MidiMessage the same way the runtime parser would, given a
// "logical" kind, channel (0..15 or -1 for non-channel kinds), and up to
// two data words. data2 is ignored for 2-byte kinds; for 14-bit kinds
// (PitchBend / SongPos) data1 carries the full 14-bit value.
MidiMessage makeMsg(MidiMessageKind kind, int channel, int data1, int data2 = 0) {
  MidiMessage m;
  std::memset(&m, 0, sizeof(m));
  m.kind = kind;
  m.length = getMidiMessageLengthForKind(kind);
  m.hasChannel = midiKindHasChannel(kind);
  m.channel = m.hasChannel ? (int8_t)channel : -1;

  uint8_t status = getMidiStatusForKind(kind, m.hasChannel ? channel : 0);
  m.bytes[0] = status;
  if (m.length == 2) {
    m.bytes[1] = (uint8_t)clampInt(data1, 0, 127);
  } else if (m.length == 3) {
    if (kind == MIDI_KIND_PITCH_BEND || kind == MIDI_KIND_SONG_POSITION) {
      int v = clampInt(data1, 0, 16383);
      m.bytes[1] = v & 0x7F;
      m.bytes[2] = (v >> 7) & 0x7F;
    } else {
      m.bytes[1] = (uint8_t)clampInt(data1, 0, 127);
      m.bytes[2] = (uint8_t)clampInt(data2, 0, 127);
    }
  }
  return m;
}

void formatMsg(const MidiMessage& m, char* out, size_t outSize) {
  char ch[8];
  if (m.hasChannel) std::snprintf(ch, sizeof(ch), "Ch%02d", m.channel + 1);
  else              std::snprintf(ch, sizeof(ch), "----");

  if (m.length == 1) {
    std::snprintf(out, outSize, "%-7s %s [%02X]",
      getMidiKindLabel(m.kind), ch, m.bytes[0]);
  } else if (m.length == 2) {
    std::snprintf(out, outSize, "%-7s %s d1=%-3d [%02X %02X]",
      getMidiKindLabel(m.kind), ch, m.bytes[1], m.bytes[0], m.bytes[1]);
  } else {
    int v = (m.kind == MIDI_KIND_PITCH_BEND || m.kind == MIDI_KIND_SONG_POSITION)
            ? ((m.bytes[1] & 0x7F) | ((m.bytes[2] & 0x7F) << 7))
            : m.bytes[2];
    std::snprintf(out, outSize, "%-7s %s d1=%-3d v=%-5d [%02X %02X %02X]",
      getMidiKindLabel(m.kind), ch, m.bytes[1], v,
      m.bytes[0], m.bytes[1], m.bytes[2]);
  }
}

bool msgEquals(const MidiMessage& a, const MidiMessage& b) {
  if (a.kind != b.kind || a.length != b.length || a.hasChannel != b.hasChannel) return false;
  if (a.hasChannel && a.channel != b.channel) return false;
  for (int i = 0; i < a.length; i++) if (a.bytes[i] != b.bytes[i]) return false;
  return true;
}

int g_passed = 0;
int g_failed = 0;

void runCase(const char* name,
             const MidiMessage& in,
             const MidiMessage& expected) {
  MidiMessage out = applyMidiMapper(in);
  char inBuf[80], outBuf[80], expBuf[80];
  formatMsg(in,       inBuf,  sizeof(inBuf));
  formatMsg(out,      outBuf, sizeof(outBuf));
  formatMsg(expected, expBuf, sizeof(expBuf));
  bool ok = msgEquals(out, expected);
  std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", name);
  std::printf("        in  : %s\n", inBuf);
  std::printf("        out : %s\n", outBuf);
  if (!ok) std::printf("        want: %s\n", expBuf);
  if (ok) g_passed++; else g_failed++;
}

// ---------------------------------------------------------------
// Section 4: rule construction helpers
// ---------------------------------------------------------------

void resetRules() {
  midiMapperRuleCount = 0;
  midiMapperBypass = false;
  std::memset(midiMapperRules, 0, sizeof(midiMapperRules));
}

MidiMapperRule& addRule() {
  MidiMapperRule& r = midiMapperRules[midiMapperRuleCount++];
  r.enabled = true;
  r.srcKind = MIDI_KIND_CONTROL_CHANGE;
  r.srcChannel = -1;
  r.srcData1 = -1;
  r.srcMin = 0;
  r.srcMax = 127;
  r.dstKind = MIDI_KIND_CONTROL_CHANGE;
  r.dstChannel = -1;
  r.dstData1 = -1;
  r.dstMin = 0;
  r.dstMax = 127;
  return r;
}

// ---------------------------------------------------------------
// Section 5: test scenarios
// ---------------------------------------------------------------

void test_bypass() {
  std::printf("[1] Bypass passes messages through unchanged\n");
  resetRules();
  midiMapperBypass = true;
  // Add a rule that would otherwise change everything, to prove bypass wins.
  MidiMapperRule& r = addRule();
  r.srcKind = MIDI_KIND_CONTROL_CHANGE;
  r.dstKind = MIDI_KIND_PROGRAM_CHANGE;

  MidiMessage in  = makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 7, 100);
  runCase("CC ch1 d1=7 v=100 -> unchanged", in, in);
}

void test_no_match_passes_through() {
  std::printf("[2] Non-matching rule leaves message unchanged\n");
  resetRules();
  // Rule only matches CC#1 on ch1, send CC#7 on ch1.
  MidiMapperRule& r = addRule();
  r.srcKind    = MIDI_KIND_CONTROL_CHANGE;
  r.srcChannel = 0;
  r.srcData1   = 1;
  r.dstKind    = MIDI_KIND_CONTROL_CHANGE;
  r.dstData1   = 64;

  MidiMessage in = makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 7, 100);
  runCase("CC#7 not matched by CC#1 rule", in, in);
}

void test_cc_kind_change_to_pitchbend() {
  std::printf("[3] Kind change CC -> PitchBend with range scale\n");
  resetRules();
  MidiMapperRule& r = addRule();
  r.srcKind    = MIDI_KIND_CONTROL_CHANGE;
  r.srcChannel = -1;          // ALL channels
  r.srcData1   = 1;           // mod wheel
  r.srcMin     = 0;
  r.srcMax     = 127;
  r.dstKind    = MIDI_KIND_PITCH_BEND;
  r.dstChannel = -1;          // KEEP source channel
  r.dstData1   = -1;
  r.dstMin     = 0;
  r.dstMax     = 16383;

  // value=0 -> bend 0
  runCase("CC#1 v=0 -> Bend 0",
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 2, 1, 0),
    makeMsg(MIDI_KIND_PITCH_BEND,     2, 0));
  // value=127 -> bend 16383 (max)
  runCase("CC#1 v=127 -> Bend 16383",
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 2, 1, 127),
    makeMsg(MIDI_KIND_PITCH_BEND,     2, 16383));
  // value=64 -> bend = 64*(16383-0)/(127-0) = 8256 (integer division)
  runCase("CC#1 v=64 -> Bend 8256",
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 2, 1, 64),
    makeMsg(MIDI_KIND_PITCH_BEND,     2, 8256));
}

void test_channel_remap() {
  std::printf("[4] Channel remap (KEEP vs fixed)\n");
  resetRules();
  MidiMapperRule& r = addRule();
  r.srcKind    = MIDI_KIND_NOTE_ON;
  r.srcChannel = 0;            // only ch1
  r.srcData1   = -1;           // any note
  r.dstKind    = MIDI_KIND_NOTE_ON;
  r.dstChannel = 9;            // remap to ch10 (drum channel)
  r.dstData1   = -1;
  r.dstMin     = 0;            // velocity passthrough
  r.dstMax     = 127;

  runCase("NoteOn ch1 v=100 -> ch10 v=100",
    makeMsg(MIDI_KIND_NOTE_ON, 0, 60, 100),
    makeMsg(MIDI_KIND_NOTE_ON, 9, 60, 100));

  // ch2 should not match (rule restricts to ch1).
  runCase("NoteOn ch2 not matched (rule is ch1 only)",
    makeMsg(MIDI_KIND_NOTE_ON, 1, 60, 100),
    makeMsg(MIDI_KIND_NOTE_ON, 1, 60, 100));
}

void test_first_match_wins() {
  std::printf("[5] First matching rule wins (multiple entries)\n");
  resetRules();

  // Rule 0: CC#7 on any ch -> CC#11 (expression) on any ch
  MidiMapperRule& r0 = addRule();
  r0.srcKind    = MIDI_KIND_CONTROL_CHANGE;
  r0.srcData1   = 7;
  r0.dstKind    = MIDI_KIND_CONTROL_CHANGE;
  r0.dstData1   = 11;

  // Rule 1: CC#7 on any ch -> CC#1 (would shadow if rule 0 matched first).
  MidiMapperRule& r1 = addRule();
  r1.srcKind    = MIDI_KIND_CONTROL_CHANGE;
  r1.srcData1   = 7;
  r1.dstKind    = MIDI_KIND_CONTROL_CHANGE;
  r1.dstData1   = 1;

  // Rule 2: CC#10 -> Bend (different src so this one matches CC#10)
  MidiMapperRule& r2 = addRule();
  r2.srcKind    = MIDI_KIND_CONTROL_CHANGE;
  r2.srcData1   = 10;
  r2.dstKind    = MIDI_KIND_PITCH_BEND;
  r2.dstMin     = 0;
  r2.dstMax     = 16383;

  runCase("CC#7 -> CC#11 (rule 0 wins)",
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 7, 80),
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 11, 80));

  runCase("CC#10 -> Bend (rule 2 matches)",
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 10, 64),
    makeMsg(MIDI_KIND_PITCH_BEND,     0, 8256));
}

void test_disabled_rule_skipped() {
  std::printf("[6] Disabled rule does not match\n");
  resetRules();

  MidiMapperRule& r0 = addRule();
  r0.enabled    = false;            // disabled
  r0.srcKind    = MIDI_KIND_CONTROL_CHANGE;
  r0.srcData1   = 7;
  r0.dstKind    = MIDI_KIND_PROGRAM_CHANGE;
  r0.dstData1   = 5;

  MidiMessage in = makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 7, 50);
  runCase("CC#7 with disabled rule -> unchanged", in, in);
}

void test_src_value_window() {
  std::printf("[7] Source value window (srcMin..srcMax filters matches)\n");
  resetRules();

  MidiMapperRule& r = addRule();
  r.srcKind    = MIDI_KIND_CONTROL_CHANGE;
  r.srcData1   = 64;          // sustain pedal
  r.srcMin     = 64;          // only 'on' half (>= 64)
  r.srcMax     = 127;
  r.dstKind    = MIDI_KIND_NOTE_ON;
  r.dstData1   = 60;          // C4
  r.dstMin     = 64;
  r.dstMax     = 127;

  // value=80: in window, becomes NoteOn C4 vel=80 (linear-mapped 64..127)
  runCase("CC#64 v=80 -> NoteOn C4 v=80",
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 64, 80),
    makeMsg(MIDI_KIND_NOTE_ON,        0, 60, 80));

  // value=0: outside window -> rule skipped, passes through unchanged.
  runCase("CC#64 v=0 outside window -> unchanged",
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 64, 0),
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 64, 0));
}

void test_pitchbend_to_cc() {
  std::printf("[8] PitchBend (14-bit) -> CC (7-bit) downscale\n");
  resetRules();
  MidiMapperRule& r = addRule();
  r.srcKind    = MIDI_KIND_PITCH_BEND;
  r.srcMin     = 0;
  r.srcMax     = 16383;
  r.dstKind    = MIDI_KIND_CONTROL_CHANGE;
  r.dstData1   = 1;
  r.dstMin     = 0;
  r.dstMax     = 127;

  runCase("Bend 0 -> CC#1 v=0",
    makeMsg(MIDI_KIND_PITCH_BEND,     3, 0),
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 3, 1, 0));
  runCase("Bend 16383 -> CC#1 v=127",
    makeMsg(MIDI_KIND_PITCH_BEND,     3, 16383),
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 3, 1, 127));
  runCase("Bend 8192 (center) -> CC#1 v=63",
    makeMsg(MIDI_KIND_PITCH_BEND,     3, 8192),
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 3, 1, 63));
}

void test_cc_to_program_change() {
  std::printf("[9] CC -> ProgramChange (2-byte dst, dstData1 carries program)\n");
  resetRules();
  MidiMapperRule& r = addRule();
  r.srcKind    = MIDI_KIND_CONTROL_CHANGE;
  r.srcData1   = 64;
  r.srcMin     = 64;            // only on press
  r.srcMax     = 127;
  r.dstKind    = MIDI_KIND_PROGRAM_CHANGE;
  r.dstData1   = 24;            // Acoustic Guitar Nylon

  runCase("CC#64 v=100 -> PrgChg #24",
    makeMsg(MIDI_KIND_CONTROL_CHANGE, 0, 64, 100),
    makeMsg(MIDI_KIND_PROGRAM_CHANGE, 0, 24));
}

void test_normalize_clamps() {
  std::printf("[10] normalizeMapperRule clamps out-of-range fields\n");
  MidiMapperRule r{};
  r.enabled = true;
  r.srcKind = MIDI_KIND_CONTROL_CHANGE;
  r.srcData1 = 200;          // valid (>=0); only kind/range checks apply
  r.srcMin = -10;            // -> 0
  r.srcMax = 9999;           // -> 127 (CC max)
  r.dstKind = MIDI_KIND_PITCH_BEND;
  r.dstData1 = 50;           // dst doesn't support data1 -> -1
  r.dstMin = 100;
  r.dstMax = 20000;          // > 16383, still fits in int16_t -> clamped to 16383

  normalizeMapperRule(r);

  bool ok =
    r.srcMin == 0 &&
    r.srcMax == 127 &&
    r.dstMin == 100 &&
    r.dstMax == 16383 &&
    r.dstData1 == -1;
  std::printf("  %s  srcMin=%d srcMax=%d dstMin=%d dstMax=%d dstData1=%d\n",
    ok ? "PASS" : "FAIL",
    r.srcMin, r.srcMax, r.dstMin, r.dstMax, r.dstData1);
  if (ok) g_passed++; else g_failed++;
}

// ---------------------------------------------------------------
// Section 6: performance / latency benchmark
// ---------------------------------------------------------------

void test_performance() {
  std::printf("[11] Performance: latency for applyMidiMapper()\n");

  // Configure 8 rules so we exercise the full per-message scan budget.
  resetRules();
  for (int i = 0; i < MAX_MAPPER_RULES; i++) {
    MidiMapperRule& r = addRule();
    r.srcKind = MIDI_KIND_CONTROL_CHANGE;
    r.srcData1 = (i < MAX_MAPPER_RULES - 1) ? i : 7;  // last rule matches CC#7
    r.dstKind = MIDI_KIND_PITCH_BEND;
    r.dstMin = 0; r.dstMax = 16383;
  }

  // Mix of messages: half match the last rule (worst case scan = full N),
  // half don't match any (also full scan; passes through).
  std::vector<MidiMessage> msgs;
  msgs.reserve(2000);
  for (int v = 0; v < 1000; v++) {
    msgs.push_back(makeMsg(MIDI_KIND_CONTROL_CHANGE, v & 0x0F, 7,  v & 0x7F));   // matches rule 7
    msgs.push_back(makeMsg(MIDI_KIND_CONTROL_CHANGE, v & 0x0F, 99, v & 0x7F));   // no match
  }

  const int reps = 500;  // -> 1,000,000 applyMidiMapper() calls
  volatile uint8_t sink = 0; // prevent optimizer from killing the loop
  auto t0 = std::chrono::steady_clock::now();
  for (int rep = 0; rep < reps; rep++) {
    for (const MidiMessage& m : msgs) {
      MidiMessage out = applyMidiMapper(m);
      sink ^= out.bytes[0] ^ out.bytes[1] ^ out.bytes[2];
    }
  }
  auto t1 = std::chrono::steady_clock::now();
  (void)sink;

  double totalNs = std::chrono::duration<double, std::nano>(t1 - t0).count();
  long long calls = (long long)msgs.size() * reps;
  double nsPerCall = totalNs / (double)calls;

  std::printf("        %lld calls, total %.2f ms, %.0f ns/call (host PC)\n",
    calls, totalNs / 1e6, nsPerCall);
  std::printf("        Note: M5Core2 (Xtensa @ 240MHz) is roughly 5-15x slower\n"
         "              than this host. A host time of <100 ns/call still\n"
         "              leaves a comfortable budget at 31250 baud (~960 us\n"
         "              per 3-byte MIDI message on the wire).\n");
  // No PASS/FAIL gate for the perf result; just print the number.
}

// ---------------------------------------------------------------
int main() {
  std::printf("MIDI Mapper standalone test\n");
  std::printf("===========================\n\n");

  test_bypass();                  std::printf("\n");
  test_no_match_passes_through(); std::printf("\n");
  test_cc_kind_change_to_pitchbend(); std::printf("\n");
  test_channel_remap();           std::printf("\n");
  test_first_match_wins();        std::printf("\n");
  test_disabled_rule_skipped();   std::printf("\n");
  test_src_value_window();        std::printf("\n");
  test_pitchbend_to_cc();         std::printf("\n");
  test_cc_to_program_change();    std::printf("\n");
  test_normalize_clamps();        std::printf("\n");
  test_performance();             std::printf("\n");

  std::printf("===========================\n");
  std::printf("Results: %d passed, %d failed\n", g_passed, g_failed);
  return (g_failed == 0) ? 0 : 1;
}
