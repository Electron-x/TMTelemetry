// TMTelemetry.h
//
#pragma once

#include "maniaplanet_telemetry.h"
using namespace NManiaPlanet;

// List-view columns
#define COL_DATE			0x0001
#define COL_TIME			0x0002
#define COL_MAPUID			0x0004
#define COL_MAPNAME			0x0008
#define COL_PLAYERMODEL		0x0010
#define COL_SECTORTIMES		0x0020
#define COL_CHECKPOINTS		0x0040
#define COL_RACETIME		0x0080
#define COL_TOPSPEED		0x0100
#define COL_RESPAWNS		0x0200
#define COL_RUMBLES			0x0400
#define COL_GEARCHANGES		0x0800
#define COL_BRAKESUSED		0x1000
#define COL_FULLSPEED		0x2000
#define COL_WHEELSLIP		0x4000
#define COL_CPSPEED			0x8000
#define COL_HIGHLIGHTCPREC  0x10000
#define COL_DEFAULT			(COL_SECTORTIMES | COL_CHECKPOINTS | COL_RACETIME | COL_TOPSPEED | COL_RESPAWNS | COL_RUMBLES | COL_GEARCHANGES | COL_BRAKESUSED | COL_FULLSPEED | COL_WHEELSLIP)

// Status bar part indices
typedef enum _STATUSBAR_PART
{
	SBP_GAMESTATE = 0,
	SBP_RACESTATE,
	SBP_MAPNAME,
	SBP_PLAYERMODEL,
	SBP_RACETIME,
	SBP_LAPS,
	SBP_CHECKPOINTS,
	SBP_SPEEDMETER,
	SBP_ENGINERPM,
	SBP_CURGEAR,
	SBP_STEERING,
	SBP_THROTTLE,
	SBP_RUMBLE,
	SBP_BRAKING,
	SBP_LAST
} STATUSBAR_PART;

// Globals used in DoTelemetry.cpp
extern HWND hwndListView;			// List-view control handle
extern HWND hwndStatusBar;			// Status bar control handle
extern HWND hwndTBSteering;			// Trackbar control handle
extern HWND hwndPBThrottle;			// First progress bar control handle
extern HWND hwndPBRumble;			// Second progress bar control handle
extern Nat32 uHeaderVersion;		// STelemetry header version
extern UINT uRumbleThreshold;		// Rumble intensity threshold
extern UINT uFullspeedThreshold;	// Full throttle threshold
extern BOOL bAutoDelete;			// Auto delete aborted races
extern BOOL bMilesPerHour;			// Indicate mph instead of km/h
extern DWORD dwColumns;				// List-view columns to show
extern TCHAR szFileName[MAX_PATH];	// Data export file name
