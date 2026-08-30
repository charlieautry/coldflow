#include "rig.h"

Rig::Rig(Hal& hal, const RigConfig& cfg)
    : hal_(hal), cfg_(cfg), pid_(cfg.pid), rateHz_(cfg.defaultRateHz) {
    const uint32_t now = hal_.millis();
    lastTickMs_ = now;
    lastFreshMs_ = now; // give the ADC one stale window of grace after boot
    lastEmitMs_ = now;
    applyOutputs(); // drive the safe outputs immediately, before any tick
}

// The state machine wants digested booleans, not raw sensor soup. These are
// the LIVE conditions, deliberately not the latched flags: the latched fault
// (in faults_) is for telemetry and CLEAR gating, while the state machine
// reacts to what is true right now. That split is what lets ABORT roll into
// SAFE after the chamber empties and the sensor recovers, while the fault
// stays latched and re-arming stays blocked until an explicit CLEAR.
Inputs Rig::buildInputs() const {
    Inputs in;
    in.psi = lastPsi_;
    in.overpressure = lastPsi_ >= cfg_.abortPsi;
    in.loopFault = loopFaultLive_;
    in.stale = staleLive_;
    return in;
}

void Rig::handleLine(const char* line) {
    const uint32_t now = hal_.millis();
    const ParsedLine p = parseLine(line);

    switch (p.kind) {
        case ParsedLine::Kind::Empty:
            return; // blank lines don't even earn an err

        case ParsedLine::Kind::Unknown:
            hal_.writeLine("err unknown");
            return;

        case ParsedLine::Kind::BadArg:
            hal_.writeLine("err arg");
            return;

        case ParsedLine::Kind::Status:
            emitTelemetry(now); // one frame right now, regardless of RATE
            hal_.writeLine("ok");
            return;

        case ParsedLine::Kind::Rate:
            if (p.arg < 0.0f || p.arg > cfg_.maxRateHz) {
                hal_.writeLine("err range");
                return;
            }
            rateHz_ = p.arg;
            hal_.writeLine("ok");
            return;

        case ParsedLine::Kind::Inject:
            // the test hook: latch the fault exactly as if its trigger fired,
            // including the redline abort every aborting fault causes
            if (p.fault == Fault::TcOpen) {
                faults_.set(Fault::TcOpen, true); // self-clears when the sensor reads healthy
            } else {
                faults_.latch(p.fault);
                forceAbortFromFault();
            }
            hal_.writeLine("ok");
            return;

        case ParsedLine::Kind::SmCommand:
            break; // handled below
    }

    const CmdResult r = sm_.handleCommand(p.smCmd, p.arg, buildInputs(), now);
    if (r == CmdResult::Ok) {
        if (p.smCmd == Command::Abort) {
            faults_.latch(Fault::AbortCmd);
        } else if (p.smCmd == Command::Clear) {
            faults_.clearAll(); // SAFE + empty chamber verified by the state machine
        } else if (p.smCmd == Command::Press) {
            pid_.reset(); // fresh loop, no leftover windup from the last run
        }
    }
    applyOutputs();
    hal_.writeLine(cmdResultText(r));
}

void Rig::tick() {
    const uint32_t now = hal_.millis();
    const float dtS = static_cast<float>(now - lastTickMs_) / 1000.0f;
    lastTickMs_ = now;

    // 1. raw hardware picture
    const RawSensors raw = hal_.readSensors();
    if (raw.pressureFresh) lastFreshMs_ = now;

    // 2. engineering units + live fault conditions
    const PressureReading pr = convertPressure(raw.pressureCounts, cfg_.cal);
    const TcReading tc = convertThermocouple(raw.tcRaw);
    lastPsi_ = pr.psi;
    lastDegC_ = tc.degC;
    tcValid_ = !tc.open;
    lastFlowLpm_ = flowLpmFromPeriodUs(raw.flowPeriodUs, cfg_.cal);
    loopFaultLive_ = pr.loopLow || pr.loopHigh;
    staleLive_ = isStale(lastFreshMs_, now, cfg_.cal);

    // 3. latch what deserves latching (tc_open self-clears, everything else
    // sticks until CLEAR per the fault table)
    if (lastPsi_ >= cfg_.abortPsi) faults_.latch(Fault::Overpressure);
    if (pr.loopLow) faults_.latch(Fault::LoopLow);
    if (pr.loopHigh) faults_.latch(Fault::LoopHigh);
    if (staleLive_) faults_.latch(Fault::Stale);
    faults_.set(Fault::TcOpen, tc.open);

    // 4. let the state machine react (redlines, dwell timers, auto rows)
    sm_.tick(buildInputs(), now);

    // 5. run the pressure loop while the state machine says the valve is PID's
    if (sm_.outputs().valve == Outputs::ValveMode::Pid) {
        if (prevState_ != State::Pressurize && prevState_ != State::Hold) {
            pid_.reset(); // covers the auto entry paths too
        }
        pid_.update(sm_.setpoint(), lastPsi_, dtS);
    }
    prevState_ = sm_.state();

    // 6. push outputs and stream telemetry
    applyOutputs();
    if (rateHz_ > 0.0f) {
        const uint32_t periodMs = static_cast<uint32_t>(1000.0f / rateHz_);
        if (now - lastEmitMs_ >= periodMs) emitTelemetry(now);
    }
}

void Rig::applyOutputs() {
    const Outputs out = sm_.outputs();

    pumpCmd_ = out.pumpOn ? 1.0f : 0.0f;

    switch (out.valve) {
        case Outputs::ValveMode::Closed:
            valveCmd_ = 0.0f;
            break;
        case Outputs::ValveMode::Open:
            valveCmd_ = 1.0f;
            break;
        case Outputs::ValveMode::Pid:
            // reverse acting: the needle valve bleeds pressure off, so when
            // the PID wants MORE pressure (bigger output) the valve should be
            // MORE closed. the sign flip lives here, at the plumbing, so the
            // PID itself stays a plain vanilla controller.
            valveCmd_ = 1.0f - pid_.output();
            break;
    }

    hal_.setPumpDuty(pumpCmd_);
    hal_.setVentEnergized(!out.ventOpen); // NO solenoid: energize to close
    hal_.setValvePos(valveCmd_);
}

void Rig::emitTelemetry(uint32_t nowMs) {
    TelemetrySnapshot snap;
    snap.tMs = nowMs;
    snap.state = sm_.state();
    snap.psi = lastPsi_;
    snap.setpoint = sm_.setpoint();
    snap.hasSetpoint = sm_.hasSetpoint();
    snap.degC = lastDegC_;
    snap.tcValid = tcValid_;
    snap.flowLpm = lastFlowLpm_;
    snap.pumpDuty = pumpCmd_;
    snap.valvePos = valveCmd_;
    snap.faultMask = faults_.mask();

    char buf[192];
    if (formatTelemetry(buf, sizeof(buf), snap) > 0) {
        hal_.writeLine(buf);
        lastEmitMs_ = nowMs;
    }
}

void Rig::forceAbortFromFault() {
    // injected aborting faults ride the same path a real redline does: the
    // state machine's abort command entry is unconditional and latches its
    // internal fault, so re-arming stays blocked until CLEAR
    sm_.handleCommand(Command::Abort, 0.0f, buildInputs(), hal_.millis());
    applyOutputs();
}
