// TMTelemetry.cpp : Defines the entry point for the application.
//
#include "stdafx.h"
#include "resource.h"
#include "tmtelemetry.h"
#include "maniaplanet_telemetry.h"

using namespace NManiaPlanet;

// Constants:
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif
#define MAX_LOADSTRING		256
#define SUSPEND_PERIOD		5	// Timer resolution in milliseconds for Sleep
#define RUMBLE_THRESHOLD	1	// Rumble intensity threshold in percent
#define FULLSPEED_THRESHOLD	85	// Full throttle threshold in percent

// String constants:
const TCHAR szMPTelemetry[] = TEXT("ManiaPlanet_Telemetry");				// File mapping object name
const TCHAR szFmtTmIfVer[] = TEXT("Telemetry Interface Version: %u");		// Format string for the About box
const TCHAR szFmtRumbleTh[] = TEXT("Rumble Intensity Threshold: %u %%");	// Format string for the About box
const TCHAR szFmtThrottleTh[] = TEXT("Full Throttle Threshold: %u %%");		// Format string for the About box
const TCHAR szWindowPlacement[] = TEXT("WindowPlacement");					// Registry value name
const TCHAR szColumns[] = TEXT("Columns");									// Registry value name
const TCHAR szAutoDelete[] = TEXT("AutoDelete");							// Registry value name
const TCHAR szSuspendPeriod[] = TEXT("SuspendPeriod");						// Registry value name
const TCHAR szRumbleThreshold[] = TEXT("RumbleIntensityThreshold");			// Registry value name
const TCHAR szFullspeedThreshold[] = TEXT("FullspeedThreshold");			// Registry value name

// Global Variables:
HINSTANCE hInst = NULL;							// Current instance
int nDpi = USER_DEFAULT_SCREEN_DPI;				// Current logical dpi
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// The main window class name
HWND hwndListView = NULL;						// List-view control handle
HWND hwndStatusBar = NULL;						// Status bar control handle
HWND hwndTBSteering = NULL;						// Trackbar control handle
HWND hwndPBThrottle = NULL;						// First progress bar control handle
HWND hwndPBRumble = NULL;						// Second progress bar control handle
Nat32 uHeaderVersion = ECurVersion;				// STelemetry header version
UINT uSuspendPeriod = SUSPEND_PERIOD;			// Time interval for Sleep
UINT uRumbleThreshold = RUMBLE_THRESHOLD;		// Rumble intensity threshold
UINT uFullspeedThreshold = FULLSPEED_THRESHOLD;	// Full throttle threshold
BOOL bAutoDelete = TRUE;						// Auto delete aborted races
BOOL bMilesPerHour = FALSE;						// Indicate mph instead of km/h
DWORD dwColumns = COL_DEFAULT;					// List-view columns to show
TCHAR szFileName[MAX_PATH];						// Data export file name

// Forward declarations of functions included in this code module:
int					DoMainLoop(void);
void				DoTelemetry(STelemetry*);
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL				WndProc_OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct);
void				WndProc_OnPaint(HWND hwnd);
BOOL				WndProc_OnEraseBkgnd(HWND hwnd, HDC hdc);
void				WndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
void				WndProc_OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos);
void				WndProc_OnInitMenuPopup(HWND hwnd, HMENU hMenu, UINT item, BOOL fSystemMenu);
LRESULT				WndProc_OnNotify(HWND hwnd, int idFrom, LPNMHDR pnmhdr);
void				WndProc_OnKey(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
void				WndProc_OnSize(HWND hwnd, UINT state, int cx, int cy);
void				WndProc_OnWinIniChange(HWND hwnd, LPCTSTR lpszSectionName);
void				WndProc_OnSysColorChange(HWND hwnd);
void				WndProc_OnDestroy(HWND hwnd);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
int CALLBACK		CompareFunc(LPARAM, LPARAM, LPARAM);


int APIENTRY _tWinMain(__in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in LPTSTR lpCmdLine, __in int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Determine system of measurement
	GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IMEASURE | LOCALE_RETURN_NUMBER,
		(LPWSTR)&bMilesPerHour, sizeof(bMilesPerHour) / sizeof(WCHAR));

	// Determine system logical DPI
	HDC hdc = GetDC(NULL);
	if (hdc != NULL)
	{
		nDpi = GetDeviceCaps(hdc, LOGPIXELSX);
		ReleaseDC(NULL, hdc);
	}

	// Load version 6 common controls and register all used controls
	INITCOMMONCONTROLSEX iccex = { 0 };
	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_PROGRESS_CLASS;
	if (!InitCommonControlsEx(&iccex))
		return 0;

	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_TMTELEMETRY, szWindowClass, MAX_LOADSTRING);
	if (!MyRegisterClass(hInstance))
		return 0;

	// Save instance handle, load settings, create main window and all child windows
	if (!InitInstance(hInstance, nCmdShow))
		return 0;

	// Acquire and dispatch messages until a WM_QUIT message is received
	return DoMainLoop();
}

// Process the main message loop
int DoMainLoop(void)
{
	MSG msg;
	HANDLE hMapFile = NULL;
	void* pBufView = NULL;
	const volatile STelemetry* Shared = NULL;

	// Set timer resolution for Sleep
	// Note: Maniaplanet and Trackmania Turbo both set the resolution globally to 1 ms
	TIMECAPS tc;
	if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR)
		uSuspendPeriod = min(max(tc.wPeriodMin, uSuspendPeriod), tc.wPeriodMax);
	BOOL bTimerBeginPeriod = (timeBeginPeriod(uSuspendPeriod) == TIMERR_NOERROR);

	HACCEL hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_TMTELEMETRY));

	// Main message loop. For the time being and the sake of simplicity, we use
	// busy waiting in this example and let the thread sleep for a fixed time.
	for (;;)
	{
		// Retrieve incoming messages and return immediately
		// so we can process the telemetry data
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			// Send all translated menu commands to the main window
			HWND hwndTranslate = msg.hwnd;
			if (hwndTranslate == hwndListView || hwndTranslate == hwndStatusBar)
				hwndTranslate = GetParent(hwndTranslate);

			if (!TranslateAccelerator(hwndTranslate, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		// Exit main loop by request of the application
		if (msg.message == WM_QUIT)
			break;

		// Get access to the telemetry data
		if (Shared == NULL)
		{
			if (hMapFile == NULL)
				hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, szMPTelemetry);

			if (hMapFile != NULL)
			{
				if (pBufView == NULL)
					pBufView = (void*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 4096);
			}

			// Show the wait state in the status bar
			static BOOL bShowWaitingMsg = TRUE;
			if (bShowWaitingMsg && pBufView == NULL && hwndStatusBar != NULL)
			{
				bShowWaitingMsg = FALSE;

				TCHAR szWaitingMsg[MAX_LOADSTRING];
				if (LoadString(hInst, IDS_WAITING, szWaitingMsg, _countof(szWaitingMsg)) > 0)
					StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szWaitingMsg);
			}

			Shared = (const STelemetry*)pBufView;
		}
		else
		{
			STelemetry S;

			for (;;)
			{
				Nat32 Before = Shared->UpdateNumber;
				memcpy(&S, (const STelemetry*)Shared, sizeof(S));
				Nat32 After = Shared->UpdateNumber;

				if (Before == After)
					break;
				else
					continue; // reading while the game is changing the values.. retry.
			}

			// Process the telemetry data
			DoTelemetry(&S);
		}

		// Suspend the busy wait loop
		Sleep(uSuspendPeriod);
	}


	// Cleanup
	if (bTimerBeginPeriod)
		timeEndPeriod(uSuspendPeriod);

	if (pBufView != NULL)
		UnmapViewOfFile(pBufView);
	if (hMapFile != NULL)
		CloseHandle(hMapFile);

	return (int)msg.wParam;
}

