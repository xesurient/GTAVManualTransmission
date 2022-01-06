#include "WheelInput.h"

#include "script.h"
#include "VehicleConfig.h"
#include "SteeringAnim.h"
#include "ScriptSettings.hpp"
#include "VehicleData.hpp"
#include "Input/CarControls.hpp"

#include "Util/ScriptUtils.h"
#include "Util/MathExt.h"
#include "Util/MiscEnums.h"
#include "Util/UIUtils.h"

#include "Memory/VehicleExtensions.hpp"
#include "Memory/Offsets.hpp"
#include "Memory/VehicleBone.h"

#include <inc/enums.h>
#include <inc/natives.h>
#include <inc/types.h>

#include <MiniPID/MiniPID.h>
#include <fmt/format.h>
#include <algorithm>

using VExt = VehicleExtensions;

extern Vehicle g_playerVehicle;
extern Ped g_playerPed;
extern ScriptSettings g_settings;
extern VehicleConfig* g_activeConfig;
extern VehicleData g_vehData;
extern CarControls g_controls;

extern VehiclePeripherals g_peripherals;
extern VehicleGearboxStates g_gearStates;
extern WheelPatchStates g_wheelPatchStates;

namespace {
    MiniPID pid(1.0, 0.0, 0.0);
}

namespace WheelInput {
    float lastConstantForce = 0.0f;

    // Use alternative throttle - brake - 
    void SetControlADZAlt(eControl control, float value, float adz, bool alt) {
        Controls::SetControlADZ(control, value, adz);
        if (alt) {
            eControl newControl;
            switch (control) {
                case ControlVehicleAccelerate:
                    newControl = (ControlVehicleSubThrottleUp);
                    break;
                case ControlVehicleBrake:
                    newControl = (ControlVehicleSubThrottleDown);
                    break;
                case ControlVehicleMoveLeftRight:
                    newControl = (ControlVehicleSubTurnLeftRight);
                    break;
                default:
                    newControl = (control);
                    break;
            }

            Controls::SetControlADZ(newControl, value, adz);
        }
    }

    float GetFFBConstantForce() {
        return lastConstantForce / 10000.0f;
    }

    bool toggledLowBeamsForFlash = false;
}

// Check for "alternative" movement methods for cars.
// Applies to:
//   - Amphibious when wet
//   - Flying when in-air
bool hasAltInputs(Vehicle vehicle) {
    Hash vehicleModel = ENTITY::GET_ENTITY_MODEL(vehicle);

    bool allWheelsOnGround = VEHICLE::IS_VEHICLE_ON_ALL_WHEELS(vehicle);
    float submergeLevel = ENTITY::GET_ENTITY_SUBMERGED_LEVEL(vehicle);
    // 5: Stromberg
    // 6: Amphibious cars / APC
    // 7: Amphibious bike
    int modelType = VExt::GetModelType(vehicle);
    bool isFrog = modelType == 5 || modelType == 6 || modelType == 7;

    float hoverRatio = VExt::GetHoverTransformRatio(vehicle);

    bool altInput;

    // Wet amphibious vehicles
    if (isFrog && submergeLevel > 0.0f) {
        altInput = true;
    }
    // Hovering
    else if (hoverRatio > 0.0f) {
        altInput = true;
    }
    // Everything else
    else {
        altInput = false;
    }

    return altInput;
}

///////////////////////////////////////////////////////////////////////////////
//                   Mod functions: Reverse/Pedal handling
///////////////////////////////////////////////////////////////////////////////

