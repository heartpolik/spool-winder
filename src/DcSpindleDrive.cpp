#include "DcSpindleDrive.h"

void DcSpindleDrive::begin() {
    // arduino-esp32 core 2.x LEDC API: channel-addressed.
    ledcSetup(DC_MOTOR_PWM_CHANNEL, DC_MOTOR_PWM_FREQ_HZ, DC_MOTOR_PWM_RES_BITS);
    ledcAttachPin(DC_MOTOR_PWM_PIN, DC_MOTOR_PWM_CHANNEL);
    ledcWrite(DC_MOTOR_PWM_CHANNEL, 0);

    encoder_.begin();
    lastUpdateMs_ = millis();
}

void DcSpindleDrive::enable() {
    enabled_ = true;
}

void DcSpindleDrive::disable() {
    enabled_ = false;
    duty_ = 0.0f;
    integral_ = 0.0f;
    writeDuty(0.0f);
}

void DcSpindleDrive::writeDuty(float duty) {
    duty = constrain(duty, 0.0f, MAX_DUTY);
    duty_ = duty;
    uint32_t maxCount = (1UL << DC_MOTOR_PWM_RES_BITS) - 1;
    ledcWrite(DC_MOTOR_PWM_CHANNEL, (uint32_t)(duty * maxCount));
}

void DcSpindleDrive::update() {
    encoder_.update();

    unsigned long now = millis();
    float dt = (now - lastUpdateMs_) / 1000.0f;
    if (dt <= 0.0f) return;
    lastUpdateMs_ = now;

    if (!enabled_ || targetRpm_ <= 0.0f) {
        // Coast: no PWM drive, but keep integrator clean for next start.
        integral_ = 0.0f;
        writeDuty(0.0f);
        return;
    }

    float error = targetRpm_ - encoder_.rpm();
    integral_ += error * dt;
    integral_ = constrain(integral_, -50.0f, 50.0f); // anti-windup clamp

    float output = KP * error + KI * integral_;
    writeDuty(constrain(duty_ + output * dt, 0.0f, MAX_DUTY));
}