//
//	FUNCTION: DoTelemetry(STelemetry*)
//
//	PURPOSE: Processes the telemetry data
//
void DoTelemetry(STelemetry* pTelemetry)
{
	TCHAR szText[MAX_CONTROLTEXT];
	static int nRaceNumber = 0; // Number of attempts
	static BOOL bAddFinalColumns = FALSE; // Append some statistics after the end of the race

	// Static variables for storing values for the statistical data:
	static Nat32 uTopSpeed = 0; // Top speed per race
	static int nNbGearchanges = 0; // Number of gear changes per race
	static int nNbBrakesUsed = 0; // Number of braking operations per race
	static int nNbRumbles = 0; // Number of rumbles per race
	static BOOL bIsRumbling = FALSE; // Are we rumbling right now?
	static Nat32 uFullspeedTime = 0; // Total time at full throttle
	static Nat32 uFullspeedTimestamp = 0; // Full throttle timestamp
	static BOOL bIsFullspeed = FALSE; // Are we going full throttle right now?
	static Nat32 uWheelSlipTime = 0; // Total time of wheel slippage
	static Nat32 uWheelSlipTimestamp = 0; // Wheel slip timestamp
	static BOOL bIsSlipping = FALSE; // Is a wheel slipping right now?
	static Nat32 uMaxNbCheckpoints = 0; // Highest number of CPs per game client

	// Static variables to save the current values of the used telemetry data records:
	static Nat32 uUpdateNumber = 0;

	static STelemetry::EGameState eGameState = (STelemetry::EGameState)-1;
	static STelemetry::ERaceState eRaceState = (STelemetry::ERaceState)-1;

	static char szMapName[256] = { 0 };
	static char szMapId[64] = { 0 };
	static char szGameplayVariant[64] = { 0 };

	static Nat32 uRaceTime = (Nat32)-2;	// -1 is a regular value
	static Nat32 uRaceNbRespawns = (Nat32)-2;	// -1 is a regular value
	static Nat32 uRaceNbCheckpoints = (Nat32)-1;

	static Nat32 uVehicleSpeedMeter = (Nat32)-1;
	static int nVehicleEngineCurGear = -1;
	static float fVehicleEngineRpm = -1.0f;
	static float fVehicleInputSteer = -2.0f;	// -1.0 is a regular value
	static float fVehicleInputGasPedal = -1.0f;
	static float fVehicleRumbleIntensity = -1.0f;
	static Bool bVehicleInputIsBraking = (Bool)-1;
	static Bool aWheelsIsSliping[4] = { (Bool)-1, (Bool)-1, (Bool)-1, (Bool)-1 };

	// Test for updated telemetry data records
	if (pTelemetry->UpdateNumber == uUpdateNumber)
		return;

	uUpdateNumber = pTelemetry->UpdateNumber;

	// STelemetry header version; will be displayed in the About box
	if (pTelemetry->Header.Version != uHeaderVersion)
		uHeaderVersion = pTelemetry->Header.Version;

	// The provided telemetry data records are not cleared after a race ends.
	// To not display wrong or unusable values, we clean at least the status
	// bar parts while the game is in Menu state.
	if (pTelemetry->Game.State == STelemetry::EState_Menus)
	{
		// We need to reset our saved values so that the start of
		// a new race will also show the data that has not changed:
		eGameState = pTelemetry->Game.State;
		eRaceState = (STelemetry::ERaceState)-1;

		szMapName[0] = '\0';
		szMapId[0] = '\0';
		szGameplayVariant[0] = '\0';

		uRaceTime = (Nat32)-2;	// -1 is a regular value
		uRaceNbRespawns = (Nat32)-2;	// -1 is a regular value
		uRaceNbCheckpoints = (Nat32)-1;

		uVehicleSpeedMeter = (Nat32)-1;
		nVehicleEngineCurGear = -1;
		fVehicleEngineRpm = -1.0f;
		fVehicleInputSteer = -2.0f;	// -1.0 is a regular value
		fVehicleInputGasPedal = -1.0f;
		fVehicleRumbleIntensity = -1.0f;
		bVehicleInputIsBraking = (Bool)-1;

		// Clear the parts of the status bar that contain race data
		StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGameMenus);
		for (int i = SBP_RACESTATE; i <= SBP_BRAKING; i++)
			StatusBar_SetText(hwndStatusBar, i, TEXT(""));

		if (hwndTBSteering != NULL)
		{
			SendMessage(hwndTBSteering, TBM_CLEARSEL, (WPARAM)TRUE, 0);
			SendMessage(hwndTBSteering, TBM_SETPOS, (WPARAM)TRUE, 0);
		}
		if (hwndPBThrottle != NULL)
			SendMessage(hwndPBThrottle, PBM_SETPOS, 0, 0);
		if (hwndPBRumble != NULL)
			SendMessage(hwndPBRumble, PBM_SETPOS, 0, 0);

		return;	// Nothing to monitor
	}

	// Check whether a used data element has changed and update the
	// respective status bar and list-view panes with the new values

	// Game state
	if (pTelemetry->Game.State != eGameState)
	{
		eGameState = pTelemetry->Game.State;

		switch (eGameState)
		{
			case STelemetry::EState_Starting:
				StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGameStarting);
				break;
			case STelemetry::EState_Menus:
				StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGameMenus);
				break;
			case STelemetry::EState_Running:
				StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGameRunning);
				break;
			case STelemetry::EState_Paused:
				StatusBar_SetText(hwndStatusBar, SBP_GAMESTATE, szGamePaused);
				break;
		}
	}

	// Race state
	if (pTelemetry->Race.State != eRaceState)
	{
		// Handle the start of a race (Race state changes from "BeforeState" to "Running")
		if (eRaceState == STelemetry::ERaceState_BeforeState && pTelemetry->Race.State == STelemetry::ERaceState_Running)
		{
			uTopSpeed = 0;
			nNbGearchanges = 0;
			nNbBrakesUsed = 0;
			bIsRumbling = pTelemetry->Vehicle.RumbleIntensity > uRumbleThreshold / 100.0f;
			nNbRumbles = bIsRumbling ? 1 : 0;
			bIsFullspeed = pTelemetry->Vehicle.InputGasPedal >= uFullspeedThreshold / 100.0f;
			uFullspeedTime = 0;
			uFullspeedTimestamp = pTelemetry->Vehicle.Timestamp;
			bIsSlipping = pTelemetry->Vehicle.WheelsIsSliping[0] || pTelemetry->Vehicle.WheelsIsSliping[1] ||
				pTelemetry->Vehicle.WheelsIsSliping[2] || pTelemetry->Vehicle.WheelsIsSliping[3];
			uWheelSlipTime = 0;
			uWheelSlipTimestamp = pTelemetry->Vehicle.Timestamp;
			uMaxNbCheckpoints = 0;

			// Add a new race to the list-view control and increment the number of attempts
			if (ListView_AddRace(hwndListView, nRaceNumber + 1, COLUMN_AUTOFIT) != -1)
				nRaceNumber++;

			// Optionally add map UID, map name, player model, date and time to each race
			if (dwColumns & COL_MAPUID)
			{
				MultiByteToWideChar(CP_UTF8, 0, pTelemetry->Game.MapId, -1, szText, _countof(szText));
				ListView_AddRaceText(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szMapIdentifier, szText);
			}
			if (dwColumns & COL_MAPNAME)
			{
				MultiByteToWideChar(CP_UTF8, 0, pTelemetry->Game.MapName, -1, szText, _countof(szText));
				ListView_AddRaceText(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szTrackName, szText);
			}
			if (dwColumns & COL_PLAYERMODEL)
			{
				MultiByteToWideChar(CP_UTF8, 0, pTelemetry->Game.GameplayVariant, -1, szText, _countof(szText));
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

		// Handle the end of a race (Race state changes from "Running" to "Finished" or "BeforeStart")
		if (eRaceState == STelemetry::ERaceState_Running && (pTelemetry->Race.State == STelemetry::ERaceState_Finished ||
			pTelemetry->Race.State == STelemetry::ERaceState_BeforeState))
		{
			// Test if the checkpoint count has increased to differ between finished and restart
			if (pTelemetry->Race.NbCheckpoints > uRaceNbCheckpoints)
			{
				bAddFinalColumns = TRUE;

				if (bIsFullspeed)
					uFullspeedTime += pTelemetry->Vehicle.Timestamp - uFullspeedTimestamp;
				if (bIsSlipping)
					uWheelSlipTime += pTelemetry->Vehicle.Timestamp - uWheelSlipTimestamp;
			}
			else if (bAutoDelete && ListView_DeleteRace(hwndListView, nRaceNumber) && nRaceNumber > 0)
				nRaceNumber--;
		}

		eRaceState = pTelemetry->Race.State;

		switch (eRaceState)
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
		}
	}

	// Map name
	if (strcmp(pTelemetry->Game.MapName, szMapName) != 0)
	{
		strncpy(szMapName, pTelemetry->Game.MapName, _countof(szMapName));

		// Convert the received UTF-8 string to Unicode UTF-16
		MultiByteToWideChar(CP_UTF8, 0, szMapName, -1, szText, _countof(szText));
		// If a new map name exists, update the file name for data export
		if (_tcslen(szText) > 0)
			lstrcpyn(szFileName, szText, _countof(szFileName));

		StatusBar_SetText(hwndStatusBar, SBP_MAPNAME, szText);
	}

	// Player model
	if (strcmp(pTelemetry->Game.GameplayVariant, szGameplayVariant) != 0)
	{
		strncpy(szGameplayVariant, pTelemetry->Game.GameplayVariant, _countof(szGameplayVariant));

		MultiByteToWideChar(CP_UTF8, 0, szGameplayVariant, -1, szText, _countof(szText));
		StatusBar_SetText(hwndStatusBar, SBP_PLAYERMODEL, szText, TRUE);
	}

	// Race time
	if (pTelemetry->Race.Time != uRaceTime)
	{
		uRaceTime = pTelemetry->Race.Time;

		FormatTime(szText, _countof(szText), uRaceTime);
		StatusBar_SetText(hwndStatusBar, SBP_RACETIME, szText, TRUE);
	}

	// Number of respawns
	if (pTelemetry->Race.NbRespawns != uRaceNbRespawns)
	{
		uRaceNbRespawns = pTelemetry->Race.NbRespawns;

		_sntprintf(szText, _countof(szText), szNbRespawns, uRaceNbRespawns);
		StatusBar_SetText(hwndStatusBar, SBP_RESPAWNS, szText, TRUE);
	}

	// Number of checkpoints
	if (pTelemetry->Race.NbCheckpoints != uRaceNbCheckpoints)
	{
		// Add the new checkpoint time to the list-view control
		Nat32 uNewNbCheckpoints = pTelemetry->Race.NbCheckpoints;
		if (uNewNbCheckpoints > 0 && uNewNbCheckpoints <= _countof(pTelemetry->Race.CheckpointTimes) &&
			uNewNbCheckpoints > uMaxNbCheckpoints)
		{
			// BUGBUG: It looks like we can't be sure that the checkpoint time was also already updated!
			if (dwColumns & COL_SECTORTIMES)
				ListView_AddSectorTime(hwndListView, nRaceNumber, COLUMN_AUTOFIT, uNewNbCheckpoints,
				pTelemetry->Race.CheckpointTimes[uNewNbCheckpoints - 1],
				uNewNbCheckpoints >= 2 ? pTelemetry->Race.CheckpointTimes[uNewNbCheckpoints - 2] : 0);

			if (dwColumns & COL_CHECKPOINTS)
				ListView_AddCheckpointTime(hwndListView, nRaceNumber, COLUMN_AUTOFIT, uNewNbCheckpoints,
				pTelemetry->Race.CheckpointTimes[uNewNbCheckpoints - 1]);
		}

		// Here we have to store the highest number of checkpoints per game client so that
		// the list is not flooded with data when Maniaplanet and Turbo run simultaneously
		if (uNewNbCheckpoints > uRaceNbCheckpoints)
			uMaxNbCheckpoints = uNewNbCheckpoints;

		uRaceNbCheckpoints = uNewNbCheckpoints;

		_sntprintf(szText, _countof(szText), szNbCheckpoints, uRaceNbCheckpoints);
		StatusBar_SetText(hwndStatusBar, SBP_CHECKPOINTS, szText, TRUE);
	}

	// Speed
	if (pTelemetry->Vehicle.SpeedMeter != uVehicleSpeedMeter)
	{
		uVehicleSpeedMeter = pTelemetry->Vehicle.SpeedMeter;

		if (bMilesPerHour)
			_sntprintf(szText, _countof(szText), szSpeedMph, MulDiv(uVehicleSpeedMeter, 1000000, 1609344));
		else
			_sntprintf(szText, _countof(szText), szSpeedKmh, uVehicleSpeedMeter);
		StatusBar_SetText(hwndStatusBar, SBP_SPEEDMETER, szText, TRUE);

		if (uVehicleSpeedMeter > uTopSpeed)
			uTopSpeed = uVehicleSpeedMeter;
	}

	// Revolutions per minute
	if (pTelemetry->Vehicle.EngineRpm != fVehicleEngineRpm)
	{
		fVehicleEngineRpm = pTelemetry->Vehicle.EngineRpm;

		_sntprintf(szText, _countof(szText), szEngineRpm, fVehicleEngineRpm);
		StatusBar_SetText(hwndStatusBar, SBP_ENGINERPM, szText, TRUE);
	}

	// Gear
	if (pTelemetry->Vehicle.EngineCurGear != nVehicleEngineCurGear)
	{
		nVehicleEngineCurGear = pTelemetry->Vehicle.EngineCurGear;

		_sntprintf(szText, _countof(szText), szEngineCurGear, nVehicleEngineCurGear);
		StatusBar_SetText(hwndStatusBar, SBP_CURGEAR, szText, TRUE);

		nNbGearchanges++;
	}

	// Steering
	if (pTelemetry->Vehicle.InputSteer != fVehicleInputSteer)
	{
		fVehicleInputSteer = pTelemetry->Vehicle.InputSteer;

		if (hwndTBSteering != NULL)
		{
			LPARAM lPos = (LPARAM)(fVehicleInputSteer * 100.0);
			SendMessage(hwndTBSteering, TBM_SETSELSTART, (WPARAM)TRUE, fVehicleInputSteer < 0.0f ? lPos : 0);
			SendMessage(hwndTBSteering, TBM_SETSELEND, (WPARAM)TRUE, fVehicleInputSteer < 0.0f ? 0 : lPos);
			SendMessage(hwndTBSteering, TBM_SETPOS, (WPARAM)TRUE, lPos);
		}
		else
		{
			_sntprintf(szText, _countof(szText), szSteering, fVehicleInputSteer * 100.0);
			StatusBar_SetText(hwndStatusBar, SBP_STEERING, szText, TRUE);
		}
	}

	// Gas pedal
	if (pTelemetry->Vehicle.InputGasPedal != fVehicleInputGasPedal)
	{
		fVehicleInputGasPedal = pTelemetry->Vehicle.InputGasPedal;

		if (hwndPBThrottle != NULL)
			SendMessage(hwndPBThrottle, PBM_SETPOS, (WPARAM)((fVehicleInputGasPedal * 100.0) + 0.5), 0);
		else
		{
			_sntprintf(szText, _countof(szText), szGasPedal, fVehicleInputGasPedal * 100.0);
			StatusBar_SetText(hwndStatusBar, SBP_THROTTLE, szText, TRUE);
		}

		// Determine full throttle percentage per race. The default threshold of 0.85 does
		// not take into account releasing the throttle during shifting (powershifting)
		if (fVehicleInputGasPedal >= uFullspeedThreshold / 100.0f)
		{
			if (!bIsFullspeed)
			{
				bIsFullspeed = TRUE;
				uFullspeedTimestamp = pTelemetry->Vehicle.Timestamp;
			}
		}
		else
		{
			if (bIsFullspeed)
			{
				bIsFullspeed = FALSE;
				uFullspeedTime += pTelemetry->Vehicle.Timestamp - uFullspeedTimestamp;
			}
		}
	}

	// Wheel slip
	if (memcmp(&pTelemetry->Vehicle.WheelsIsSliping, &aWheelsIsSliping, sizeof(aWheelsIsSliping)) != 0)
	{
		memcpy(&aWheelsIsSliping, &pTelemetry->Vehicle.WheelsIsSliping, sizeof(aWheelsIsSliping));

		// Determine the percentage of time per race in which at least one wheel is slipping.
		if (aWheelsIsSliping[0] || aWheelsIsSliping[1] || aWheelsIsSliping[2] || aWheelsIsSliping[3])
		{
			if (!bIsSlipping)
			{
				bIsSlipping = TRUE;
				uWheelSlipTimestamp = pTelemetry->Vehicle.Timestamp;
			}
		}
		else
		{
			if (bIsSlipping)
			{
				bIsSlipping = FALSE;
				uWheelSlipTime += pTelemetry->Vehicle.Timestamp - uWheelSlipTimestamp;
			}
		}
	}

	// Rumble intensity
	if (pTelemetry->Vehicle.RumbleIntensity != fVehicleRumbleIntensity)
	{
		fVehicleRumbleIntensity = pTelemetry->Vehicle.RumbleIntensity;

		if (hwndPBRumble != NULL)
			SendMessage(hwndPBRumble, PBM_SETPOS, (WPARAM)((fVehicleRumbleIntensity * 100.0) + 0.5), 0);
		else
		{
			_sntprintf(szText, _countof(szText), szRumbleIntensity, fVehicleRumbleIntensity * 100.0);
			StatusBar_SetText(hwndStatusBar, SBP_RUMBLE, szText, TRUE);
		}

		// Determine number of rumbles per race
		if (fVehicleRumbleIntensity > uRumbleThreshold / 100.0f)
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
	}

	// Brake
	if (pTelemetry->Vehicle.InputIsBraking != bVehicleInputIsBraking)
	{
		bVehicleInputIsBraking = pTelemetry->Vehicle.InputIsBraking;

		StatusBar_SetText(hwndStatusBar, SBP_BRAKING, bVehicleInputIsBraking ? szBraking : TEXT(""), TRUE);

		// Determine the number of braking operations with at least one wheel on the ground.
		// BUGBUG: The transition between air braking and ground contact is not detected.
		if (bVehicleInputIsBraking && (pTelemetry->Vehicle.WheelsIsGroundContact[0] || pTelemetry->Vehicle.WheelsIsGroundContact[1] ||
			pTelemetry->Vehicle.WheelsIsGroundContact[2] || pTelemetry->Vehicle.WheelsIsGroundContact[3]))
			nNbBrakesUsed++;
	}

	// Map UID
	if (strcmp(pTelemetry->Game.MapId, szMapId) != 0)
	{
		lstrcpynA(szMapId, pTelemetry->Game.MapId, _countof(szMapId));

		// Clear all races after map changes
		if (strcmp(szMapId, "Unassigned") != 0)
			if (ListView_DeleteAllRaces(hwndListView))
				nRaceNumber = 0;
	}

	// Finally add race time, top speed and further statistics
	if (bAddFinalColumns)
	{
		bAddFinalColumns = FALSE;

		if (dwColumns & COL_RACETIME)
			ListView_AddRaceTime(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szRaceTime, uRaceTime);

		if (dwColumns & COL_TOPSPEED)
		{
			if (bMilesPerHour)
				ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szTopSpeed, szSpeedMph,
					MulDiv(uTopSpeed, 1000000, 1609344));
			else
				ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szTopSpeed, szSpeedKmh, uTopSpeed);
		}

		if (dwColumns & COL_RESPAWNS)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szRespawns, TEXT("%d"), uRaceNbRespawns);

		if (dwColumns & COL_RUMBLES)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szRumbles, TEXT("%d"), nNbRumbles);

		if (dwColumns & COL_GEARCHANGES)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szGearchanges, TEXT("%d"), nNbGearchanges);

		if (dwColumns & COL_BRAKESUSED)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szBrakesUsed, TEXT("%d"), nNbBrakesUsed);

		if (dwColumns & COL_FULLSPEED)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szFullspeed, TEXT("%d %%"), MulDiv(100, uFullspeedTime, uRaceTime));

		if (dwColumns & COL_WHEELSLIP)
			ListView_AddRaceData(hwndListView, nRaceNumber, COLUMN_AUTOFIT, szWheelSlip, TEXT("%d %%"), MulDiv(100, uWheelSlipTime, uRaceTime));
	}
}