// Forward gear: Throttle accelerates, Brake brakes (exclusive)
// Reverse gear: Throttle reverses, Brake brakes (exclusive)
void WheelInput::HandlePedals(float wheelThrottleVal, float wheelBrakeVal) {
    bool altInput = hasAltInputs(g_playerVehicle);

    wheelThrottleVal = pow(wheelThrottleVal, g_settings.Wheel.Throttle.Gamma);
    wheelBrakeVal = pow(wheelBrakeVal, g_settings.Wheel.Brake.Gamma);

    float speedThreshold = 0.5f;
    const float reverseThreshold = 2.0f;

    if (g_vehData.mGearCurr > 0) {
        // Going forward
        if (g_vehData.mVelocity.y >= speedThreshold) {
            //UI::ShowText(0.3, 0.0, 1.0, "We are going forward");
            // Throttle Pedal normal
            if (wheelThrottleVal > 0.01f) {
                Controls::SetControlADZ(ControlVehicleAccelerate, wheelThrottleVal, g_settings.Wheel.Throttle.AntiDeadZone);
            }
            // Brake Pedal normal
            if (wheelBrakeVal > 0.01f) {
                Controls::SetControlADZ(ControlVehicleBrake, wheelBrakeVal, g_settings.Wheel.Brake.AntiDeadZone);
            }
        }

        // Standing still
        if (abs(g_vehData.mVelocity.y) < speedThreshold) {
            //UI::ShowText(0.3, 0.0, 1.0, "We are stopped");
            // Throttle Pedal normal
            if (wheelThrottleVal > 0.01f) {
                Controls::SetControlADZ(ControlVehicleAccelerate, wheelThrottleVal, g_settings.Wheel.Throttle.AntiDeadZone);
            }
            // Brake Pedal conditional
            if (wheelBrakeVal > wheelThrottleVal) {
                //UI::ShowText(0.3, 0.0, 1.0, "We are stopped + just brake lights");
                
                if (wheelBrakeVal > 0.01f) {
                    //VExt::SetThrottleP(g_playerVehicle, 0.1f);
                    VExt::SetBrakeP(g_playerVehicle, 1.0f);
                    VEHICLE::SET_VEHICLE_BRAKE_LIGHTS(g_playerVehicle, true);
                }
            }
            else {
                //UI::ShowText(0.3, 0.0, 1.0, "We are stopped + burnout");
                // Brake Pedal normal
                if (wheelBrakeVal > 0.01f) {
                    Controls::SetControlADZ(ControlVehicleBrake, wheelBrakeVal, g_settings.Wheel.Brake.AntiDeadZone);
                }
            }
        }

        // Rolling back
        if (g_vehData.mVelocity.y <= -speedThreshold) {
            bool brakelights = false;
            // Just brake
            if (wheelThrottleVal <= 0.01f && wheelBrakeVal > 0.01f) {
                //UI::ShowText(0.3, 0.0, 1.0, "We should brake");
                //UI::ShowText(0.3, 0.05, 1.0, ("Brake pressure:" + std::to_string(wheelBrakeVal)));
                Controls::SetControlADZ(ControlVehicleAccelerate, wheelBrakeVal, g_settings.Wheel.Brake.AntiDeadZone);
                VExt::SetThrottleP(g_playerVehicle, 0.1f);
                VExt::SetBrakeP(g_playerVehicle, 1.0f);
                brakelights = true;
            }

            if (!g_gearStates.FakeNeutral && wheelThrottleVal > 0.01f && !isClutchPressed()) {
                //UI::ShowText(0.3, 0.0, 1.0, "We should burnout");
                if (g_controls.BrakeVal < 0.1f) {
                    VEHICLE::SET_VEHICLE_BRAKE_LIGHTS(g_playerVehicle, false);
                }
                for (int i = 0; i < g_vehData.mWheelCount; i++) {
                    if (VExt::IsWheelPowered(g_playerVehicle, i)) {
                        VExt::SetWheelBrakePressure(g_playerVehicle, i, 0.0f);
                        VExt::SetWheelPower(g_playerVehicle, i, 2.0f * VExt::GetDriveForce(g_playerVehicle));
                    }
                    else {
                        float handlingBrakeForce = *reinterpret_cast<float*>(g_vehData.mHandlingPtr + hOffsets.fBrakeForce);
                        float inpBrakeForce = handlingBrakeForce * g_controls.BrakeVal;
                        VExt::SetWheelPower(g_playerVehicle, i, 0.0f);
                        VExt::SetWheelBrakePressure(g_playerVehicle, i, inpBrakeForce);
                    }
                }
                fakeRev();
                VExt::SetThrottle(g_playerVehicle, g_controls.ThrottleVal);
                VExt::SetThrottleP(g_playerVehicle, g_controls.ThrottleVal);
                g_wheelPatchStates.InduceBurnout = true;
            }

            if (wheelThrottleVal > 0.01f && (isClutchPressed() || g_gearStates.FakeNeutral)) {
                if (wheelBrakeVal > 0.01f) {
                    //UI::ShowText(0.3, 0.0, 1.0, "We should rev and brake");
                    //UI::ShowText(0.3, 0.05, 1.0, ("Brake pressure:" + std::to_string(wheelBrakeVal)) );
                    Controls::SetControlADZ(ControlVehicleAccelerate, wheelBrakeVal, g_settings.Wheel.Brake.AntiDeadZone);
                    VExt::SetThrottleP(g_playerVehicle, 0.1f);
                    VExt::SetBrakeP(g_playerVehicle, 1.0f);
                    brakelights = true;
                    fakeRev(false, 0);
                }
                else if (g_controls.ClutchVal > 0.9 || g_gearStates.FakeNeutral) {
                    //UI::ShowText(0.3, 0.0, 1.0, "We should rev and do nothing");
                    VExt::SetThrottleP(g_playerVehicle, wheelThrottleVal);
                    fakeRev(false, 0);
                }
                else {
                    //UI::ShowText(0.3, 0.0, 1.0, "We should rev and apply throttle");
                    Controls::SetControlADZ(ControlVehicleAccelerate, wheelThrottleVal, g_settings.Wheel.Throttle.AntiDeadZone);
                    VExt::SetThrottleP(g_playerVehicle, wheelThrottleVal);
                    fakeRev(false, 0);
                }
            }
            VEHICLE::SET_VEHICLE_BRAKE_LIGHTS(g_playerVehicle, brakelights);
        }
        else {
            g_wheelPatchStates.InduceBurnout = false;
        }
    }

    if (g_vehData.mGearCurr == 0) {
        // Enables reverse lights
        VExt::SetThrottleP(g_playerVehicle, -0.1f);

        // We're reversing
        if (g_vehData.mVelocity.y < -speedThreshold) {
            //UI::ShowText(0.3, 0.0, 1.0, "We are reversing");
            // Throttle Pedal Reverse
            if (wheelThrottleVal > 0.01f) {
                Controls::SetControlADZ(ControlVehicleBrake, wheelThrottleVal, g_settings.Wheel.Throttle.AntiDeadZone);
            }
            // Brake Pedal Reverse
            if (wheelBrakeVal > 0.01f) {
                Controls::SetControlADZ(ControlVehicleAccelerate, wheelBrakeVal, g_settings.Wheel.Brake.AntiDeadZone);
                VExt::SetThrottleP(g_playerVehicle, -wheelBrakeVal);
                VExt::SetBrakeP(g_playerVehicle, wheelBrakeVal);
            }
        }

        // Standing still
        if (g_vehData.mVelocity.y < speedThreshold && g_vehData.mVelocity.y >= -speedThreshold) {
            //UI::ShowText(0.3, 0.0, 1.0, "We are stopped");

            if (wheelThrottleVal > 0.01f) {
                Controls::SetControlADZ(ControlVehicleBrake, wheelThrottleVal, g_settings.Wheel.Throttle.AntiDeadZone);
            }

            if (wheelBrakeVal > 0.01f) {
                VExt::SetThrottleP(g_playerVehicle, -wheelBrakeVal);
                VExt::SetBrakeP(g_playerVehicle, 1.0f);
                VEHICLE::SET_VEHICLE_BRAKE_LIGHTS(g_playerVehicle, true);
            }
        }

        // We're rolling forwards
        if (g_vehData.mVelocity.y > speedThreshold) {
            //UI::ShowText(0.3, 0.0, 1.0, "We are rolling forwards");
            //bool brakelights = false;

            if (g_vehData.mVelocity.y > reverseThreshold) {
                if (!isClutchPressed()) {
                    PAD::_SET_CONTROL_NORMAL(0, ControlVehicleHandbrake, 1.0f);
                }
                // Brake Pedal Reverse
                if (wheelBrakeVal > 0.01f) {
                    Controls::SetControlADZ(ControlVehicleBrake, wheelBrakeVal, g_settings.Wheel.Brake.AntiDeadZone);
                    VExt::SetThrottleP(g_playerVehicle, -wheelBrakeVal);
                    VExt::SetBrakeP(g_playerVehicle, wheelBrakeVal);
                }
            }

            //VEHICLE::SET_VEHICLE_BRAKE_LIGHTS(vehicle, brakelights);
        }
    }
}

