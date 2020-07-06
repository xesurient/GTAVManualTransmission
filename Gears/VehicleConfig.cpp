#include "VehicleConfig.h"
#include "Util/Strings.hpp"
#include <filesystem>

#define CHECK_LOG_SI_ERROR(result, operation) \
    if (result < 0) { \
        logger.Write(ERROR, "[Settings] %s Failed to %s, SI_Error [%d]", \
        __FUNCTION__, operation, result); \
    }

VehicleConfig::VehicleConfig(const ScriptSettings& gSettings, const std::string& file) {
    loadSettings(gSettings, file);
}

#pragma warning(push)
#pragma warning(disable: 4244)
void VehicleConfig::loadSettings(const ScriptSettings& gSettings, const std::string& file) {
    CSimpleIniA ini;
    ini.SetUnicode();
    SI_Error result = ini.LoadFile(file.c_str());
    CHECK_LOG_SI_ERROR(result, "load");

    Name = std::filesystem::path(file).stem().string();

    // [ID]
    std::string allNames = ini.GetValue("ID", "ModelName", "");
    std::string allPlates = ini.GetValue("ID", "Plate", "");
    ModelNames = StrUtil::split(allNames, ' ');
    Plates = StrUtil::split(allPlates, ' ');
    Description = ini.GetValue("ID", "Description", "No description.");
    
    // [MT_OPTIONS]
    MTOptions.ShiftMode =
        static_cast<EShiftMode>(ini.GetLongValue("MT_OPTIONS", "ShiftMode", EToInt(gSettings.MTOptions.ShiftMode)));
    MTOptions.ClutchCreep = ini.GetBoolValue("MT_OPTIONS", "ClutchCatching", gSettings.MTOptions.ClutchCreep);
    MTOptions.ClutchShiftH = ini.GetBoolValue("MT_OPTIONS", "ClutchShiftingH", gSettings.MTOptions.ClutchShiftH);
    MTOptions.ClutchShiftS = ini.GetBoolValue("MT_OPTIONS", "ClutchShiftingS", gSettings.MTOptions.ClutchShiftS);

    // [MT_PARAMS]
    MTParams.ClutchThreshold = ini.GetDoubleValue("MT_PARAMS", "ClutchCatchpoint", gSettings.MTParams.ClutchThreshold);
    MTParams.StallingRPM = ini.GetDoubleValue("MT_PARAMS", "StallingRPM", gSettings.MTParams.StallingRPM);
    MTParams.StallingRate = ini.GetDoubleValue("MT_PARAMS", "StallingRate", gSettings.MTParams.StallingRate);
    MTParams.StallingSlip = ini.GetDoubleValue("MT_PARAMS", "StallingSlip", gSettings.MTParams.StallingSlip);
    MTParams.RPMDamage = ini.GetDoubleValue("MT_PARAMS", "RPMDamage", gSettings.MTParams.RPMDamage);
    MTParams.MisshiftDamage = ini.GetDoubleValue("MT_PARAMS", "MisshiftDamage", gSettings.MTParams.MisshiftDamage);
    MTParams.EngBrakePower = ini.GetDoubleValue("MT_PARAMS", "EngBrakePower", gSettings.MTParams.EngBrakePower);
    MTParams.EngBrakeThreshold = ini.GetDoubleValue("MT_PARAMS", "EngBrakeThreshold", gSettings.MTParams.EngBrakeThreshold);
    MTParams.CreepIdleThrottle = ini.GetDoubleValue("MT_PARAMS", "CreepIdleThrottle", gSettings.MTParams.CreepIdleThrottle);
    MTParams.CreepIdleRPM = ini.GetDoubleValue("MT_PARAMS", "CreepIdleRPM", gSettings.MTParams.CreepIdleRPM);

    // [DRIVING_ASSISTS]
    DriveAssists.ABS.Enable = ini.GetBoolValue("DRIVING_ASSISTS", "ABS", gSettings.DriveAssists.ABS.Enable);
    DriveAssists.ABS.Filter = ini.GetBoolValue("DRIVING_ASSISTS", "ABSFilter", gSettings.DriveAssists.ABS.Filter);
    DriveAssists.TCS.Enable = ini.GetLongValue("DRIVING_ASSISTS", "TCS", gSettings.DriveAssists.TCS.Enable);
    DriveAssists.TCS.Mode = ini.GetLongValue("DRIVING_ASSISTS", "TCSMode", gSettings.DriveAssists.TCS.Mode);
    DriveAssists.TCS.SlipMax = ini.GetDoubleValue("DRIVING_ASSISTS", "TCSSlipMax", gSettings.DriveAssists.TCS.SlipMax);
    DriveAssists.ESP.Enable = ini.GetBoolValue("DRIVING_ASSISTS", "ESP", gSettings.DriveAssists.ESP.Enable);
    DriveAssists.ESP.OverMin = ini.GetDoubleValue("DRIVING_ASSISTS", "ESPOverMin", gSettings.DriveAssists.ESP.OverMin);
    DriveAssists.ESP.OverMax = ini.GetDoubleValue("DRIVING_ASSISTS", "ESPOverMax", gSettings.DriveAssists.ESP.OverMax);
    DriveAssists.ESP.OverMinComp = ini.GetDoubleValue("DRIVING_ASSISTS", "ESPOverMinComp", gSettings.DriveAssists.ESP.OverMinComp);
    DriveAssists.ESP.OverMaxComp = ini.GetDoubleValue("DRIVING_ASSISTS", "ESPOverMaxComp", gSettings.DriveAssists.ESP.OverMaxComp);
    DriveAssists.ESP.UnderMin = ini.GetDoubleValue("DRIVING_ASSISTS", "ESPUnderMin", gSettings.DriveAssists.ESP.UnderMin);
    DriveAssists.ESP.UnderMax = ini.GetDoubleValue("DRIVING_ASSISTS", "ESPUnderMax", gSettings.DriveAssists.ESP.UnderMax);
    DriveAssists.ESP.UnderMinComp = ini.GetDoubleValue("DRIVING_ASSISTS", "ESPUnderMinComp", gSettings.DriveAssists.ESP.UnderMinComp);
    DriveAssists.ESP.UnderMaxComp = ini.GetDoubleValue("DRIVING_ASSISTS", "ESPUnderMaxComp", gSettings.DriveAssists.ESP.UnderMaxComp);

    DriveAssists.LSD.Enable = ini.GetBoolValue("DRIVING_ASSISTS", "LSD", gSettings.DriveAssists.LSD.Enable);
    DriveAssists.LSD.Viscosity = ini.GetDoubleValue("DRIVING_ASSISTS", "LSDViscosity", gSettings.DriveAssists.LSD.Viscosity);

    // [SHIFT_OPTIONS]
    ShiftOptions.UpshiftCut = ini.GetBoolValue("SHIFT_OPTIONS", "UpshiftCut", gSettings.ShiftOptions.UpshiftCut);
    ShiftOptions.DownshiftBlip = ini.GetBoolValue("SHIFT_OPTIONS", "DownshiftBlip", gSettings.ShiftOptions.DownshiftBlip);
    ShiftOptions.ClutchRateMult = ini.GetDoubleValue("SHIFT_OPTIONS", "ClutchRateMult", gSettings.ShiftOptions.ClutchRateMult);
    ShiftOptions.RPMTolerance = ini.GetDoubleValue("SHIFT_OPTIONS", "RPMTolerance", gSettings.ShiftOptions.RPMTolerance);

    // [AUTO_PARAMS]
    AutoParams.UpshiftLoad = ini.GetDoubleValue("AUTO_PARAMS", "UpshiftLoad", gSettings.AutoParams.UpshiftLoad);
    AutoParams.DownshiftLoad = ini.GetDoubleValue("AUTO_PARAMS", "DownshiftLoad", gSettings.AutoParams.DownshiftLoad);
    AutoParams.NextGearMinRPM = ini.GetDoubleValue("AUTO_PARAMS", "NextGearMinRPM", gSettings.AutoParams.NextGearMinRPM);
    AutoParams.CurrGearMinRPM = ini.GetDoubleValue("AUTO_PARAMS", "CurrGearMinRPM", gSettings.AutoParams.CurrGearMinRPM);
    AutoParams.EcoRate = ini.GetDoubleValue("AUTO_PARAMS", "EcoRate", gSettings.AutoParams.EcoRate);
    AutoParams.DownshiftTimeoutMult = ini.GetDoubleValue("AUTO_PARAMS", "DownshiftTimeoutMult", gSettings.AutoParams.DownshiftTimeoutMult);
    AutoParams.UsingATCU = ini.GetBoolValue("AUTO_PARAMS", "UsingATCU", gSettings.AutoParams.UsingATCU);

    // [FORCE_FEEDBACK]
    Wheel.FFB.Enable = ini.GetBoolValue("FORCE_FEEDBACK", "Enable", gSettings.Wheel.FFB.Enable);
    Wheel.FFB.Scale = ini.GetBoolValue("FORCE_FEEDBACK", "Scale", gSettings.Wheel.FFB.Scale);
    Wheel.FFB.AntiDeadForce = ini.GetLongValue("FORCE_FEEDBACK", "AntiDeadForce", gSettings.Wheel.FFB.AntiDeadForce);
    Wheel.FFB.SATAmpMult = ini.GetDoubleValue("FORCE_FEEDBACK", "SATAmpMult", gSettings.Wheel.FFB.SATAmpMult);
    Wheel.FFB.SATMax = ini.GetLongValue("FORCE_FEEDBACK", "SATMax", gSettings.Wheel.FFB.SATMax);
    Wheel.FFB.SATFactor = ini.GetDoubleValue("FORCE_FEEDBACK", "SATFactor", gSettings.Wheel.FFB.SATFactor);
    Wheel.FFB.DamperMax = ini.GetLongValue("FORCE_FEEDBACK", "DamperMax", gSettings.Wheel.FFB.DamperMax);
    Wheel.FFB.DamperMin = ini.GetLongValue("FORCE_FEEDBACK", "DamperMin", gSettings.Wheel.FFB.DamperMin); ;
    Wheel.FFB.DamperMinSpeed = ini.GetDoubleValue("FORCE_FEEDBACK", "DamperMinSpeed", gSettings.Wheel.FFB.DamperMinSpeed);
    Wheel.FFB.DetailMult = ini.GetDoubleValue("FORCE_FEEDBACK", "DetailMult", gSettings.Wheel.FFB.DetailMult);
    Wheel.FFB.DetailLim = ini.GetLongValue("FORCE_FEEDBACK", "DetailLim", gSettings.Wheel.FFB.DetailLim);
    Wheel.FFB.DetailMAW = ini.GetLongValue("FORCE_FEEDBACK", "DetailMaw", gSettings.Wheel.FFB.DetailMAW);
    Wheel.FFB.CollisionMult = ini.GetDoubleValue("FORCE_FEEDBACK", "CollisionMult", gSettings.Wheel.FFB.CollisionMult);
    Wheel.FFB.Gamma = ini.GetDoubleValue("FORCE_FEEDBACK", "Gamma", gSettings.Wheel.FFB.Gamma);
    Wheel.FFB.MaxSpeed = ini.GetDoubleValue("FORCE_FEEDBACK", "MaxSpeed", gSettings.Wheel.FFB.MaxSpeed);

    // [STEER]
    Wheel.Steering.Angle = ini.GetDoubleValue("STEER", "Angle", gSettings.Wheel.Steering.AngleCar);
    Wheel.Steering.Gamma = ini.GetDoubleValue("STEER", "Gamma", gSettings.Wheel.Steering.Gamma);

    // [CUSTOM_STEERING]
    CustomSteering.CustomRotationDegrees = ini.GetDoubleValue("CUSTOM_STEERING", "CustomRotationDegrees", gSettings.CustomSteering.CustomRotationDegrees);

    // [HUD]
    HUD.Enable = ini.GetBoolValue("HUD", "EnableHUD", gSettings.HUD.Enable);
    HUD.Font = ini.GetLongValue("HUD", "HUDFont", gSettings.HUD.Font);
    HUD.Outline = ini.GetBoolValue("HUD", "Outline", gSettings.HUD.Outline);

    HUD.Gear.Enable = ini.GetBoolValue("HUD", "GearIndicator", gSettings.HUD.Gear.Enable);
    HUD.Gear.XPos = ini.GetDoubleValue("HUD", "GearXpos", gSettings.HUD.Gear.XPos);
    HUD.Gear.YPos = ini.GetDoubleValue("HUD", "GearYpos", gSettings.HUD.Gear.YPos);
    HUD.Gear.Size = ini.GetDoubleValue("HUD", "GearSize", gSettings.HUD.Gear.Size);
    HUD.Gear.TopColorR = ini.GetLongValue("HUD", "GearTopColorR", gSettings.HUD.Gear.TopColorR);
    HUD.Gear.TopColorG = ini.GetLongValue("HUD", "GearTopColorG", gSettings.HUD.Gear.TopColorG);
    HUD.Gear.TopColorB = ini.GetLongValue("HUD", "GearTopColorB", gSettings.HUD.Gear.TopColorB);
    HUD.Gear.ColorR = ini.GetLongValue("HUD", "GearColorR", gSettings.HUD.Gear.ColorR);
    HUD.Gear.ColorG = ini.GetLongValue("HUD", "GearColorG", gSettings.HUD.Gear.ColorG);
    HUD.Gear.ColorB = ini.GetLongValue("HUD", "GearColorB", gSettings.HUD.Gear.ColorB);

    HUD.ShiftMode.Enable = ini.GetBoolValue("HUD", "ShiftModeIndicator", gSettings.HUD.ShiftMode.Enable);
    HUD.ShiftMode.XPos = ini.GetDoubleValue("HUD", "ShiftModeXpos", gSettings.HUD.ShiftMode.XPos);
    HUD.ShiftMode.YPos = ini.GetDoubleValue("HUD", "ShiftModeYpos", gSettings.HUD.ShiftMode.YPos);
    HUD.ShiftMode.Size = ini.GetDoubleValue("HUD", "ShiftModeSize", gSettings.HUD.ShiftMode.Size);
    HUD.ShiftMode.ColorR = ini.GetLongValue("HUD", "ShiftModeColorR", gSettings.HUD.ShiftMode.ColorR);
    HUD.ShiftMode.ColorG = ini.GetLongValue("HUD", "ShiftModeColorG", gSettings.HUD.ShiftMode.ColorG);
    HUD.ShiftMode.ColorB = ini.GetLongValue("HUD", "ShiftModeColorB", gSettings.HUD.ShiftMode.ColorB);

    HUD.Speedo.Speedo = ini.GetValue("HUD", "Speedo", gSettings.HUD.Speedo.Speedo.c_str());
    HUD.Speedo.ShowUnit = ini.GetBoolValue("HUD", "SpeedoShowUnit", gSettings.HUD.Speedo.ShowUnit);
    HUD.Speedo.XPos = ini.GetDoubleValue("HUD", "SpeedoXpos", gSettings.HUD.Speedo.XPos);
    HUD.Speedo.YPos = ini.GetDoubleValue("HUD", "SpeedoYpos", gSettings.HUD.Speedo.YPos);
    HUD.Speedo.Size = ini.GetDoubleValue("HUD", "SpeedoSize", gSettings.HUD.Speedo.Size);
    HUD.Speedo.ColorR = ini.GetLongValue("HUD", "SpeedoColorR", gSettings.HUD.Speedo.ColorR);
    HUD.Speedo.ColorG = ini.GetLongValue("HUD", "SpeedoColorG", gSettings.HUD.Speedo.ColorG);
    HUD.Speedo.ColorB = ini.GetLongValue("HUD", "SpeedoColorB", gSettings.HUD.Speedo.ColorB);

    HUD.RPMBar.Enable = ini.GetBoolValue("HUD", "EnableRPMIndicator", gSettings.HUD.RPMBar.Enable);
    HUD.RPMBar.XPos = ini.GetDoubleValue("HUD", "RPMIndicatorXpos", gSettings.HUD.RPMBar.XPos);
    HUD.RPMBar.YPos = ini.GetDoubleValue("HUD", "RPMIndicatorYpos", gSettings.HUD.RPMBar.YPos);
    HUD.RPMBar.XSz = ini.GetDoubleValue("HUD", "RPMIndicatorWidth", gSettings.HUD.RPMBar.XSz);
    HUD.RPMBar.YSz = ini.GetDoubleValue("HUD", "RPMIndicatorHeight", gSettings.HUD.RPMBar.YSz);
    HUD.RPMBar.Redline = ini.GetDoubleValue("HUD", "RPMIndicatorRedline", gSettings.HUD.RPMBar.Redline);

    HUD.RPMBar.BgR = ini.GetLongValue("HUD", "RPMIndicatorBackgroundR", gSettings.HUD.RPMBar.BgR);
    HUD.RPMBar.BgG = ini.GetLongValue("HUD", "RPMIndicatorBackgroundG", gSettings.HUD.RPMBar.BgG);
    HUD.RPMBar.BgB = ini.GetLongValue("HUD", "RPMIndicatorBackgroundB", gSettings.HUD.RPMBar.BgB);
    HUD.RPMBar.BgA = ini.GetLongValue("HUD", "RPMIndicatorBackgroundA", gSettings.HUD.RPMBar.BgA);

    HUD.RPMBar.FgR = ini.GetLongValue("HUD", "RPMIndicatorForegroundR", gSettings.HUD.RPMBar.FgR);
    HUD.RPMBar.FgG = ini.GetLongValue("HUD", "RPMIndicatorForegroundG", gSettings.HUD.RPMBar.FgG);
    HUD.RPMBar.FgB = ini.GetLongValue("HUD", "RPMIndicatorForegroundB", gSettings.HUD.RPMBar.FgB);
    HUD.RPMBar.FgA = ini.GetLongValue("HUD", "RPMIndicatorForegroundA", gSettings.HUD.RPMBar.FgA);

    HUD.RPMBar.RedlineR = ini.GetLongValue("HUD", "RPMIndicatorRedlineR", gSettings.HUD.RPMBar.RedlineR);
    HUD.RPMBar.RedlineG = ini.GetLongValue("HUD", "RPMIndicatorRedlineG", gSettings.HUD.RPMBar.RedlineG);
    HUD.RPMBar.RedlineB = ini.GetLongValue("HUD", "RPMIndicatorRedlineB", gSettings.HUD.RPMBar.RedlineB);
    HUD.RPMBar.RedlineA = ini.GetLongValue("HUD", "RPMIndicatorRedlineA", gSettings.HUD.RPMBar.RedlineA);

    HUD.RPMBar.RevLimitR = ini.GetLongValue("HUD", "RPMIndicatorRevlimitR", gSettings.HUD.RPMBar.RevLimitR);
    HUD.RPMBar.RevLimitG = ini.GetLongValue("HUD", "RPMIndicatorRevlimitG", gSettings.HUD.RPMBar.RevLimitG);
    HUD.RPMBar.RevLimitB = ini.GetLongValue("HUD", "RPMIndicatorRevlimitB", gSettings.HUD.RPMBar.RevLimitB);
    HUD.RPMBar.RevLimitA = ini.GetLongValue("HUD", "RPMIndicatorRevlimitA", gSettings.HUD.RPMBar.RevLimitA);

    HUD.DashIndicators.Enable = ini.GetBoolValue("HUD", "DashIndicators", gSettings.HUD.DashIndicators.Enable);
    HUD.DashIndicators.XPos = ini.GetDoubleValue("HUD", "DashIndicatorsXpos", gSettings.HUD.DashIndicators.XPos);
    HUD.DashIndicators.YPos = ini.GetDoubleValue("HUD", "DashIndicatorsYpos", gSettings.HUD.DashIndicators.YPos);
    HUD.DashIndicators.Size = ini.GetDoubleValue("HUD", "DashIndicatorsSize", gSettings.HUD.DashIndicators.Size);

    // [CAM]
    Misc.Camera.Enable = ini.GetBoolValue("CAM", "Enable", gSettings.Misc.Camera.Enable);
    Misc.Camera.AttachId = ini.GetLongValue("CAM", "AttachId", gSettings.Misc.Camera.AttachId);

    Misc.Camera.Movement.Follow = ini.GetBoolValue("CAM", "FollowMovement", gSettings.Misc.Camera.Movement.Follow);
    Misc.Camera.Movement.RotationDirectionMult = ini.GetDoubleValue("CAM", "MovementMultVel", gSettings.Misc.Camera.Movement.RotationDirectionMult);
    Misc.Camera.Movement.RotationRotationMult = ini.GetDoubleValue("CAM", "MovementMultRot", gSettings.Misc.Camera.Movement.RotationRotationMult);
    Misc.Camera.Movement.RotationMaxAngle = ini.GetDoubleValue("CAM", "MovementCap", gSettings.Misc.Camera.Movement.RotationMaxAngle);

    Misc.Camera.Movement.LongForwardMult = ini.GetDoubleValue("CAM", "LongForwardMult", gSettings.Misc.Camera.Movement.LongForwardMult);
    Misc.Camera.Movement.LongBackwardMult = ini.GetDoubleValue("CAM", "LongBackwardMult", gSettings.Misc.Camera.Movement.LongBackwardMult);
    Misc.Camera.Movement.LongDeadzone = ini.GetDoubleValue("CAM", "LongDeadzone", gSettings.Misc.Camera.Movement.LongDeadzone);
    Misc.Camera.Movement.LongGamma = ini.GetDoubleValue("CAM", "LongGamma", gSettings.Misc.Camera.Movement.LongGamma);
    Misc.Camera.Movement.LongForwardLimit = ini.GetDoubleValue("CAM", "LongForwardLimit", gSettings.Misc.Camera.Movement.LongForwardLimit);
    Misc.Camera.Movement.LongBackwardLimit = ini.GetDoubleValue("CAM", "LongBackwardLimit", gSettings.Misc.Camera.Movement.LongBackwardLimit);

    Misc.Camera.RemoveHead = ini.GetBoolValue("CAM", "RemoveHead", gSettings.Misc.Camera.RemoveHead);
    Misc.Camera.FOV = ini.GetDoubleValue("CAM", "FOV", gSettings.Misc.Camera.FOV);
    Misc.Camera.OffsetHeight = ini.GetDoubleValue("CAM", "OffsetHeight", gSettings.Misc.Camera.OffsetHeight);
    Misc.Camera.OffsetForward = ini.GetDoubleValue("CAM", "OffsetForward", gSettings.Misc.Camera.OffsetForward);
    Misc.Camera.OffsetSide = ini.GetDoubleValue("CAM", "OffsetSide", gSettings.Misc.Camera.OffsetSide);
    Misc.Camera.Pitch = ini.GetDoubleValue("CAM", "Pitch", gSettings.Misc.Camera.Pitch);
    Misc.Camera.LookTime = ini.GetDoubleValue("CAM", "LookTime", gSettings.Misc.Camera.LookTime);
    Misc.Camera.MouseLookTime = ini.GetDoubleValue("CAM", "MouseLookTime", gSettings.Misc.Camera.MouseLookTime);
    Misc.Camera.MouseCenterTimeout = ini.GetLongValue("CAM", "MouseCenterTimeout", gSettings.Misc.Camera.MouseCenterTimeout);
    Misc.Camera.MouseSensitivity = ini.GetDoubleValue("CAM", "MouseSensitivity", gSettings.Misc.Camera.MouseSensitivity);

    Misc.Camera.Bike.Disable = ini.GetBoolValue("CAM", "BikeDisable", gSettings.Misc.Camera.Bike.Disable);
    Misc.Camera.Bike.AttachId = ini.GetLongValue("CAM", "BikeAttachId", gSettings.Misc.Camera.Bike.AttachId);
    Misc.Camera.Bike.FOV = ini.GetDoubleValue("CAM", "BikeFOV", gSettings.Misc.Camera.Bike.FOV);
    Misc.Camera.Bike.OffsetHeight = ini.GetDoubleValue("CAM", "BikeOffsetHeight", gSettings.Misc.Camera.Bike.OffsetHeight);
    Misc.Camera.Bike.OffsetForward = ini.GetDoubleValue("CAM", "BikeOffsetForward", gSettings.Misc.Camera.Bike.OffsetForward);
    Misc.Camera.Bike.OffsetSide = ini.GetDoubleValue("CAM", "BikeOffsetSide", gSettings.Misc.Camera.Bike.OffsetSide);
    Misc.Camera.Bike.Pitch = ini.GetDoubleValue("CAM", "BikePitch", gSettings.Misc.Camera.Bike.Pitch);

}
#pragma warning(pop)