//
//	FUNCTION: MyRegisterClass()
//
//	PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TMTELEMETRY));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCE(IDC_TMTELEMETRY);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_SMALL), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

	return RegisterClassEx(&wcex);
}

//
//	FUNCTION: InitInstance(HINSTANCE, int)
//
//	PURPOSE: Saves instance handle and creates main window
//
//	COMMENTS:
//		In this function, we save the instance handle in a global variable and
//		create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	// Store instance handle in our global variable
	hInst = hInstance;

	BOOL bUseDefaultWindowPos = TRUE;
	WINDOWPLACEMENT wpl = { 0 };
	wpl.length = sizeof(wpl);
	wpl.showCmd = nCmdShow;
	SetRect(&wpl.rcNormalPosition, 40, 40, 600, 440);

	// Load settings
	TCHAR szRegPath[MAX_LOADSTRING];
	if (LoadString(hInstance, IDS_REGPATH, szRegPath, _countof(szRegPath)) > 0)
	{
		HKEY hKey = NULL;
		LONG lStatus = RegOpenKeyEx(HKEY_CURRENT_USER, szRegPath, 0, KEY_READ, &hKey);
		if (lStatus == ERROR_SUCCESS && hKey != NULL)
		{
			// Window position
			DWORD dwSize = sizeof(wpl);
			DWORD dwType = REG_BINARY;
			lStatus = RegQueryValueEx(hKey, szWindowPlacement, NULL, &dwType, (LPBYTE)&wpl, &dwSize);
			if (lStatus == ERROR_SUCCESS && dwSize == sizeof(WINDOWPLACEMENT))
			{
				bUseDefaultWindowPos = FALSE;
				wpl.length = sizeof(wpl);
				wpl.showCmd = nCmdShow;

				// SetWindowPlacement doesn't adjust the coordinates when the window was
				// placed side-by-side on a now disabled monitor. Therefore, we will check
				// whether the stored window position is outside the current virtual screen:
				RECT rcDst, rcVirtual;
				rcVirtual.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
				rcVirtual.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
				rcVirtual.right = rcVirtual.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
				rcVirtual.bottom = rcVirtual.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
				if (!IntersectRect(&rcDst, &rcVirtual, &wpl.rcNormalPosition))
					bUseDefaultWindowPos = TRUE;
			}

			// List-view columns to show
			DWORD dwValue = dwColumns;
			dwSize = sizeof(dwValue);
			dwType = REG_DWORD;
			lStatus = RegQueryValueEx(hKey, szColumns, NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
			if (lStatus == ERROR_SUCCESS)
				dwColumns = dwValue;

			// Auto remove uncompleted races
			dwValue = (DWORD)bAutoDelete;
			dwSize = sizeof(dwValue);
			dwType = REG_DWORD;
			lStatus = RegQueryValueEx(hKey, szAutoDelete, NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
			if (lStatus == ERROR_SUCCESS)
				bAutoDelete = !!(BOOL)dwValue;

			// Suspend time interval
			dwValue = (DWORD)uSuspendPeriod;
			dwSize = sizeof(dwValue);
			dwType = REG_DWORD;
			lStatus = RegQueryValueEx(hKey, szSuspendPeriod, NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
			if (lStatus == ERROR_SUCCESS && dwValue >= 1 && dwValue <= 100)
				uSuspendPeriod = (UINT)dwValue;

			// Rumble intensity threshold
			dwValue = (DWORD)uRumbleThreshold;
			dwSize = sizeof(dwValue);
			dwType = REG_DWORD;
			lStatus = RegQueryValueEx(hKey, szRumbleThreshold, NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
			if (lStatus == ERROR_SUCCESS && dwValue >= 0 && dwValue <= 100)
				uRumbleThreshold = (UINT)dwValue;

			// Full throttle threshold
			dwValue = (DWORD)uFullspeedThreshold;
			dwSize = sizeof(dwValue);
			dwType = REG_DWORD;
			lStatus = RegQueryValueEx(hKey, szFullspeedThreshold, NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
			if (lStatus == ERROR_SUCCESS && dwValue >= 0 && dwValue <= 100)
				uFullspeedThreshold = (UINT)dwValue;

			RegCloseKey(hKey);
		}
	}

	// Create main window
	HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (hWnd == NULL)
		return FALSE;

	if (bUseDefaultWindowPos || !SetWindowPlacement(hWnd, &wpl))
	{	// Note: SetWindowPlacement already does a ShowWindow/UpdateWindow
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
	}

	return TRUE;
}

//
//	FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//	PURPOSE:  Processes messages for the main window.
//
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		HANDLE_MSG(hwnd, WM_CREATE, WndProc_OnCreate);
		HANDLE_MSG(hwnd, WM_PAINT, WndProc_OnPaint);
		HANDLE_MSG(hwnd, WM_ERASEBKGND, WndProc_OnEraseBkgnd);
		HANDLE_MSG(hwnd, WM_COMMAND, WndProc_OnCommand);
		HANDLE_MSG(hwnd, WM_CONTEXTMENU, WndProc_OnContextMenu);
		HANDLE_MSG(hwnd, WM_INITMENUPOPUP, WndProc_OnInitMenuPopup);
		HANDLE_MSG(hwnd, WM_NOTIFY, WndProc_OnNotify);
		HANDLE_MSG(hwnd, WM_KEYDOWN, WndProc_OnKey);
		HANDLE_MSG(hwnd, WM_SIZE, WndProc_OnSize);
		HANDLE_MSG(hwnd, WM_WININICHANGE, WndProc_OnWinIniChange);
		HANDLE_MSG(hwnd, WM_SYSCOLORCHANGE, WndProc_OnSysColorChange);
		HANDLE_MSG(hwnd, WM_DESTROY, WndProc_OnDestroy);

		case WM_DPICHANGED:
		{
			LPCRECT lprc = (LPCRECT)lParam;
			SetWindowPos(hwnd, HWND_TOP, lprc->left, lprc->top, lprc->right - lprc->left, lprc->bottom - lprc->top,
				SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
			break;
		}

		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

BOOL WndProc_OnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct)
{
	// Create the list-view control
	if ((hwndListView = CreateWindow(WC_LISTVIEW, TEXT(""),
		WS_CHILD | WS_TABSTOP | WS_VISIBLE | LVS_REPORT | LVS_AUTOARRANGE | LVS_SHOWSELALWAYS | LVS_EDITLABELS,
		0, 0, 0, 0, hwnd, (HMENU)ID_LISTVIEW, lpCreateStruct->hInstance, NULL)) == NULL)
		return FALSE;

	// Set extended list-view styles
	DWORD dwExStyle = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP;
	ListView_SetExtendedListViewStyleEx(hwndListView, dwExStyle, dwExStyle);

	// Set list-view font
	SetWindowFont(hwndListView, GetStockFont(DEFAULT_GUI_FONT), FALSE);

	// Determine the image size for the image list
	int cx = GetSystemMetrics(SM_CXSMICON);
	int cy = GetSystemMetrics(SM_CYSMICON);
	if (cx < 24) cx = 16; else if (cx < 32) cx = 24; else cx = 32;
	if (cy < 24) cy = 16; else if (cy < 32) cy = 24; else cy = 32;

	// Create image list
	HIMAGELIST himl = ImageList_Create(cx, cy, ILC_COLORDDB | ILC_MASK, 1, 0);
	if (himl != NULL)
	{
		HICON hIcon = (HICON)LoadImage(lpCreateStruct->hInstance, MAKEINTRESOURCE(IDI_SMALL),
			IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR);
		if (hIcon != NULL)
		{
			if (ImageList_AddIcon(himl, hIcon) != -1)
				ListView_SetImageList(hwndListView, himl, LVSIL_SMALL);

			// We can release the icon immediately because ImageList_AddIcon made a copy of it
			DestroyIcon(hIcon);
		}
	}

	// Create the status bar control
	if ((hwndStatusBar = CreateWindow(STATUSCLASSNAME, TEXT(""),
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | SBARS_TOOLTIPS | SBARS_SIZEGRIP,
		0, 0, 0, 0, hwnd, (HMENU)ID_STATUSBAR, lpCreateStruct->hInstance, NULL)) == NULL)
		return FALSE;	// It's not necessary to destroy all child windows already created

	SetWindowFont(hwndStatusBar, GetStockFont(DEFAULT_GUI_FONT), FALSE);

	// Set the number of parts and the coordinate of the right edge of each part
	int aStatusBarParts[15] = { 125, 235, 385, 480, 555, 605, 650, 715, 790, 850, 940, 1030, 1120, 1180, -1 };
	SIZE_T uParts = _countof(aStatusBarParts);
	for (SIZE_T i = 0; i < (uParts - 1); i++)
		aStatusBarParts[i] = MulDiv(aStatusBarParts[i], nDpi, USER_DEFAULT_SCREEN_DPI);
	SendMessage(hwndStatusBar, SB_SETPARTS, uParts, (LPARAM)&aStatusBarParts);

	// Create trackbar and progress bar controls for steering, throttle and rumble.
	// These controls are optional. If an error occurs, the parent window should still be created.
	if ((hwndTBSteering = CreateWindow(TRACKBAR_CLASS, TEXT(""),
		WS_CHILD | WS_VISIBLE | WS_DISABLED | TBS_HORZ | TBS_BOTH | TBS_NOTICKS | TBS_ENABLESELRANGE | TBS_FIXEDLENGTH,
		0, 0, 0, 0, hwndStatusBar, (HMENU)ID_TRACKBAR, lpCreateStruct->hInstance, NULL)) != NULL)
	{
		SendMessage(hwndTBSteering, TBM_SETRANGEMIN, (WPARAM)FALSE, -100);
		SendMessage(hwndTBSteering, TBM_SETRANGEMAX, (WPARAM)FALSE, 100);
		SendMessage(hwndTBSteering, TBM_CLEARSEL, (WPARAM)FALSE, 0);
		SendMessage(hwndTBSteering, TBM_SETPOS, (WPARAM)FALSE, 0);
	}

	if ((hwndPBThrottle = CreateWindow(PROGRESS_CLASS, TEXT(""), WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
		0, 0, 0, 0, hwndStatusBar, (HMENU)ID_PROGRESS1, lpCreateStruct->hInstance, NULL)) != NULL)
	{
		// Disable visual styles to show a flat progress bar
		SetWindowTheme(hwndPBThrottle, L"", L"");
		SendMessage(hwndPBThrottle, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
	}

	if ((hwndPBRumble = CreateWindow(PROGRESS_CLASS, TEXT(""), WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
		0, 0, 0, 0, hwndStatusBar, (HMENU)ID_PROGRESS2, lpCreateStruct->hInstance, NULL)) != NULL)
	{
		SetWindowTheme(hwndPBRumble, L"", L"");
		SendMessage(hwndPBRumble, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
		// Use a red progress indicator bar for rumble
		SendMessage(hwndPBRumble, PBM_SETBARCOLOR, 0, RGB(255, 51, 51));
	}

	return TRUE;
}

void WndProc_OnPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	RECT rcPaint = ps.rcPaint;

	// TODO: Draw line graphs of gear, rpm, speed...

	EndPaint(hwnd, &ps);
}

BOOL WndProc_OnEraseBkgnd(HWND hwnd, HDC hdc)
{
	// Eat this message to avoid erasing portions that we are going to repaint
	// in WM_PAINT or are covered by the list-view and status bar controls
	return TRUE;
}

void WndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id)
	{
		case ID_FILE_EXPORT:
		{
			static DWORD dwFilterIndex = FI_TEXT;
			if (GetFileName(hwnd, szFileName, _countof(szFileName), &dwFilterIndex, TRUE))
			{
				HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
				ListView_SaveAllItems(hwndListView, szFileName, dwFilterIndex == FI_CSV);
				SetCursor(hOldCursor);
			}
			break;
		}

		case IDM_EXIT:
			DestroyWindow(hwnd);
			break;

		case ID_EDIT_DELETE:
			ListView_DeleteSelectedItems(hwndListView);
			break;

		case ID_EDIT_DELETEALL:
			ListView_DeleteAllRaces(hwndListView);
			break;

		case ID_EDIT_RENAME:
			ListView_RenameSelectedItem(hwndListView);
			break;

		case ID_EDIT_SELECTALL:
			ListView_SelectAll(hwndListView);
			break;

		case ID_EDIT_INVERTSELECTION:
			ListView_InvertSelection(hwndListView);
			break;

		case ID_EDIT_AUTODELETE:
			bAutoDelete = !bAutoDelete;
			break;

		case ID_VIEW_DATE:
			dwColumns ^= COL_DATE;
			break;

		case ID_VIEW_TIME:
			dwColumns ^= COL_TIME;
			break;

		case ID_VIEW_MAPUID:
			dwColumns ^= COL_MAPUID;
			break;

		case ID_VIEW_MAPNAME:
			dwColumns ^= COL_MAPNAME;
			break;

		case ID_VIEW_PLAYERMODEL:
			dwColumns ^= COL_PLAYERMODEL;
			break;

		case ID_VIEW_SECTORTIMES:
			dwColumns ^= COL_SECTORTIMES;
			break;

		case ID_VIEW_CHECKPOINTS:
			dwColumns ^= COL_CHECKPOINTS;
			break;

		case ID_VIEW_RACETIME:
			dwColumns ^= COL_RACETIME;
			break;

		case ID_VIEW_TOPSPEED:
			dwColumns ^= COL_TOPSPEED;
			break;

		case ID_VIEW_RESPAWNS:
			dwColumns ^= COL_RESPAWNS;
			break;

		case ID_VIEW_RUMBLES:
			dwColumns ^= COL_RUMBLES;
			break;

		case ID_VIEW_GEARCHANGES:
			dwColumns ^= COL_GEARCHANGES;
			break;

		case ID_VIEW_BRAKESUSED:
			dwColumns ^= COL_BRAKESUSED;
			break;

		case ID_VIEW_FULLSPEED:
			dwColumns ^= COL_FULLSPEED;
			break;

		case ID_VIEW_WHEELSLIP:
			dwColumns ^= COL_WHEELSLIP;
			break;

		case ID_VIEW_AUTOFITCOLUMNS:
			ListView_AutoSizeAllColumns(hwndListView);
			break;

		case IDM_ABOUT:
			PlaySound((LPCTSTR)SND_ALIAS_SYSTEMASTERISK, NULL, SND_ALIAS_ID | SND_ASYNC);
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hwnd, About);
			break;
	}
}

void WndProc_OnContextMenu(HWND hwnd, HWND hwndContext, UINT xPos, UINT yPos)
{
	if (hwndContext != hwndListView)
		return;

	// Change the values from unsigned to signed
	int x = (int)(short)LOWORD(xPos);
	int y = (int)(short)LOWORD(yPos);

	RECT rcHeader = { 0 };
	GetWindowRect(ListView_GetHeader(hwndListView), &rcHeader);

	POINT pt = { x, y };
	int nPos = 1;	// Edit menu
	if (PtInRect(&rcHeader, pt))
		nPos = 2;	// View menu

	if (x == -1 || y == -1)
	{	// If the context menu is generated from the keyboard,
		// display it at the location of the current selection
		LPARAM point = ListView_GetContextMenuPoint(hwndListView);
		x = GET_X_LPARAM(point);
		y = GET_Y_LPARAM(point);
	}

	HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDC_TMTELEMETRY));
	if (hMenu == NULL)
		return;

	HMENU hSubMenu = GetSubMenu(hMenu, nPos);
	if (hSubMenu == NULL)
	{
		DestroyMenu(hMenu);
		return;
	}

	WndProc_OnInitMenuPopup(hwnd, hSubMenu, nPos, FALSE);
	TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, x, y, 0, hwnd, NULL);

	DestroyMenu(hMenu);
}