// Pedals behave like RT/LT
void WheelInput::HandlePedalsArcade(float wheelThrottleVal, float wheelBrakeVal) {
    bool altInput = hasAltInputs(g_playerVehicle);
    wheelThrottleVal = pow(wheelThrottleVal, g_settings.Wheel.Throttle.Gamma);
    wheelBrakeVal = pow(wheelBrakeVal, g_settings.Wheel.Brake.Gamma);

    if (wheelThrottleVal > 0.01f) {
        SetControlADZAlt(ControlVehicleAccelerate, wheelThrottleVal, g_settings.Wheel.Throttle.AntiDeadZone, altInput);
    }
    if (wheelBrakeVal > 0.01f) {
        SetControlADZAlt(ControlVehicleBrake, wheelBrakeVal, g_settings.Wheel.Brake.AntiDeadZone, altInput);
    }
}


///////////////////////////////////////////////////////////////////////////////
//                       Mod functions: Buttons
///////////////////////////////////////////////////////////////////////////////

// Set state, and activate them if hazards are off.
void setBlinkers(bool left, bool right) {
    g_peripherals.BlinkerLeft = left;
    g_peripherals.BlinkerRight = right;

    if (!g_peripherals.BlinkerHazard) {
        VEHICLE::SET_VEHICLE_INDICATOR_LIGHTS(g_playerVehicle, 0, right);
        VEHICLE::SET_VEHICLE_INDICATOR_LIGHTS(g_playerVehicle, 1, left);
    }
}

void checkIndicatorActions() {
    if (g_controls.ButtonJustPressed(CarControls::WheelControlType::IndicatorHazard)) {
        if (!g_peripherals.BlinkerHazard) {
            VEHICLE::SET_VEHICLE_INDICATOR_LIGHTS(g_playerVehicle, 0, true);
            VEHICLE::SET_VEHICLE_INDICATOR_LIGHTS(g_playerVehicle, 1, true);
            g_peripherals.BlinkerHazard = true;
        }
        else {
            VEHICLE::SET_VEHICLE_INDICATOR_LIGHTS(g_playerVehicle, 0, g_peripherals.BlinkerRight);
            VEHICLE::SET_VEHICLE_INDICATOR_LIGHTS(g_playerVehicle, 1, g_peripherals.BlinkerLeft);
            g_peripherals.BlinkerHazard = false;
        }
    }

    if (g_controls.ButtonJustPressed(CarControls::WheelControlType::IndicatorLeft)) {
        if (!g_peripherals.BlinkerLeft) {
            g_peripherals.BlinkerTicks = 1;
            setBlinkers(true, false);
        }
        else {
            g_peripherals.BlinkerTicks = 0;
            setBlinkers(false, false);
        }
    }
    if (g_controls.ButtonJustPressed(CarControls::WheelControlType::IndicatorRight)) {
        if (!g_peripherals.BlinkerRight) {
            g_peripherals.BlinkerTicks = 1;
            setBlinkers(false, true);
        }
        else {
            g_peripherals.BlinkerTicks = 0;
            setBlinkers(false, false);
        }
    }

    float wheelCenterDeviation = (g_controls.SteerVal - 0.5f) / 0.5f;

    if (g_peripherals.BlinkerTicks == 1 && abs(wheelCenterDeviation) > 0.2f) {
        g_peripherals.BlinkerTicks = 2;
    }

    if (g_peripherals.BlinkerTicks == 2 && abs(wheelCenterDeviation) < 0.1f) {
        g_peripherals.BlinkerTicks = 0;
        setBlinkers(false, false);
    }
}

void checkRadioButtons() {
    if (g_controls.ButtonHeld(CarControls::WheelControlType::RadioPrev, 1000) ||
        g_controls.ButtonHeld(CarControls::WheelControlType::RadioNext, 1000)) {
        if (AUDIO::GET_PLAYER_RADIO_STATION_INDEX() != RadioOff) {
            g_peripherals.RadioStationIndex = AUDIO::GET_PLAYER_RADIO_STATION_INDEX();
        }
        AUDIO::SET_VEH_RADIO_STATION(g_playerVehicle, "OFF");
        return;
    }
    if (g_controls.ButtonReleased(CarControls::WheelControlType::RadioNext)) {
        if (AUDIO::GET_PLAYER_RADIO_STATION_INDEX() == RadioOff) {
            AUDIO::SET_RADIO_TO_STATION_INDEX(g_peripherals.RadioStationIndex);
            return;
        }
        AUDIO::_0xFF266D1D0EB1195D(); // Next radio station
    }
    if (g_controls.ButtonReleased(CarControls::WheelControlType::RadioPrev)) {
        if (AUDIO::GET_PLAYER_RADIO_STATION_INDEX() == RadioOff) {
            AUDIO::SET_RADIO_TO_STATION_INDEX(g_peripherals.RadioStationIndex);
            return;
        }
        AUDIO::_0xDD6BCF9E94425DF9(); // Prev radio station
    }
}

