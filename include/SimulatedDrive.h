#pragma once

// A visual simulation driven by the throttle request, not measured speed or RPM.
struct SimulatedDriveReading { float speed; float rpm; char gear; };
constexpr float SIMULATED_TOP_SPEED_KPH = 10.4f;

// variation is a smooth signal between -1 and 1 from the dashboard loop.
inline float simulatedThrottleTarget(float throttleSpeed, float variation) {
  return throttleSpeed * (1.025f + 0.015f * variation);
}

class SimulatedDrive {
 public:
  SimulatedDriveReading update(char command, float targetSpeed, float dt) {
    if (dt < 0) dt=0;
    if (dt > 0.2f) dt=0.2f;
    if (targetSpeed < 0) targetSpeed=0;
    if (targetSpeed > SIMULATED_TOP_SPEED_KPH) targetSpeed=SIMULATED_TOP_SPEED_KPH;
    if (command=='N') targetSpeed=0;
    const float previousSpeed=speed_;
    // Low revs pull more gently; the simulated drivetrain pauses during an upshift.
    const float acceleration=command=='D' ? 3.0f*(0.65f+0.35f*clamp01(rpm_/8000.0f)) : 3.0f;
    const bool shifting=upshiftWaiting_ && command=='D' && targetSpeed>=speed_;
    const float maxChange=(targetSpeed>speed_ ? (shifting ? 0.0f : acceleration) : 5.0f)*dt;
    if (targetSpeed>speed_+maxChange) speed_+=maxChange;
    else if (targetSpeed<speed_-maxChange) speed_-=maxChange;
    else speed_=targetSpeed;

    if (command=='D' && !forwardActive_) {
      forwardActive_=true;
      rpm_=650.0f; phase_=1; phaseStartSpeed_=previousSpeed; phaseStartRpm_=rpm_;
    }
    // A released throttle coasts down through the gears before showing N.
    if (command=='D' || (command=='N' && forwardActive_ && (speed_>0.05f || gear_>1))) {
      const int wantedPhase=targetSpeed>previousSpeed+0.005f ? 1 :
          (targetSpeed<previousSpeed-0.005f ? -1 : phase_);
      if (wantedPhase!=phase_) {
        phase_=wantedPhase; phaseStartSpeed_=previousSpeed; phaseStartRpm_=rpm_;
      }
      if (gear_>1 && speed_<=(SIMULATED_TOP_SPEED_KPH/5.0f)*0.8f*(gear_-1)) {
        rpm_=650.0f;
        if (!downshiftWaiting_) {
          downshiftWaiting_=true; downshiftHold_=0;
          return {speed_,rpm_,char('0'+gear_)};
        }
        downshiftHold_+=dt;
        if (downshiftHold_<0.1f) return {speed_,rpm_,char('0'+gear_)};
        --gear_;
        shiftHold_=downshiftHold_=0; upshiftWaiting_=downshiftWaiting_=false;
        phase_=-1; phaseStartSpeed_=speed_; phaseStartRpm_=rpm_=8000.0f;
        return {speed_,rpm_,char('0'+gear_)};
      } else { downshiftHold_=0; downshiftWaiting_=false; }
      const float redlineSpeed=(SIMULATED_TOP_SPEED_KPH/5.0f)*gear_;
      if (command=='D' && gear_<5 && speed_>=redlineSpeed && targetSpeed>=speed_) {
        if (!upshiftWaiting_) {
          upshiftWaiting_=true; shiftHold_=0; rpm_=8000.0f;
          return {speed_,rpm_,char('0'+gear_)};
        }
        shiftHold_+=dt;
        // Show the redline briefly before the next gear and RPM drop.
        if (shiftHold_<0.2f) { rpm_=8000.0f; return {speed_,rpm_,char('0'+gear_)}; }
        ++gear_; shiftHold_=0; upshiftWaiting_=false;
        phase_=1; phaseStartSpeed_=speed_; phaseStartRpm_=rpm_=1000.0f;
        return {speed_,rpm_,char('0'+gear_)};
      } else { shiftHold_=0; upshiftWaiting_=false; }
      if (phase_>0) {
        const float span=redlineSpeed-phaseStartSpeed_;
        const float progress=clamp01(span>0.001f ? (speed_-phaseStartSpeed_)/span : 1.0f);
        const float eased=1.0f-(1.0f-progress)*(1.0f-progress);
        rpm_=phaseStartRpm_+(8000.0f-phaseStartRpm_)*eased;
      } else {
        const float downshiftSpeed=gear_>1 ? (SIMULATED_TOP_SPEED_KPH/5.0f)*0.8f*(gear_-1) : 0.0f;
        const float span=phaseStartSpeed_-downshiftSpeed;
        const float progress=clamp01(span>0.001f ? (speed_-downshiftSpeed)/span : 0.0f);
        rpm_=650.0f+(phaseStartRpm_-650.0f)*progress;
      }
      return {speed_,rpm_,char('0'+gear_)};
    }
    gear_=1; shiftHold_=downshiftHold_=0; upshiftWaiting_=downshiftWaiting_=false; forwardActive_=false; rpm_=650.0f; phase_=1;
    if (command=='R') return {speed_,650.0f+7350.0f*speed_/SIMULATED_TOP_SPEED_KPH,'R'};
    if (command=='N') return {speed_,0,'N'};
    return {speed_,speed_*(8000.0f/SIMULATED_TOP_SPEED_KPH),'-'};
  }
 private:
  static float clamp01(float value) { return value<0 ? 0 : (value>1 ? 1 : value); }
  float speed_=0;
  float shiftHold_=0, downshiftHold_=0;
  float rpm_=650, phaseStartSpeed_=0, phaseStartRpm_=650;
  unsigned gear_=1;
  int phase_=1;
  bool upshiftWaiting_=false, downshiftWaiting_=false;
  bool forwardActive_=false;
};