void WndProc_OnInitMenuPopup(HWND hwnd, HMENU hMenu, UINT item, BOOL fSystemMenu)
{
	UINT uFlags;

	if (fSystemMenu)
		return;	// Window menu

	switch (item)
	{
		case 0: // File menu
			uFlags = (ListView_GetItemCount(hwndListView) > 0) ? MF_ENABLED : MF_DISABLED | MF_GRAYED;
			EnableMenuItem(hMenu, ID_FILE_EXPORT, uFlags);
			break;

		case 1: // Edit menu
			uFlags = (ListView_GetSelectedCount(hwndListView) > 0) ? MF_ENABLED : MF_DISABLED | MF_GRAYED;
			EnableMenuItem(hMenu, ID_EDIT_DELETE, uFlags);

			uFlags = (ListView_GetColumnCount(hwndListView) > 0) ? MF_ENABLED : MF_DISABLED | MF_GRAYED;
			EnableMenuItem(hMenu, ID_EDIT_DELETEALL, uFlags);

			uFlags = (ListView_GetSelectedCount(hwndListView) == 1) ? MF_ENABLED : MF_DISABLED | MF_GRAYED;
			EnableMenuItem(hMenu, ID_EDIT_RENAME, uFlags);

			uFlags = (ListView_GetItemCount(hwndListView) > 0) ? MF_ENABLED : MF_DISABLED | MF_GRAYED;
			EnableMenuItem(hMenu, ID_EDIT_SELECTALL, uFlags);
			EnableMenuItem(hMenu, ID_EDIT_INVERTSELECTION, uFlags);

			uFlags = bAutoDelete ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_EDIT_AUTODELETE, MF_BYCOMMAND | uFlags);
			break;

		case 2: // View menu
			uFlags = (dwColumns & COL_DATE) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_DATE, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_TIME) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_TIME, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_MAPUID) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_MAPUID, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_MAPNAME) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_MAPNAME, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_PLAYERMODEL) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_PLAYERMODEL, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_SECTORTIMES) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_SECTORTIMES, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_CHECKPOINTS) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_CHECKPOINTS, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_RACETIME) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_RACETIME, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_TOPSPEED) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_TOPSPEED, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_RESPAWNS) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_RESPAWNS, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_RUMBLES) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_RUMBLES, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_GEARCHANGES) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_GEARCHANGES, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_BRAKESUSED) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_BRAKESUSED, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_FULLSPEED) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_FULLSPEED, MF_BYCOMMAND | uFlags);

			uFlags = (dwColumns & COL_WHEELSLIP) ? MF_CHECKED : MF_UNCHECKED;
			CheckMenuItem(hMenu, ID_VIEW_WHEELSLIP, MF_BYCOMMAND | uFlags);

			uFlags = (ListView_GetColumnCount(hwndListView) > 0) ? MF_ENABLED : MF_DISABLED | MF_GRAYED;
			EnableMenuItem(hMenu, ID_VIEW_AUTOFITCOLUMNS, uFlags);
			break;

		case 3: // Help menu
			break;
	}
}