void checkCameraButtons() {
    if (g_controls.ButtonIn(CarControls::WheelControlType::LookBack)) {
        PAD::_SET_CONTROL_NORMAL(0, ControlVehicleLookBehind, 1.0f);
    }

    // who was first?
    if (g_controls.ButtonIn(CarControls::WheelControlType::LookRight) &&
        g_controls.ButtonJustPressed(CarControls::WheelControlType::LookLeft)) {
        g_peripherals.LookBackRShoulder = true;
    }

    if (g_controls.ButtonIn(CarControls::WheelControlType::LookLeft) &&
        g_controls.ButtonIn(CarControls::WheelControlType::LookRight)) {
        CAM::SET_GAMEPLAY_CAM_RELATIVE_HEADING(g_peripherals.LookBackRShoulder ? -180.0f : 180.0f);
    }
    else if (g_controls.ButtonIn(CarControls::WheelControlType::LookLeft)) {
        CAM::SET_GAMEPLAY_CAM_RELATIVE_PITCH(0.0f, 1.0f);
        CAM::SET_GAMEPLAY_CAM_RELATIVE_HEADING(90.0f);
    }
    else if (g_controls.ButtonIn(CarControls::WheelControlType::LookRight)) {
        CAM::SET_GAMEPLAY_CAM_RELATIVE_PITCH(0.0f, 1.0f);
        CAM::SET_GAMEPLAY_CAM_RELATIVE_HEADING(-90.0f);
    }
    if (g_controls.ButtonReleased(CarControls::WheelControlType::LookLeft) && !g_controls.ButtonIn(CarControls::WheelControlType::LookRight) ||
        g_controls.ButtonReleased(CarControls::WheelControlType::LookRight) && !g_controls.ButtonIn(CarControls::WheelControlType::LookLeft)) {
        CAM::SET_GAMEPLAY_CAM_RELATIVE_HEADING(0.0f);
    }
    if (g_controls.ButtonReleased(CarControls::WheelControlType::LookLeft) ||
        g_controls.ButtonReleased(CarControls::WheelControlType::LookRight)) {
        g_peripherals.LookBackRShoulder = false;
    }

    if (g_controls.ButtonJustPressed(CarControls::WheelControlType::Camera)) {
        PAD::_SET_CONTROL_NORMAL(0, ControlNextCamera, 1.0f);
    }
}

void checkVehicleInputButtons() {
    if (g_controls.HandbrakeVal > 0.1f) {
        PAD::_SET_CONTROL_NORMAL(0, ControlVehicleHandbrake, g_controls.HandbrakeVal);
    }
    if (g_controls.ButtonIn(CarControls::WheelControlType::Handbrake)) {
        PAD::_SET_CONTROL_NORMAL(0, ControlVehicleHandbrake, 1.0f);
    }
    if (g_controls.ButtonIn(CarControls::WheelControlType::Horn)) {
        PAD::_SET_CONTROL_NORMAL(0, ControlVehicleHorn, 1.0f);
    }
    if (g_controls.ButtonJustPressed(CarControls::WheelControlType::Lights)) {
        PAD::_SET_CONTROL_NORMAL(0, ControlVehicleHeadlight, 1.0f);
    }

    BOOL areLowBeamsOn = FALSE;
    BOOL areHighBeamsOn = FALSE;
    VEHICLE::GET_VEHICLE_LIGHTS_STATE(g_playerVehicle, &areLowBeamsOn, &areHighBeamsOn);
    bool areLowBeamsOn_ = areLowBeamsOn == TRUE;
    bool areHighBeamsOn_ = areHighBeamsOn == TRUE;

    if (g_controls.ButtonJustPressed(CarControls::WheelControlType::LightsLow)) {
        VEHICLE::SET_VEHICLE_LIGHTS(g_playerVehicle, !areLowBeamsOn_ ? 3 : 4);
    }
    if (g_controls.ButtonJustPressed(CarControls::WheelControlType::LightsHigh)) {
        if (areLowBeamsOn_)
            VEHICLE::SET_VEHICLE_FULLBEAM(g_playerVehicle, !areHighBeamsOn_);
    }

    if (g_controls.ButtonJustPressed(CarControls::WheelControlType::LightsHighFlash)) {
        if (!areLowBeamsOn_) {
            VEHICLE::SET_VEHICLE_LIGHTS(g_playerVehicle, 3);
            WheelInput::toggledLowBeamsForFlash = true;
            auto brokenLights = VExt::GetLightsBroken(g_playerVehicle);
            VExt::SetLightsBroken(g_playerVehicle, (uint32_t)(brokenLights | LeftTailLight | RightTailLight));
        }
        VEHICLE::SET_VEHICLE_FULLBEAM(g_playerVehicle, true);
    }
    if (g_controls.ButtonReleased(CarControls::WheelControlType::LightsHighFlash)) {
        if (WheelInput::toggledLowBeamsForFlash) {
            VEHICLE::SET_VEHICLE_LIGHTS(g_playerVehicle, 4);
            WheelInput::toggledLowBeamsForFlash = false;

            auto brokenLights = VExt::GetLightsBroken(g_playerVehicle);
            VExt::SetLightsBroken(g_playerVehicle, VExt::GetLightsBrokenVisual(g_playerVehicle));
        }
        VEHICLE::SET_VEHICLE_FULLBEAM(g_playerVehicle, false);
    }
}

void WheelInput::CheckButtons() {
    if (g_controls.PrevInput != CarControls::Wheel) {
        return;
    }

    checkVehicleInputButtons();
    checkCameraButtons();
    checkRadioButtons();
    checkIndicatorActions();
}

///////////////////////////////////////////////////////////////////////////////
//                    Wheel input and force feedback
///////////////////////////////////////////////////////////////////////////////

