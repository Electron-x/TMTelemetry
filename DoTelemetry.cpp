// DoTelemetry.cpp - Functions for processing the telemetry data
//
#include "stdafx.h"
#include "dotelemetry.h"
#include "tmtelemetry.h"

// Forward declarations
BOOL IsRaceBeforeStart(STelemetryData* pTelemetry);
BOOL IsRaceRunning(STelemetryData* pTelemetry);
BOOL IsRaceFinished(STelemetryData* pTelemetry);

// Processing of the telemetry data
void DoTelemetry(STelemetryData* pTelemetry)
{
	TCHAR szText[MAX_CONTROLTEXT];
	static int nRaceNumber = 0; // Number of attempts
	static Nat32 uLapCount = 0; // Current lap
	static BOOL bAddFinalColumns = FALSE; // Append some stats after the end of each race

	// Static variables for storing values for the statistics:
	static Nat32 uTopSpeed = 0;           // Top speed per race
	static int   nNbGearchanges = 0;      // Number of gear changes per race
	static int   nNbBrakesUsed = 0;       // Number of braking operations per race
	static Nat32 uMaxNbCheckpoints = 0;   // Highest number of CPs per game client
	static int   nNbRumbles = 0;          // Number of rumbles per race
	static BOOL  bIsRumbling = FALSE;     // Are we rumbling right now?
	static Nat32 uFullspeedTime = 0;      // Total time at full throttle
	static Nat32 uFullspeedTimestamp = 0; // Full throttle timestamp
	static BOOL  bIsFullspeed = FALSE;    // Are we going full throttle right now?
	static Nat32 uWheelSlipTime = 0;      // Total time of wheel slippage
	static Nat32 uWheelSlipTimestamp = 0; // Wheel slip timestamp
	static BOOL  bIsSlipping = FALSE;     // Is a wheel slipping right now?

	// Test for updated telemetry data records
	if (pTelemetry->Current.UpdateNumber == pTelemetry->Previous.UpdateNumber)
		return;

	pTelemetry->Previous.UpdateNumber = pTelemetry->Current.UpdateNumber;

	// STelemetry header version; will be displayed in the about box
	if (pTelemetry->Current.Header.Version != uHeaderVersion)
		uHeaderVersion = pTelemetry->Current.Header.Version;

	// Check whether a used data element has changed and update the
	// respective status bar and list-view panes with the new values

	// Game state
	if (pTelemetry->Current.Game.State != pTelemetry->Previous.Game.State)
	{
		switch (pTelemetry->Current.Game.State)
		{
		case STelemetry::EState_Menus:
			// The provided telemetry data records are not cleared after a race ends.
			// To not display wrong or unusable values, we clean at least the status
			// bar parts while the game is in menu state.
			// We also need to reset our saved telemetry values so that the start of
			// a new race will also show the data that has not changed.
			InitTelemetryData(pTelemetry);

			// As long as we are in the menu, the telemetry data will not be updated.
			// We therefore need to restore the update number so that this function
			// will not be executed until changes are made.
			pTelemetry->Previous.UpdateNumber = pTelemetry->Current.UpdateNumber;
			pTelemetry->Previous.Game.State = pTelemetry->Current.Game.State;

			// Clear the parts of the status bar that contain race data
			StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGameMenus);
			for (int i = SBP_RACESTATE; i <= SBP_BRAKING; i++)
				StatusBar_SetText(hwndStatusBar, i, TEXT(""));

			if (hwndTBSteering != NULL)
			{
				SendMessage(hwndTBSteering, TBM_CLEARSEL, (WPARAM)TRUE, 0);
				SendMessage(hwndTBSteering, TBM_SETPOS, (WPARAM)TRUE, 0);
			}

			if (hwndPBThrottle != NULL) SendMessage(hwndPBThrottle, PBM_SETPOS, 0, 0);
			if (hwndPBRumble != NULL) SendMessage(hwndPBRumble, PBM_SETPOS, 0, 0);

			return;	// Nothing to monitor

		case STelemetry::EState_Starting:
			StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGameStarting);
			break;

		case STelemetry::EState_Running:
			StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGameRunning);
			break;

		case STelemetry::EState_Paused:
			StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGamePaused);
			break;

		default:
			StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, TEXT(""));
		}

		pTelemetry->Previous.Game.State = pTelemetry->Current.Game.State;
	}

	// Race state
	if (pTelemetry->Current.Race.State != pTelemetry->Previous.Race.State)
	{
		switch (pTelemetry->Current.Race.State)
		{
		case STelemetry::ERaceState_BeforeState:
			StatusBar_SetText(hwndStatusBar, SBP_RACESTATE, szRaceBeforeStart);
			break;

		case STelemetry::ERaceState_Running:
			StatusBar_SetText(hwndStatusBar, SBP_RACESTATE, szRaceRunning);
			break;

		case STelemetry::ERaceState_Finished:
			StatusBar_SetText(hwndStatusBar, SBP_RACESTATE, szRaceFinished);
			break;

		//case STelemetry::ERaceState_Eliminated:
		//	StatusBar_SetText(hwndStatusBar, SBP_RACESTATE, szRaceEliminated);
		//	break;

		default:
			StatusBar_SetText(hwndStatusBar, SBP_RACESTATE, TEXT(""));
		}

		// Handle the countdown
		if (IsRaceBeforeStart(pTelemetry))
		{
			// Reset the lap counter
			uLapCount = 0;
			StatusBar_SetLapCount(hwndStatusBar, SBP_LAPS, uLapCount, pTelemetry->Current.Race.NbLaps);
		}

		// Handle the start of a race
		if (IsRaceRunning(pTelemetry))
		{
			uTopSpeed = 0;
			nNbGearchanges = 0;
			nNbBrakesUsed = 0;
			uMaxNbCheckpoints = 0;

			bIsRumbling = pTelemetry->Current.Vehicle.RumbleIntensity > uRumbleThreshold / 100.0f;
			nNbRumbles = bIsRumbling ? 1 : 0;

			uFullspeedTime = 0;
			uFullspeedTimestamp = pTelemetry->Current.Vehicle.Timestamp;
			bIsFullspeed = pTelemetry->Current.Vehicle.InputGasPedal >= uFullspeedThreshold / 100.0f;

			uWheelSlipTime = 0;
			uWheelSlipTimestamp = pTelemetry->Current.Vehicle.Timestamp;
			bIsSlipping = pTelemetry->Current.Vehicle.WheelsIsSliping[0] || pTelemetry->Current.Vehicle.WheelsIsSliping[1] ||
				pTelemetry->Current.Vehicle.WheelsIsSliping[2] || pTelemetry->Current.Vehicle.WheelsIsSliping[3];

			// Set the lap counter to 1 (the number of checkpoints crossed remains 0 at start)
			uLapCount = 1;
			StatusBar_SetLapCount(hwndStatusBar, SBP_LAPS, uLapCount, pTelemetry->Current.Race.NbLaps);

			// Add a new race to the list-view control and increment the number of attempts
			if (ListView_AddRace(hwndListView, nRaceNumber + 1, COLUMN_AUTOFIT) != -1)
				nRaceNumber++;

			// Optionally add map UID, map name, player model, date and time to each race
			if (dwColumns & COL_MAPUID)
			{
				MultiByteToWideChar(CP_UTF8, 0, pTelemetry->Current.Game.MapId, -1, szText, _countof(szText));
				ListView_AddRaceText(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szMapIdentifier, szText);
			}

			if (dwColumns & COL_MAPNAME)
			{
				MultiByteToWideChar(CP_UTF8, 0, pTelemetry->Current.Game.MapName, -1, szText, _countof(szText));
				ListView_AddRaceText(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szTrackName, szText);
			}

			if (dwColumns & COL_PLAYERMODEL)
			{
				MultiByteToWideChar(CP_UTF8, 0, pTelemetry->Current.Game.GameplayVariant, -1, szText, _countof(szText));
				ListView_AddRaceText(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szPlayerModel, szText);
			}

			if (dwColumns & (COL_DATE | COL_TIME))
			{
				SYSTEMTIME st;
				GetLocalTime(&st);

				if (dwColumns & COL_DATE)
				{
					_sntprintf(szText, _countof(szText), TEXT("%02u-%02u-%02u"), st.wYear, st.wMonth, st.wDay);
					ListView_AddRaceText(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szDate, szText);
				}

				if (dwColumns & COL_TIME)
				{
					_sntprintf(szText, _countof(szText), TEXT("%02u:%02u:%02u"), st.wHour, st.wMinute, st.wSecond);
					ListView_AddRaceText(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szTime, szText);
				}
			}
		}

		// Handle the end of a race
		if (IsRaceFinished(pTelemetry))
		{
			// Test if the checkpoint count has increased to differ between finished and restart
			if (pTelemetry->Current.Race.NbCheckpoints > pTelemetry->Previous.Race.NbCheckpoints)
			{
				bAddFinalColumns = TRUE;

				if (bIsFullspeed)
					uFullspeedTime += pTelemetry->Current.Vehicle.Timestamp - uFullspeedTimestamp;

				if (bIsSlipping)
					uWheelSlipTime += pTelemetry->Current.Vehicle.Timestamp - uWheelSlipTimestamp;
			}
			else if (bAutoDelete && ListView_DeleteRace(hwndListView, nRaceNumber) && nRaceNumber > 0)
				nRaceNumber--;
		}

		pTelemetry->Previous.Race.State = pTelemetry->Current.Race.State;
	}

	// Number of checkpoints
	if (pTelemetry->Current.Race.NbCheckpoints != pTelemetry->Previous.Race.NbCheckpoints)
	{
		// Add the new checkpoint time to the list-view control
		Nat32 uCurrentNbCheckpoints = pTelemetry->Current.Race.NbCheckpoints;
		if (uCurrentNbCheckpoints > 0 && uCurrentNbCheckpoints <= _countof(pTelemetry->Current.Race.CheckpointTimes) &&
			uCurrentNbCheckpoints > uMaxNbCheckpoints)
		{
			if (dwColumns & COL_SECTORTIMES)
				ListView_AddSectorTime(hwndListView, nRaceNumber, COLUMN_AUTOFIT, uCurrentNbCheckpoints,
					pTelemetry->Current.Race.CheckpointTimes[uCurrentNbCheckpoints - 1],
					uCurrentNbCheckpoints >= 2 ? pTelemetry->Current.Race.CheckpointTimes[uCurrentNbCheckpoints - 2] : 0);

			if (dwColumns & COL_CHECKPOINTS)
				ListView_AddCheckpointTime(hwndListView, nRaceNumber, COLUMN_AUTOFIT, uCurrentNbCheckpoints,
					pTelemetry->Current.Race.CheckpointTimes[uCurrentNbCheckpoints - 1]);
		}

		// Update the lap counter after crossing the start/finish line
		Nat32 uCurrentLap = uLapCount;
		Nat32 uNumberOfLaps = pTelemetry->Current.Race.NbLaps;	// Could be zero
		Nat32 uCheckpointsPerLap = pTelemetry->Current.Race.NbCheckpointsPerLap;	// Always 0 with Trackmania Turbo
		
		if (uCheckpointsPerLap != 0)
		{
			uCurrentLap = uCurrentNbCheckpoints / uCheckpointsPerLap;
			// Correct the lap counter by one, except on restart and when crossing the finish line
			if (uCurrentNbCheckpoints != 0 && (uCurrentLap < uNumberOfLaps || uNumberOfLaps == 0))
				uCurrentLap++;
			// Keep the number of laps driven after the race is over
			if (uCurrentLap < uLapCount)
				uCurrentLap = uLapCount;
		}
		
		if (uCurrentLap != uLapCount)
		{
			uLapCount = uCurrentLap;
			StatusBar_SetLapCount(hwndStatusBar, SBP_LAPS, uLapCount, uNumberOfLaps);
		}

		// Update the checkpoint counter
		_sntprintf(szText, _countof(szText), szNbCheckpoints, uCurrentNbCheckpoints);
		StatusBar_SetText(hwndStatusBar, SBP_CHECKPOINTS, szText, TRUE);

		// Here we have to store the highest number of checkpoints per game client so that
		// the list is not flooded with data when Maniaplanet and Turbo run simultaneously
		if (uCurrentNbCheckpoints > pTelemetry->Previous.Race.NbCheckpoints)
			uMaxNbCheckpoints = uCurrentNbCheckpoints;

		pTelemetry->Previous.Race.NbCheckpoints = uCurrentNbCheckpoints;
	}

	// Map name
	if (strcmp(pTelemetry->Current.Game.MapName, pTelemetry->Previous.Game.MapName) != 0)
	{
		// Convert the received UTF-8 string to Unicode UTF-16
		MultiByteToWideChar(CP_UTF8, 0, pTelemetry->Current.Game.MapName, -1, szText, _countof(szText));
		// If a new map name exists, update the file name for data export
		if (_tcslen(szText) > 0)
			lstrcpyn(szFileName, szText, _countof(szFileName));

		StatusBar_SetText(hwndStatusBar, SBP_MAPNAME, szText);

		strncpy(pTelemetry->Previous.Game.MapName, pTelemetry->Current.Game.MapName,
			_countof(pTelemetry->Previous.Game.MapName));
	}

	// Player model
	if (strcmp(pTelemetry->Current.Game.GameplayVariant, pTelemetry->Previous.Game.GameplayVariant) != 0)
	{
		MultiByteToWideChar(CP_UTF8, 0, pTelemetry->Current.Game.GameplayVariant, -1, szText, _countof(szText));
		StatusBar_SetText(hwndStatusBar, SBP_PLAYERMODEL, szText, TRUE);

		strncpy(pTelemetry->Previous.Game.GameplayVariant, pTelemetry->Current.Game.GameplayVariant,
			_countof(pTelemetry->Previous.Game.GameplayVariant));
	}

	// Race time
	if (pTelemetry->Current.Race.Time != pTelemetry->Previous.Race.Time)
	{
		FormatTime(szText, _countof(szText), pTelemetry->Current.Race.Time);
		StatusBar_SetText(hwndStatusBar, SBP_RACETIME, szText, TRUE);

		pTelemetry->Previous.Race.Time = pTelemetry->Current.Race.Time;
	}

	// Speed
	if (pTelemetry->Current.Vehicle.SpeedMeter != pTelemetry->Previous.Vehicle.SpeedMeter)
	{
		Nat32 uCurrentSpeedMeter = pTelemetry->Current.Vehicle.SpeedMeter;

		if (uCurrentSpeedMeter > uTopSpeed)
			uTopSpeed = uCurrentSpeedMeter;

		if (bMilesPerHour)
			_sntprintf(szText, _countof(szText), szSpeedMph, MulDiv(uCurrentSpeedMeter, 1000000, 1609344));
		else
			_sntprintf(szText, _countof(szText), szSpeedKmh, uCurrentSpeedMeter);
		StatusBar_SetText(hwndStatusBar, SBP_SPEEDMETER, szText, TRUE);

		pTelemetry->Previous.Vehicle.SpeedMeter = uCurrentSpeedMeter;
	}

	// Revolutions per minute
	if (pTelemetry->Current.Vehicle.EngineRpm != pTelemetry->Previous.Vehicle.EngineRpm)
	{
		_sntprintf(szText, _countof(szText), szEngineRpm, pTelemetry->Current.Vehicle.EngineRpm);
		StatusBar_SetText(hwndStatusBar, SBP_ENGINERPM, szText, TRUE);

		pTelemetry->Previous.Vehicle.EngineRpm = pTelemetry->Current.Vehicle.EngineRpm;
	}

	// Gear
	if (pTelemetry->Current.Vehicle.EngineCurGear != pTelemetry->Previous.Vehicle.EngineCurGear)
	{
		nNbGearchanges++;

		_sntprintf(szText, _countof(szText), szEngineCurGear, pTelemetry->Current.Vehicle.EngineCurGear);
		StatusBar_SetText(hwndStatusBar, SBP_CURGEAR, szText, TRUE);

		pTelemetry->Previous.Vehicle.EngineCurGear = pTelemetry->Current.Vehicle.EngineCurGear;
	}

	// Steering
	if (pTelemetry->Current.Vehicle.InputSteer != pTelemetry->Previous.Vehicle.InputSteer)
	{
		float fCurrentInputSteer = pTelemetry->Current.Vehicle.InputSteer;

		if (hwndTBSteering != NULL)
		{
			LPARAM lPos = (LPARAM)(fCurrentInputSteer * 100.0);
			SendMessage(hwndTBSteering, TBM_SETSELSTART, (WPARAM)TRUE, fCurrentInputSteer < 0.0f ? lPos : 0);
			SendMessage(hwndTBSteering, TBM_SETSELEND, (WPARAM)TRUE, fCurrentInputSteer < 0.0f ? 0 : lPos);
			SendMessage(hwndTBSteering, TBM_SETPOS, (WPARAM)TRUE, lPos);
		}
		else
		{
			_sntprintf(szText, _countof(szText), szSteering, fCurrentInputSteer * 100.0);
			StatusBar_SetText(hwndStatusBar, SBP_STEERING, szText, TRUE);
		}

		pTelemetry->Previous.Vehicle.InputSteer = fCurrentInputSteer;
	}

	// Gas pedal
	if (pTelemetry->Current.Vehicle.InputGasPedal != pTelemetry->Previous.Vehicle.InputGasPedal)
	{
		float fCurrentInputGasPedal = pTelemetry->Current.Vehicle.InputGasPedal;

		// Determine full throttle percentage per race. The default threshold
		// of 0.85 compensates that the game throttles down during shifting.
		if (fCurrentInputGasPedal >= uFullspeedThreshold / 100.0f)
		{
			if (!bIsFullspeed)
			{
				bIsFullspeed = TRUE;
				uFullspeedTimestamp = pTelemetry->Current.Vehicle.Timestamp;
			}
		}
		else
		{
			if (bIsFullspeed)
			{
				bIsFullspeed = FALSE;
				uFullspeedTime += pTelemetry->Current.Vehicle.Timestamp - uFullspeedTimestamp;
			}
		}

		if (hwndPBThrottle != NULL)
			SendMessage(hwndPBThrottle, PBM_SETPOS, (WPARAM)((fCurrentInputGasPedal * 100.0) + 0.5), 0);
		else
		{
			_sntprintf(szText, _countof(szText), szGasPedal, fCurrentInputGasPedal * 100.0);
			StatusBar_SetText(hwndStatusBar, SBP_THROTTLE, szText, TRUE);
		}

		pTelemetry->Previous.Vehicle.InputGasPedal = fCurrentInputGasPedal;
	}

	// Wheel slip
	if (memcmp(&pTelemetry->Current.Vehicle.WheelsIsSliping, &pTelemetry->Previous.Vehicle.WheelsIsSliping,
		sizeof(pTelemetry->Current.Vehicle.WheelsIsSliping)) != 0)
	{
		// Determine the percentage of time per race in which at least one wheel is slipping.
		if (pTelemetry->Current.Vehicle.WheelsIsSliping[0] || pTelemetry->Current.Vehicle.WheelsIsSliping[1] ||
			pTelemetry->Current.Vehicle.WheelsIsSliping[2] || pTelemetry->Current.Vehicle.WheelsIsSliping[3])
		{
			if (!bIsSlipping)
			{
				bIsSlipping = TRUE;
				uWheelSlipTimestamp = pTelemetry->Current.Vehicle.Timestamp;
			}
		}
		else
		{
			if (bIsSlipping)
			{
				bIsSlipping = FALSE;
				uWheelSlipTime += pTelemetry->Current.Vehicle.Timestamp - uWheelSlipTimestamp;
			}
		}

		memcpy(&pTelemetry->Previous.Vehicle.WheelsIsSliping, &pTelemetry->Current.Vehicle.WheelsIsSliping,
			sizeof(pTelemetry->Previous.Vehicle.WheelsIsSliping));
	}

	// Rumble intensity
	if (pTelemetry->Current.Vehicle.RumbleIntensity != pTelemetry->Previous.Vehicle.RumbleIntensity)
	{
		float fCurrentRumbleIntensity = pTelemetry->Current.Vehicle.RumbleIntensity;

		// Determine number of rumbles per race
		if (fCurrentRumbleIntensity > uRumbleThreshold / 100.0f)
		{
			if (!bIsRumbling)
			{
				bIsRumbling = TRUE;
				nNbRumbles++;
			}
		}
		else
		{
			if (bIsRumbling)
				bIsRumbling = FALSE;
		}

		if (hwndPBRumble != NULL)
			SendMessage(hwndPBRumble, PBM_SETPOS, (WPARAM)((fCurrentRumbleIntensity * 100.0) + 0.5), 0);
		else
		{
			_sntprintf(szText, _countof(szText), szRumbleIntensity, fCurrentRumbleIntensity * 100.0);
			StatusBar_SetText(hwndStatusBar, SBP_RUMBLE, szText, TRUE);
		}

		pTelemetry->Previous.Vehicle.RumbleIntensity = fCurrentRumbleIntensity;
	}

	// Brake
	if (pTelemetry->Current.Vehicle.InputIsBraking != pTelemetry->Previous.Vehicle.InputIsBraking)
	{
		StatusBar_SetText(hwndStatusBar, SBP_BRAKING, pTelemetry->Current.Vehicle.InputIsBraking ? szBraking : TEXT(""), TRUE);

		// Determine the number of braking operations with at least one wheel on the ground.
		// BUGBUG: The transition between air braking and ground contact is not detected.
		if (pTelemetry->Current.Vehicle.InputIsBraking &&
			(pTelemetry->Current.Vehicle.WheelsIsGroundContact[0] || pTelemetry->Current.Vehicle.WheelsIsGroundContact[1] ||
				pTelemetry->Current.Vehicle.WheelsIsGroundContact[2] || pTelemetry->Current.Vehicle.WheelsIsGroundContact[3]))
			nNbBrakesUsed++;

		pTelemetry->Previous.Vehicle.InputIsBraking = pTelemetry->Current.Vehicle.InputIsBraking;
	}

	// Map UID
	if (strcmp(pTelemetry->Current.Game.MapId, pTelemetry->Previous.Game.MapId) != 0)
	{
		if (strcmp(pTelemetry->Current.Game.MapId, "Unassigned") != 0)
		{
			// Clear all races after map change
			if (ListView_DeleteAllRaces(hwndListView))
				nRaceNumber = 0;

			// Reset the lap counter
			uLapCount = 0;
			StatusBar_SetLapCount(hwndStatusBar, SBP_LAPS, uLapCount, pTelemetry->Current.Race.NbLaps);
		}

		lstrcpynA(pTelemetry->Previous.Game.MapId, pTelemetry->Current.Game.MapId, _countof(pTelemetry->Previous.Game.MapId));
	}

	// Finally add race time, top speed and further statistics
	if (bAddFinalColumns)
	{
		bAddFinalColumns = FALSE;

		Nat32 uCurrentRaceTime = pTelemetry->Current.Race.Time;

		if (dwColumns & COL_RACETIME)
			ListView_AddRaceTime(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szRaceTime, uCurrentRaceTime);

		if (dwColumns & COL_TOPSPEED)
		{
			if (bMilesPerHour)
				ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szTopSpeed, szSpeedMph,
					MulDiv(uTopSpeed, 1000000, 1609344));
			else
				ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szTopSpeed, szSpeedKmh, uTopSpeed);
		}

		if (dwColumns & COL_RESPAWNS)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szRespawns, TEXT("%d"),
				pTelemetry->Current.Race.NbRespawns);

		if (dwColumns & COL_RUMBLES)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szRumbles, TEXT("%d"), nNbRumbles);

		if (dwColumns & COL_GEARCHANGES)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szGearchanges, TEXT("%d"), nNbGearchanges);

		if (dwColumns & COL_BRAKESUSED)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szBrakesUsed, TEXT("%d"), nNbBrakesUsed);

		if (dwColumns & COL_FULLSPEED)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szFullspeed, TEXT("%d %%"),
				MulDiv(100, uFullspeedTime, uCurrentRaceTime));

		if (dwColumns & COL_WHEELSLIP)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szWheelSlip, TEXT("%d %%"),
				MulDiv(100, uWheelSlipTime, uCurrentRaceTime));
	}
}