LRESULT WndProc_OnNotify(HWND hwnd, int idFrom, LPNMHDR pnmhdr)
{
	if (pnmhdr == NULL)
		return 0;

	if (pnmhdr->idFrom == ID_LISTVIEW)
	{
		switch (pnmhdr->code)
		{
			case LVN_GETEMPTYMARKUP:
			{
				// You can define a link with the <a> tag
				NMLVEMPTYMARKUP* pnmMarkup = (NMLVEMPTYMARKUP*)pnmhdr;
				return (LoadString(hInst, IDS_EMPTYMARKUP, pnmMarkup->szMarkup, L_MAX_URL_LENGTH - 1) > 0);
			}

			case LVN_COLUMNCLICK:
			{
				LPNMLISTVIEW pnmv = (LPNMLISTVIEW)pnmhdr;
				static BOOL bSortDescending = FALSE;
				static int nPrevSubItem = -1;

				if (pnmv->iSubItem != nPrevSubItem)
					bSortDescending = FALSE;

				// Sort the items of the list-view control
				if (ListView_SortItemsEx(pnmhdr->hwndFrom, CompareFunc, MAKELONG(LOWORD(pnmv->iSubItem), bSortDescending)))
				{
					ListView_DrawSortArrow(pnmhdr->hwndFrom, bSortDescending ? HDF_SORTDOWN : HDF_SORTUP, pnmv->iSubItem);

					nPrevSubItem = pnmv->iSubItem;
					bSortDescending = !bSortDescending;
				}
				break;
			}

			case LVN_BEGINLABELEDIT:
			{
				// Set the text limit to 3 characters
				HWND hwndEdit = ListView_GetEditControl(pnmhdr->hwndFrom);
				if (hwndEdit != NULL)
					Edit_LimitText(hwndEdit, 3);
				return FALSE;
			}

			case LVN_ENDLABELEDIT:
			{
				NMLVDISPINFO* pdi = (NMLVDISPINFO*)pnmhdr;
				if (pdi->item.pszText != NULL)
				{
					// Allow only numbers between 0 and 999
					SIZE_T len = _tcslen(pdi->item.pszText);
					if (len == 0 || len > 3 || (len > 1 && pdi->item.pszText[0] == TEXT('0')))
						return FALSE;
					for (UINT i = 0; i < len; i++)
						if (pdi->item.pszText[i] < TEXT('0') || pdi->item.pszText[i] > TEXT('9'))
							return FALSE;
					return TRUE;
				}
				break;
			}
		}
	}

	return 0;
}