void WheelInput::DoSteering() {
    if (g_controls.PrevInput != CarControls::Wheel)
        return;

    float steerMult;
    if (g_settings.ConfigActive())
        steerMult = g_settings.Wheel.Steering.AngleMax / g_settings().Steering.Wheel.SoftLock;
    else if (g_vehData.mClass == VehicleClass::Bike || g_vehData.mClass == VehicleClass::Quad)
        steerMult = g_settings.Wheel.Steering.AngleMax / g_settings.Wheel.Steering.AngleBike;
    else if (g_vehData.mClass == VehicleClass::Car)
        steerMult = g_settings.Wheel.Steering.AngleMax / g_settings.Wheel.Steering.AngleCar;
    else {
        steerMult = g_settings.Wheel.Steering.AngleMax / g_settings.Wheel.Steering.AngleBoat;
    }

    float steerClamp = deg2rad(g_settings.Wheel.Steering.AngleMax) / steerMult;

    float steerValL = map(g_controls.SteerVal, 0.0f, 0.5f, 1.0f, 0.0f);
    float steerValR = map(g_controls.SteerVal, 0.5f, 1.0f, 0.0f, 1.0f);
    float steerValGammaL = pow(steerValL, g_settings.Wheel.Steering.Gamma);
    float steerValGammaR = pow(steerValR, g_settings.Wheel.Steering.Gamma);
    float steerValGamma = g_controls.SteerVal < 0.5f ? -steerValGammaL : steerValGammaR;
    float effSteer = steerMult * steerValGamma;

    bool altInputs = hasAltInputs(g_playerVehicle);

    if (g_vehData.mClass == VehicleClass::Car) {
        VExt::SetSteeringInputAngle(g_playerVehicle, -std::clamp(effSteer, -1.0f, 1.0f));

        if (!VEHICLE::GET_IS_VEHICLE_ENGINE_RUNNING(g_playerVehicle)) {
            float angleOff = -std::clamp(effSteer, -1.0f, 1.0f) * VExt::GetMaxSteeringAngle(g_playerVehicle);
            VExt::SetSteeringAngle(g_playerVehicle, angleOff);
        }

        auto boneIdx = ENTITY::GET_ENTITY_BONE_INDEX_BY_NAME(g_playerVehicle, "steeringwheel");
        if (boneIdx != -1) {
            Vector3 rotAxis{};
            rotAxis.y = 1.0f;
            float rotRad = deg2rad(g_settings.Wheel.Steering.AngleMax) / 2.0f * steerValGamma;

            // clamp
            if (abs(rotRad) > steerClamp / 2.0f) {
                rotRad = std::clamp(rotRad, -steerClamp / 2.0f, steerClamp / 2.0f);
            }
            float rotRadRaw = rotRad;
            // Setting angle using the VExt:: calls above causes the angle to overshoot the "real" coords
            // Not sure if this is the best solution, but hey, it works!
            rotRad -= 2.0f * std::clamp(effSteer, -1.0f, 1.0f) * VExt::GetMaxSteeringAngle(g_playerVehicle);

            Vector3 scale{ 1.0f, 0, 1.0f, 0, 1.0f, 0 };
            if (g_settings.Misc.HideWheelInFPV && CAM::GET_FOLLOW_PED_CAM_VIEW_MODE() == 4) {
                scale.x = 0.0f;
                scale.y = 0.0f;
                scale.z = 0.0f;
            }

            VehicleBones::RotateAxis(g_playerVehicle, boneIdx, rotAxis, rotRad);
            VehicleBones::Scale(g_playerVehicle, boneIdx, scale);
            SteeringAnimation::SetRotation(rotRadRaw);
        }
    }
    if (g_vehData.mClass != VehicleClass::Car || altInputs){
        SetControlADZAlt(ControlVehicleMoveLeftRight, effSteer, g_settings.Wheel.Steering.AntiDeadZone, altInputs);
    }
}

int calculateDamper(float gain, float wheelsOffGroundRatio) {
    // Just to use floats everywhere here
    float damperMax = static_cast<float>(g_settings.Wheel.FFB.DamperMax);
    float damperMin = static_cast<float>(g_settings.Wheel.FFB.DamperMin);
    float damperMinSpeed = static_cast<float>(g_settings.Wheel.FFB.DamperMinSpeed);

    float absVehicleSpeed = abs(ENTITY::GET_ENTITY_SPEED_VECTOR(g_playerVehicle, true).y);
    float damperFactorSpeed = map(absVehicleSpeed, 0.0f, damperMinSpeed, damperMax, damperMin);
    damperFactorSpeed = fmaxf(damperFactorSpeed, damperMin);
    // already clamped on the upper bound by abs(vel) in map()

    float damperForce = damperFactorSpeed;

    damperForce = damperForce * (1.0f - wheelsOffGroundRatio);

    if (g_vehData.mClass == VehicleClass::Car || g_vehData.mClass == VehicleClass::Quad) {
        if (VEHICLE::IS_VEHICLE_TYRE_BURST(g_playerVehicle, 0, true)) {
            damperForce /= 2.0f;
        }
        if (VEHICLE::IS_VEHICLE_TYRE_BURST(g_playerVehicle, 1, true)) {
            damperForce /= 2.0f;
        }
    }
    else if (g_vehData.mClass == VehicleClass::Bike) {
        if (VEHICLE::IS_VEHICLE_TYRE_BURST(g_playerVehicle, 0, true)) {
            damperForce /= 4.0f;
        }
    }

    if (!VEHICLE::GET_IS_VEHICLE_ENGINE_RUNNING(g_playerVehicle)) {
        damperForce *= 2.0f;
    }

    // clamp
    damperForce = fmaxf(damperForce, damperMin / 4.0f);
    damperForce = fminf(damperForce, damperMax * 2.0f);

    damperForce *= gain;

    return static_cast<int>(damperForce);
}

int calculateDetail() {
    // Detail feel / suspension compression based
    float compSpeedTotal = 0.0f;
    auto compSpeed = g_vehData.mSuspensionTravelSpeeds;

    // More than 2 wheels! Trikes should be ok, etc.
    if (compSpeed.size() > 2) {
        // left should pull left, right should pull right
        compSpeedTotal = -compSpeed[0] + compSpeed[1];
    }

    return static_cast<int>(1000.0f * g_settings.Wheel.FFB.DetailMult * compSpeedTotal);
}

