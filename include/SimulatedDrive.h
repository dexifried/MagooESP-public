#pragma once

// A visual simulation driven by the throttle request, not measured speed or RPM.
struct SimulatedDriveReading { float speed; float rpm; char gear; };

class SimulatedDrive {
 public:
  SimulatedDriveReading update(char command, float targetSpeed, float dt) {
    if (dt < 0) dt=0;
    if (dt > 0.2f) dt=0.2f;
    if (targetSpeed < 0) targetSpeed=0;
    if (targetSpeed > 100) targetSpeed=100;
    if (command=='N') targetSpeed=0;
    const float maxChange=(targetSpeed>speed_ ? 30.0f : 50.0f)*dt;
    if (targetSpeed>speed_+maxChange) speed_+=maxChange;
    else if (targetSpeed<speed_-maxChange) speed_-=maxChange;
    else speed_=targetSpeed;

    if (command=='D') {
      if (speed_<0.5f) gear_=1;
      const float redlineSpeed=25.0f*gear_;
      if (gear_<4 && speed_>=redlineSpeed) {
        shiftHold_+=dt;
        // Show the redline briefly before the next gear and RPM drop.
        if (shiftHold_<0.2f) return {speed_,8000.0f,char('0'+gear_)};
        ++gear_; shiftHold_=0;
      } else shiftHold_=0;
      const float revs=speed_/(25.0f*gear_);
      return {speed_,650.0f+7350.0f*(revs<1.0f?revs:1.0f),char('0'+gear_)};
    }
    gear_=1; shiftHold_=0;
    if (command=='R') return {speed_,650.0f+7350.0f*speed_/100.0f,'R'};
    if (command=='N') return {speed_,0,'N'};
    return {speed_,speed_*80.0f,'-'};
  }
 private:
  float speed_=0;
  float shiftHold_=0;
  unsigned gear_=1;
};