void WndProc_OnKey(HWND hwnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
	switch (vk)
	{
		case VK_TAB:
			SetFocus(hwndListView);
			break;
	}
}

void WndProc_OnSize(HWND hwnd, UINT state, int cx, int cy)
{
	RECT rcPart;
	RECT rcStatusBar = { 0 };

	// Forward this message to the status bar to automatically adjust its position and size
	FORWARD_WM_SIZE(hwndStatusBar, state, cx, cy, SendMessage);

	GetWindowRect(hwndStatusBar, &rcStatusBar);

	// Place the "Steering" trackbar in the part with index SBP_STEERING
	if (hwndTBSteering != NULL && SendMessage(hwndStatusBar, SB_GETRECT, SBP_STEERING, (LPARAM)&rcPart))
	{
		MoveWindow(hwndTBSteering, rcPart.left, rcPart.top, rcPart.right - rcPart.left - 1,
			rcPart.bottom - rcPart.top - 1, TRUE);
		SendMessage(hwndTBSteering, TBM_SETTHUMBLENGTH, rcPart.bottom - rcPart.top - 4, 0);
	}

	// Place the "Throttle" progress bar in the part with index SBP_THROTTLE
	if (hwndPBThrottle != NULL && SendMessage(hwndStatusBar, SB_GETRECT, SBP_THROTTLE, (LPARAM)&rcPart))
		MoveWindow(hwndPBThrottle, rcPart.left, rcPart.top, rcPart.right - rcPart.left - 1,
		rcPart.bottom - rcPart.top - 1, TRUE);

	// Place the "Rumble" progress bar in the part with index SBP_RUMBLE
	if (hwndPBRumble != NULL && SendMessage(hwndStatusBar, SB_GETRECT, SBP_RUMBLE, (LPARAM)&rcPart))
		MoveWindow(hwndPBRumble, rcPart.left, rcPart.top, rcPart.right - rcPart.left - 1,
		rcPart.bottom - rcPart.top - 1, TRUE);

	// Position the list-view control
	MoveWindow(hwndListView, 0, 0, cx, cy - (rcStatusBar.bottom - rcStatusBar.top), TRUE);
}

