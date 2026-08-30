#include "protocol.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// copy the line into a bounded local buffer, uppercased, tabs -> spaces,
// trailing cr/lf stripped. returns false if the line is too long to be legal.
bool normalize(const char* line, char* out, int outLen) {
    int n = 0;
    for (const char* p = line; *p != '\0'; p++) {
        if (*p == '\r' || *p == '\n') break;
        if (n >= outLen - 1) return false;
        char c = *p;
        if (c == '\t') c = ' ';
        out[n++] = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    out[n] = '\0';
    return true;
}

// split into first word + rest. returns pointer to args ("" if none).
const char* splitWord(char* buf, char** word) {
    char* p = buf;
    while (*p == ' ') p++;
    *word = p;
    while (*p != '\0' && *p != ' ') p++;
    if (*p == ' ') {
        *p = '\0';
        p++;
        while (*p == ' ') p++;
    }
    return p;
}

bool parseFloatArg(const char* s, float* out) {
    if (*s == '\0') return false;
    char* end = nullptr;
    float v = std::strtof(s, &end);
    if (end == s) return false;
    while (*end == ' ') end++;
    if (*end != '\0') return false; // trailing junk is a parse error, not ignored
    *out = v;
    return true;
}

}  // namespace

ParsedLine parseLine(const char* line) {
    ParsedLine p;

    char buf[64];
    if (!normalize(line, buf, sizeof(buf))) {
        p.kind = ParsedLine::Kind::Unknown;
        return p;
    }

    char* word = nullptr;
    const char* args = splitWord(buf, &word);
    if (*word == '\0') {
        p.kind = ParsedLine::Kind::Empty;
        return p;
    }

    struct BareCmd { const char* name; Command cmd; };
    // bare state machine commands take no argument; anything after them is
    // tolerated and ignored (so "ARM please" still arms, which helps at 11pm)
    static constexpr BareCmd kBare[] = {
        {"ARM", Command::Arm},   {"DISARM", Command::Disarm},
        {"PRESS", Command::Press}, {"VENT", Command::Vent},
        {"ABORT", Command::Abort}, {"CLEAR", Command::Clear},
    };
    for (const auto& b : kBare) {
        if (std::strcmp(word, b.name) == 0) {
            p.kind = ParsedLine::Kind::SmCommand;
            p.smCmd = b.cmd;
            return p;
        }
    }

    if (std::strcmp(word, "SET") == 0) {
        if (!parseFloatArg(args, &p.arg)) {
            p.kind = ParsedLine::Kind::BadArg;
            return p;
        }
        p.kind = ParsedLine::Kind::SmCommand;
        p.smCmd = Command::Set;
        p.hasArg = true;
        return p;
    }

    if (std::strcmp(word, "RATE") == 0) {
        if (!parseFloatArg(args, &p.arg)) {
            p.kind = ParsedLine::Kind::BadArg;
            return p;
        }
        p.kind = ParsedLine::Kind::Rate;
        p.hasArg = true;
        return p;
    }

    if (std::strcmp(word, "STATUS") == 0) {
        p.kind = ParsedLine::Kind::Status;
        return p;
    }

    if (std::strcmp(word, "INJECT") == 0) {
        // fault names are lowercase in the spec, our buffer is uppercased
        char lower[24];
        int n = 0;
        for (const char* q = args; *q != '\0' && *q != ' ' && n < 23; q++) {
            lower[n++] = static_cast<char>(std::tolower(static_cast<unsigned char>(*q)));
        }
        lower[n] = '\0';
        if (!FaultFlags::fromName(lower, p.fault)) {
            p.kind = ParsedLine::Kind::BadArg;
            return p;
        }
        p.kind = ParsedLine::Kind::Inject;
        return p;
    }

    p.kind = ParsedLine::Kind::Unknown;
    return p;
}

const char* cmdResultText(CmdResult r) {
    switch (r) {
        case CmdResult::Ok:       return "ok";
        case CmdResult::ErrState: return "err state";
        case CmdResult::ErrRange: return "err range";
        case CmdResult::ErrFault: return "err fault";
        default:                  return "err unknown";
    }
}

const char* stateName(State s) {
    switch (s) {
        case State::Idle:       return "IDLE";
        case State::Armed:      return "ARMED";
        case State::Pressurize: return "PRESSURIZE";
        case State::Hold:       return "HOLD";
        case State::Vent:       return "VENT";
        case State::Abort:      return "ABORT";
        case State::Safe:       return "SAFE";
        default:                return "?";
    }
}

int formatTelemetry(char* buf, int bufLen, const TelemetrySnapshot& snap) {
    // build the faults array first: ["overpressure","stale"] or []
    char faults[96];
    int fn = 0;
    faults[fn++] = '[';
    for (uint8_t i = 0; i < static_cast<uint8_t>(Fault::Count); i++) {
        if ((snap.faultMask & (1u << i)) == 0) continue;
        const char* name = FaultFlags::name(static_cast<Fault>(i));
        int need = static_cast<int>(std::strlen(name)) + 3; // quotes + comma
        if (fn + need >= static_cast<int>(sizeof(faults)) - 2) break;
        if (fn > 1) faults[fn++] = ',';
        faults[fn++] = '"';
        std::memcpy(faults + fn, name, std::strlen(name));
        fn += static_cast<int>(std::strlen(name));
        faults[fn++] = '"';
    }
    faults[fn++] = ']';
    faults[fn] = '\0';

    // null-able fields get their own little buffers so the big snprintf stays
    // one readable format string
    char setpoint[16];
    if (snap.hasSetpoint) std::snprintf(setpoint, sizeof(setpoint), "%.2f", snap.setpoint);
    else std::snprintf(setpoint, sizeof(setpoint), "null");

    char degc[16];
    if (snap.tcValid) std::snprintf(degc, sizeof(degc), "%.1f", snap.degC);
    else std::snprintf(degc, sizeof(degc), "null");

    int n = std::snprintf(buf, bufLen,
        "{\"t\":%lu,\"state\":\"%s\",\"psi\":%.2f,\"setpoint\":%s,\"degC\":%s,"
        "\"flow_lpm\":%.2f,\"pump\":%.2f,\"valve\":%.2f,\"faults\":%s}\n",
        static_cast<unsigned long>(snap.tMs), stateName(snap.state), snap.psi,
        setpoint, degc, snap.flowLpm, snap.pumpDuty, snap.valvePos, faults);

    if (n < 0 || n >= bufLen) return 0; // truncated json is worse than none
    return n;
}