void calculateSoftLock(int& totalForce, int& damperForce) {
    float steerMult;

    if (g_settings.ConfigActive())
        steerMult = g_settings.Wheel.Steering.AngleMax / g_settings().Steering.Wheel.SoftLock;
    else if (g_vehData.mClass == VehicleClass::Bike || g_vehData.mClass == VehicleClass::Quad)
        steerMult = g_settings.Wheel.Steering.AngleMax / g_settings.Wheel.Steering.AngleBike;
    else if (g_vehData.mClass == VehicleClass::Car)
        steerMult = g_settings.Wheel.Steering.AngleMax / g_settings.Wheel.Steering.AngleCar;
    else {
        steerMult = g_settings.Wheel.Steering.AngleMax / g_settings.Wheel.Steering.AngleBoat;
    }
    float effSteer = steerMult * 2.0f * (g_controls.SteerVal - 0.5f);
    float steerSpeed = g_controls.GetAxisSpeed(CarControls::WheelAxisType::Steer);
    if (effSteer > 1.0f) {
        if (steerSpeed > 0.0f) {
            damperForce = 10000;
        }
        else {
            damperForce = 0;
        }
        totalForce = (int)map(effSteer, 1.0f, steerMult, (float)totalForce, 40000.0f);
    }
    else if (effSteer < -1.0f) {
        if (steerSpeed < 0.0f) {
            damperForce = 10000;
        }
        else {
            damperForce = 0;
        }
        totalForce = (int)map(effSteer, -1.0f, -steerMult, (float)totalForce, -40000.0f);
    }
}

// TODO: Probably move this to some less-wheel related place.
std::vector<WheelInput::SSlipInfo> WheelInput::CalculateSlipInfo() {
    auto loads = VExt::GetWheelLoads(g_playerVehicle);
    auto boneVels = VExt::GetWheelBoneVelocity(g_playerVehicle);
    auto tracVels = VExt::GetWheelTractionVector(g_playerVehicle);

    auto velWorld = ENTITY::GET_ENTITY_VELOCITY(g_playerVehicle);
    auto posWorld = ENTITY::GET_ENTITY_COORDS(g_playerVehicle, 0);

    std::vector<SSlipInfo> slipAngles;

    auto numWheels = VExt::GetNumWheels(g_playerVehicle);

    auto wheelCoords = Util::GetWheelCoords(g_playerVehicle);
    auto wheelOffs = VExt::GetWheelOffsets(g_playerVehicle);

    for (uint32_t i = 0; i < numWheels; ++i) {
        Vector3 boneVel = boneVels[i];
        Vector3 tracVel = tracVels[i] * -1.0f;

        // Translate absolute bone velocity to relative velocity
        auto boneVelProjection = posWorld + boneVel;
        auto boneVelRel = ENTITY::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS(
            g_playerVehicle, boneVelProjection.x, boneVelProjection.y, boneVelProjection.z);

        auto tracVelProjection = posWorld + tracVel;
        auto tracVelRel = ENTITY::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS(
            g_playerVehicle, tracVelProjection.x, tracVelProjection.y, tracVelProjection.z);

        auto tracVelRelIgnoreZ = tracVelRel;
        tracVelRelIgnoreZ.z = boneVelRel.z;

        float angle = GetAngleBetween(tracVelRelIgnoreZ, boneVelRel);

        if (std::isnan(angle) || Length(velWorld) == 0.0f) {
            boneVel = Vector3();
            boneVelRel = Vector3();
            tracVel = Vector3();
            angle = 0.0f;
        }

        slipAngles.push_back({ angle, loads[i], Length(boneVelRel)});

        if (g_settings.Debug.DisplayInfo) {
            int alpha = 255;
            if (!VExt::IsWheelSteered(g_playerVehicle, i)) {
                alpha = 63;
            }

            Vector3 boneVelProjection2 = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(g_playerVehicle,
                wheelOffs[i].x + boneVelRel.x, wheelOffs[i].y + boneVelRel.y, wheelOffs[i].z + boneVelRel.z);
            UI::DrawSphere(boneVelProjection2, 0.05f, Util::ColorI{ 255, 255, 255, alpha });
            GRAPHICS::DRAW_LINE(wheelCoords[i].x, wheelCoords[i].y, wheelCoords[i].z,
                boneVelProjection2.x, boneVelProjection2.y, boneVelProjection2.z, 255, 255, 255, alpha);

            Vector3 tracVelProjection2 = ENTITY::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS(g_playerVehicle,
                wheelOffs[i].x + tracVelRelIgnoreZ.x, wheelOffs[i].y + tracVelRelIgnoreZ.y, wheelOffs[i].z + tracVelRelIgnoreZ.z);
            UI::DrawSphere(tracVelProjection2, 0.05f, Util::ColorI{ 255, 0, 0, alpha });
            GRAPHICS::DRAW_LINE(wheelCoords[i].x, wheelCoords[i].y, wheelCoords[i].z,
                tracVelProjection2.x, tracVelProjection2.y, tracVelProjection2.z, 255, 0, 0, alpha);
        }
    }

    return slipAngles;
}

// The downside of this method based on slip angle, is that high-slip-ratio
// handlings are very weak and need a low FFB.Gamma to ramp up the "early" force with
// "low" steering angles.
float calcSlipRatio(float slip, float slipOpt,
                    float slipMultA, float slipMultB,
                    float outMultA, float outMultB) {
    float slipRatio = 0.0f;

    // Understeer just based on slip
    if (abs(slip) > slipOpt * slipMultB) {
        slipRatio = std::clamp(slipRatio, outMultB, 1.0f);
    }
    else if (abs(slip) > slipOpt * slipMultA) {
        slipRatio = map(abs(slip), slipOpt * slipMultA, slipOpt * slipMultB, outMultA, outMultB);
    }
    else if (abs(slip) > slipOpt) {
        slipRatio = map(abs(slip), slipOpt, slipOpt * slipMultA, 1.0f, outMultA);
    }
    else {
        // Normalize slip to ratio
        slipRatio = map(abs(slip), 0.0f, slipOpt, 0.0f, 1.0f);
        float x = slipRatio;

        // rNorm normalizes the response curve for different fTractionCurveLateral values
        // Tested from 5 degrees to 30 degrees
        // Results in similar force at a similar lock (< fTractionCurveLateral).
        float rNorm = g_settings.Wheel.FFB.ResponseCurve * 
            map(rad2deg(slipOpt), 
                g_settings.Wheel.FFB.SlipOptMin, g_settings.Wheel.FFB.SlipOptMax,
                g_settings.Wheel.FFB.SlipOptMinMult, g_settings.Wheel.FFB.SlipOptMaxMult);

        // Increase the force quick, then rise towards 1.
        // x + (1 - x) * (the rest): Make the initial part steeper than linear y=x
        // x^rNorm * ((rNorm+1)-rNorm*x): Gently ramp up, then rise, then gently ramp off
        // Put together: Linear ramp up, rise, quickly ramp off towards 1.0
        slipRatio = x + (1.0f - x) * (pow(x, rNorm) * ((rNorm + 1.0f) - rNorm * x));
    }

    return slipRatio * sgn(slip);
}