void WndProc_OnWinIniChange(HWND hwnd, LPCTSTR lpszSectionName)
{
	if (_tcscmp(lpszSectionName, TEXT("intl")) == 0)
		GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IMEASURE | LOCALE_RETURN_NUMBER,
			(LPWSTR)&bMilesPerHour, sizeof(bMilesPerHour) / sizeof(WCHAR));
}

void WndProc_OnSysColorChange(HWND hwnd)
{
	// Forward this message to all common controls
	FORWARD_WM_SYSCOLORCHANGE(hwndListView, PostMessage);
	FORWARD_WM_SYSCOLORCHANGE(hwndStatusBar, PostMessage);
	if (hwndTBSteering != NULL)
		FORWARD_WM_SYSCOLORCHANGE(hwndTBSteering, PostMessage);
	if (hwndPBThrottle != NULL)
		FORWARD_WM_SYSCOLORCHANGE(hwndPBThrottle, PostMessage);
	if (hwndPBRumble != NULL)
		FORWARD_WM_SYSCOLORCHANGE(hwndPBRumble, PostMessage);
}

void WndProc_OnDestroy(HWND hwnd)
{
	// Save settings
	WINDOWPLACEMENT wpl = { 0 };
	wpl.length = sizeof(wpl);
	GetWindowPlacement(hwnd, &wpl);

	TCHAR szRegPath[MAX_LOADSTRING];
	if (LoadString(hInst, IDS_REGPATH, szRegPath, _countof(szRegPath)) > 0)
	{
		HKEY hKey = NULL;
		if (RegCreateKeyEx(HKEY_CURRENT_USER, szRegPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE,
			NULL, &hKey, NULL) == ERROR_SUCCESS && hKey != NULL)
		{
			RegSetValueEx(hKey, szAutoDelete, 0, REG_DWORD, (LPBYTE)&bAutoDelete, sizeof(DWORD));
			RegSetValueEx(hKey, szColumns, 0, REG_DWORD, (LPBYTE)&dwColumns, sizeof(DWORD));
			RegSetValueEx(hKey, szWindowPlacement, 0, REG_BINARY, (LPBYTE)&wpl, sizeof(wpl));

			RegCloseKey(hKey);
		}
	}

	PostQuitMessage(0);
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
		case WM_INITDIALOG:
		{
			TCHAR szText[MAX_CONTROLTEXT];

			_sntprintf(szText, _countof(szText) - 1, szFmtTmIfVer, uHeaderVersion);
			szText[MAX_CONTROLTEXT-1] = TEXT('\0');
			SetDlgItemText(hDlg, IDC_TELEMETRY_VERSION, szText);

			_sntprintf(szText, _countof(szText) - 1, szFmtRumbleTh, uRumbleThreshold);
			szText[MAX_CONTROLTEXT - 1] = TEXT('\0');
			SetDlgItemText(hDlg, IDC_RUMBLE_THRESHOLD, szText);

			_sntprintf(szText, _countof(szText) - 1, szFmtThrottleTh, uFullspeedThreshold);
			szText[MAX_CONTROLTEXT - 1] = TEXT('\0');
			SetDlgItemText(hDlg, IDC_FULLSPEED_THRESHOLD, szText);
		}
		return (INT_PTR)TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
			break;
	}
	return (INT_PTR)FALSE;
}

