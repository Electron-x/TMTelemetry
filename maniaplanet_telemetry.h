#ifndef _MANIAPLANET_TELEMETRY_H
#define _MANIAPLANET_TELEMETRY_H

#pragma once

namespace NManiaPlanet {

enum {
    ECurVersion = 2,
};

typedef unsigned int Nat32;
typedef unsigned int Bool;

struct Vec3 {
    float x,y,z;
};
struct Quat {
	float w,x,y,z;
};

struct STelemetry {
    struct SHeader {
        char        Magic[32];              //  "ManiaPlanet_Telemetry"
        Nat32       Version;
        Nat32       Size;                   // == sizeof(STelemetry)
    };
    enum EGameState {
        EState_Starting = 0,
        EState_Menus,
        EState_Running,
        EState_Paused,
    };
    enum ERaceState {
        ERaceState_BeforeState = 0,
        ERaceState_Running,
        ERaceState_Finished,
    };
    struct SGameState {
        EGameState  State;
        char        GameplayVariant[64];    // player model 'StadiumCar', 'CanyonCar', ....
        char        MapId[64];
        char        MapName[256];
        char        __future__[128];
    };
    struct SRaceState {
        ERaceState  State;
        Nat32       Time;
        Nat32       NbRespawns;
        Nat32       NbCheckpoints;
        Nat32       CheckpointTimes[125];
        Nat32       NbCheckpointsPerLap;    // new since Maniaplanet update 2019-10-10; not supported by Trackmania Turbo.
        Nat32       NbLaps;                 // new since Maniaplanet update 2019-10-10; not supported by Trackmania Turbo.
        char        __future__[24];
    };
    struct SObjectState {
        Nat32       Timestamp;
        Nat32       DiscontinuityCount;     // the number changes everytime the object is moved not continuously (== teleported).
        Quat        Rotation;
        Vec3        Translation;            // +x is "left", +y is "up", +z is "front"
        Vec3        Velocity;               // (world velocity)
        Nat32       LatestStableGroundContactTime;
        char        __future__[32];
    };
    struct SVehicleState {
        Nat32       Timestamp;

        float       InputSteer;
        float       InputGasPedal;
        Bool        InputIsBraking;
        Bool        InputIsHorn;

        float       EngineRpm;              // 1500 -> 10000
        int         EngineCurGear;
        float       EngineTurboRatio;       // 1 turbo starting/full .... 0 -> finished
        Bool        EngineFreeWheeling;

        Bool        WheelsIsGroundContact[4];
        Bool        WheelsIsSliping[4];
        float       WheelsDamperLen[4];
        float       WheelsDamperRangeMin;
        float       WheelsDamperRangeMax;

        float       RumbleIntensity;

        Nat32       SpeedMeter;             // unsigned km/h
        Bool        IsInWater;
        Bool        IsSparkling;
        Bool        IsLightTrails;
        Bool        IsLightsOn;
        Bool        IsFlying;               // long time since touching ground.

        char        __future__[32];
    };
    struct SDeviceState {   // VrChair state.
        Vec3        Euler;                  // yaw, pitch, roll  (order: pitch, roll, yaw)
        float       CenteredYaw;            // yaw accumulated + recentered to apply onto the device
        float       CenteredAltitude;       // Altitude accumulated + recentered

        char        __future__[32];
    };

    SHeader         Header;

    Nat32           UpdateNumber;
    SGameState      Game;
    SRaceState      Race;
    SObjectState    Object;
    SVehicleState   Vehicle;
    SDeviceState    Device;
};

}

#endif // _MANIAPLANET_TELEMETRY_H