int calculateSat() {
    auto numWheels = VExt::GetNumWheels(g_playerVehicle);
    if (numWheels < 1)
        return 0;

    const float maxSlipMultA = 1.050f;
    const float maxSlipMultB = 1.075f;
    const float outMultA = 0.50f;
    const float outMultB = 0.35f;

    // in radians
    const float latSlipOpt = *(float*)(VExt::GetHandlingPtr(g_playerVehicle) + hOffsets1604.fTractionCurveLateral);
    const auto comOffset = *(V3F*)(VExt::GetHandlingPtr(g_playerVehicle) + hOffsets1604.vecCentreOfMass.X);
    // in kg
    const float mass = *(float*)(VExt::GetHandlingPtr(g_playerVehicle) + hOffsets1604.fMass);

    auto wheelOffsets = VExt::GetWheelOffsets(g_playerVehicle);
    auto wheelVels = VExt::GetTyreSpeeds(g_playerVehicle);
    auto wheelSteeringMults = VExt::GetWheelSteeringMultipliers(g_playerVehicle);

    auto satValues = WheelInput::CalculateSlipInfo();
    const float weightWheelAvg = mass / (float)satValues.size();

    uint32_t numSteeredWheelsTotal = 0;
    for (uint32_t i = 0; i < satValues.size(); ++i) {
        if (VExt::IsWheelSteered(g_playerVehicle, i)) {
            numSteeredWheelsTotal++;
        }
    }

    float longSlip = 0.0f;
    float slipRatio = 0.0f;
    float steeredWheelsDiv = 0.0f;
    float steeredAxleWeight = 0.0f;

    auto calculateSlip = [&](uint32_t i) {
        float thisSlipRatio = calcSlipRatio(satValues[i].Angle, latSlipOpt, maxSlipMultA, maxSlipMultB, outMultA, outMultB);
        slipRatio += thisSlipRatio;

        // std::max so we ignore divide-by-almost-zero stuff
        float thisLongSlip = wheelVels[i] / std::max(1.0f, satValues[i].VelocityAmplitude);
        longSlip += thisLongSlip;
        steeredAxleWeight += satValues[i].Weight;

        steeredWheelsDiv += 1.0f;
        if (g_settings.Debug.DisplayInfo) {
            UI::ShowText(0.0f + static_cast<float>(i) * 0.075f, 0.75f, 0.3f, fmt::format("[{}]Angle: {:.2f}\nSlipRatio:{:.2f}\nLongSlip: {:.2f}", i, rad2deg(satValues[i].Angle), thisSlipRatio, thisLongSlip));
        }
    };

    // Bikes have the front wheel index 1 instead of 0
    if (numWheels == 2) {
        calculateSlip(1);
    }
    // E.g. Chimera has 1 steered wheel (and is a quad)
    else if (numSteeredWheelsTotal == 1) {
        calculateSlip(0);
    }
    // Assume 2 front steered wheels for all other vehicles
    // Even 4+-wheel or rear-wheel steered vehicles
    else {
        calculateSlip(0);
        calculateSlip(1);
    }

    slipRatio /= steeredWheelsDiv;
    longSlip /= steeredWheelsDiv;
    longSlip = std::clamp(longSlip, 1.0f, 10.0f);
    
    float longSlipMult = map(longSlip, 1.0f, 2.0f, 1.0f, 0.0f);
    longSlipMult = std::clamp(longSlipMult, 0.2f, 1.0f);

    float velFac = std::clamp(ENTITY::GET_ENTITY_SPEED(g_playerVehicle), 0.0f, 1.0f);

    // Heavier under braking, lighter under no braking, zero when airborne. 1 when stopped, but this defaults to 0 which is also fine.

    float frontAxleOffset = -1.0f;
    float rearAxleOffset = 1.0f;

    // Bikes
    if (numWheels == 2) {
        frontAxleOffset = wheelOffsets[1].y;
        rearAxleOffset = wheelOffsets[0].y;
    }
    // The rest
    else {
        frontAxleOffset = wheelOffsets[0].y;
        rearAxleOffset = wheelOffsets[numWheels-1].y;
    }
    float wheelbase = frontAxleOffset - rearAxleOffset;

    // front bias - except for bikes but we stealthily swapped it anyway.
    float comBiasFront = map(comOffset.y, rearAxleOffset, frontAxleOffset, 0.0f, 1.0f);

    float frontAxleDesignWeight = mass * comBiasFront;

    float weightTransferFactor = steeredAxleWeight / frontAxleDesignWeight;

    float satForce = g_settings.Wheel.FFB.SATAmpMult * 10000.0f * slipRatio * velFac * weightTransferFactor * longSlipMult;

    if (g_settings.Wheel.FFB.LUTFile.empty()) {
        float adf = static_cast<float>(g_settings.Wheel.FFB.AntiDeadForce);
        if (satForce > 0.0f) {
            satForce = map(satForce, 0.0f, 10000.0f, adf, 10000.0f);
        }
        if (satForce < 0.0f) {
            satForce = map(satForce, -10000.0f, -0.0f, -10000.0f, -adf);
        }
    }
    float satMax = static_cast<float>(g_settings.Wheel.FFB.SATMax);
    satForce = std::clamp(satForce, -satMax, satMax);
    return static_cast<int>(satForce);
}