//
//	FUNCTION: CompareFunc(LPARAM, LPARAM, LPARAM)
//
//	PURPOSE: List-view comparison function
//
//	COMMENTS:
//		lParam1 = Index of the first item
//		lParam2 = Index of the second item
//		LOWORD(lParamSort) = Index of the subitem
//		HIWORD(lParamSort) = Sort order
//
int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	TCHAR szSortText1[MAX_CONTROLTEXT];
	TCHAR szSortText2[MAX_CONTROLTEXT];
	szSortText1[0] = TEXT('\0');
	szSortText2[0] = TEXT('\0');

	ListView_GetItemText(hwndListView, lParam1, LOWORD(lParamSort),
		HIWORD(lParamSort) ? szSortText2 : szSortText1, MAX_CONTROLTEXT - 1);
	ListView_GetItemText(hwndListView, lParam2, LOWORD(lParamSort),
		HIWORD(lParamSort) ? szSortText1 : szSortText2, MAX_CONTROLTEXT - 1);

	int nLen1 = (int)_tcslen(szSortText1);
	int nLen2 = (int)_tcslen(szSortText2);
	int nDiff = nLen1 - nLen2;

	// Specify that empty cells are larger than items with text
	if ((nLen1 == 0) ^ (nLen2 == 0))
		return -nDiff;

	// Specify that shorter strings are always smaller
	if (nDiff != 0)
		return nDiff;

	return _tcsncmp(szSortText1, szSortText2, MAX_CONTROLTEXT - 1);
}