// Initialization of all used data records for the first comparison
// after program start and at the start of each game session
void InitTelemetryData(STelemetryData* pTelemetry)
{
	if (pTelemetry == NULL)
		return;

	// As a precaution, set all member variables to zero
	memset(&pTelemetry->Previous, 0, sizeof(pTelemetry->Previous));

	// Set all variables that can be zero at startup to a different value
	pTelemetry->Previous.Game.State = (STelemetry::EGameState)-1;

	pTelemetry->Previous.Race.State = (STelemetry::ERaceState)-1;
	pTelemetry->Previous.Race.Time = (Nat32)-2;	// -1 is a regular value
	pTelemetry->Previous.Race.NbRespawns = (Nat32)-2;	// -1 is a regular value
	pTelemetry->Previous.Race.NbCheckpoints = (Nat32)-1;

	pTelemetry->Previous.Vehicle.InputSteer = -2.0f;	// -1.0 is a regular value
	pTelemetry->Previous.Vehicle.InputGasPedal = -1.0f;
	pTelemetry->Previous.Vehicle.InputIsBraking = (Bool)-1;
	pTelemetry->Previous.Vehicle.EngineRpm = -1.0f;
	pTelemetry->Previous.Vehicle.EngineCurGear = -1;
	pTelemetry->Previous.Vehicle.WheelsIsSliping[0] = (Bool)-1;
	pTelemetry->Previous.Vehicle.WheelsIsSliping[1] = (Bool)-1;
	pTelemetry->Previous.Vehicle.WheelsIsSliping[2] = (Bool)-1;
	pTelemetry->Previous.Vehicle.WheelsIsSliping[3] = (Bool)-1;
	pTelemetry->Previous.Vehicle.RumbleIntensity = -1.0f;
	pTelemetry->Previous.Vehicle.SpeedMeter = (Nat32)-1;
}

// Checks if the countdown has just started (race state changes from "Finished" or "Running" to "BeforeStart")
static __inline BOOL IsRaceBeforeStart(STelemetryData* pTelemetry)
{
	return ((pTelemetry->Previous.Race.State == STelemetry::ERaceState_Finished ||
		pTelemetry->Previous.Race.State == STelemetry::ERaceState_Running) &&
		pTelemetry->Current.Race.State == STelemetry::ERaceState_BeforeState);
}

// Checks if the race has just started (race state changes from "BeforeStart" to "Running")
static __inline BOOL IsRaceRunning(STelemetryData* pTelemetry)
{
	return (pTelemetry->Previous.Race.State == STelemetry::ERaceState_BeforeState &&
		pTelemetry->Current.Race.State == STelemetry::ERaceState_Running);
}

// Checks if the race has just finished (race state changes from "Running" to "Finished" or "BeforeStart")
static __inline BOOL IsRaceFinished(STelemetryData* pTelemetry)
{
	return (pTelemetry->Previous.Race.State == STelemetry::ERaceState_Running &&
		(pTelemetry->Current.Race.State == STelemetry::ERaceState_Finished ||
			pTelemetry->Current.Race.State == STelemetry::ERaceState_BeforeState));
}