float getFloatingSteeredWheelsRatio(Vehicle v) {
    auto suspensionStates = g_vehData.mWheelsOnGround;

    float wheelsOffGroundRatio = 0.0f;
    float wheelsInAir = 0.0f;
    float wheelsSteered = 0.0f;

    for (int i = 0; i < g_vehData.mWheelCount; i++) {
        if (VExt::IsWheelSteered(v, i)) {
            wheelsSteered += 1.0f;
            if (suspensionStates[i] == false) {
                wheelsInAir += 1.0f;
            }
        }
    }
    if (wheelsSteered != 0.0f) {
        wheelsOffGroundRatio = wheelsInAir / wheelsSteered;
    }
    return wheelsOffGroundRatio;
}

void WheelInput::PlayFFBGround() {
    if (!g_settings.Wheel.FFB.Enable ||
        g_controls.PrevInput != CarControls::Wheel) {
        return;
    }

    if (g_settings.Wheel.Options.LogiLEDs) {
        g_controls.PlayLEDs(g_vehData.mRPM, 0.45f, 0.95f);
    }

    // avgAngle: left is positive
    // steerVal: left is negative
    // Rear-wheel steered cars don't match, so this needs to be flipped in that case.
    float avgAngle = VExt::GetWheelAverageAngle(g_playerVehicle) * g_settings().Steering.Wheel.SteeringMult;

    float steerVal = map(g_controls.SteerVal, 0.0f, 1.0f, -1.0f, 1.0f);    
    if (sgn(avgAngle) == sgn(steerVal)) {
        avgAngle = -avgAngle;
    }

    float wheelsOffGroundRatio = getFloatingSteeredWheelsRatio(g_playerVehicle);

    int detailForce = std::clamp(calculateDetail(), -g_settings.Wheel.FFB.DetailLim, g_settings.Wheel.FFB.DetailLim);
    int satForce = calculateSat();
    int damperForce = calculateDamper(50.0f, wheelsOffGroundRatio);

    // Decrease damper if sat rises, so constantForce doesn't fight against damper
    float damperMult = 1.0f - std::min(fabs((float)satForce), 10000.0f) / 10000.0f;
    damperForce = (int)(damperMult * (float)damperForce);

    // Dampen suspension, minimize damper, minimize SAT
    if (hasAltInputs(g_playerVehicle)) {
        detailForce /= 10;
        satForce /= 5;
        damperForce = g_settings.Wheel.FFB.DamperMin;
    }

    int totalForce = satForce + detailForce;
    calculateSoftLock(totalForce, damperForce);

    lastConstantForce = static_cast<float>(totalForce);
    g_controls.PlayFFBDynamics(std::clamp(totalForce, -10000, 10000), std::clamp(damperForce, -10000, 10000));

    const float minGforce = 5.0f;
    const float maxGforce = 50.0f;
    const float minForce = 500.0f;
    const float maxForce = 10000.0f;
    float gForce = abs(g_vehData.mAcceleration.y) / 9.81f; // TODO: Average/dampen later
    bool collision = gForce > minGforce;
    int res = static_cast<int>(map(gForce, minGforce, maxGforce, minForce, maxForce) * g_settings.Wheel.FFB.CollisionMult);
    if (collision) {
        g_controls.PlayFFBCollision(std::clamp(res, -10000, 10000));
    }

    if (collision) {
        UI::Notify(DEBUG, fmt::format("Collision @ ~r~{:.3f}G~w~~n~"
            "FFB: {}", gForce, res));
    }

    if (g_settings.Debug.DisplayInfo) {
        UI::ShowText(0.85, 0.275, 0.4, fmt::format("{}FFBSat:\t\t{}~w~", abs(satForce) > g_settings.Wheel.FFB.SATMax ? "~r~" : "~w~", satForce), 4);
        UI::ShowText(0.85, 0.300, 0.4, fmt::format("{}FFBFin:\t\t{}~w~", abs(totalForce) > 10000 ? "~r~" : "~w~", totalForce), 4);
        UI::ShowText(0.85, 0.325, 0.4, fmt::format("Damper:\t\t{}", damperForce), 4);
        UI::ShowText(0.85, 0.350, 0.4, fmt::format("Detail:\t\t{}", detailForce), 4);
    }
}

void WheelInput::PlayFFBWater() {
    if (!g_settings.Wheel.FFB.Enable ||
        g_controls.PrevInput != CarControls::Wheel) {
        return;
    }

    if (g_settings.Wheel.Options.LogiLEDs) {
        g_controls.PlayLEDs(g_vehData.mRPM, 0.45f, 0.95f);
    }

    bool isInWater = ENTITY::GET_ENTITY_SUBMERGED_LEVEL(g_playerVehicle) > 0.10f;
    int damperForce = calculateDamper(50.0f, isInWater ? 0.25f : 1.0f);
    int detailForce = calculateDetail();
    int satForce = calculateSat();

    if (!isInWater) {
        satForce = 0;
    }

    int totalForce = satForce + detailForce;
    calculateSoftLock(totalForce, damperForce);
    lastConstantForce = static_cast<float>(totalForce);
    g_controls.PlayFFBDynamics(totalForce, damperForce);

    if (g_settings.Debug.DisplayInfo) {
        UI::ShowText(0.85, 0.275, 0.4, fmt::format("{}FFBSat:\t\t{}~w~", abs(satForce) > 10000 ? "~r~" : "~w~", satForce), 4);
        UI::ShowText(0.85, 0.300, 0.4, fmt::format("{}FFBFin:\t\t{}~w~", abs(totalForce) > 10000 ? "~r~" : "~w~", totalForce), 4);
        UI::ShowText(0.85, 0.325, 0.4, fmt::format("Damper:\t{}", damperForce), 4);
    }
}
