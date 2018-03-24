// Helper.h : Helper functions
//
#pragma once

#define INDEX_CURRENT	-1
#define DSA_REMOVE_ALL	-1
#define COLUMN_AUTOFIT	-1

#define MAX_CONTROLTEXT	256

typedef enum _FILTERINDEX
{
	FI_CUSTOM = 0,
	FI_TEXT,
	FI_CSV
} FILTERINDEX;

// Status bar string constants
const TCHAR szGameStarting[] = TEXT("Game: Starting");
const TCHAR szGameMenus[] = TEXT("Game: Menus");
const TCHAR szGameRunning[] = TEXT("Game: Running");
const TCHAR szGamePaused[] = TEXT("Game: Paused");
const TCHAR szRaceBeforeStart[] = TEXT("Race: BeforeStart");
const TCHAR szRaceRunning[] = TEXT("Race: Running");
const TCHAR szRaceFinished[] = TEXT("Race: Finished");
const TCHAR szNbRespawns[] = TEXT("RSP: %d");
const TCHAR szNbCheckpoints[] = TEXT("CP: %d");
const TCHAR szSpeedKmh[] = TEXT("%d km/h");
const TCHAR szSpeedMph[] = TEXT("%d mph");
const TCHAR szEngineRpm[] = TEXT("%.0f rpm");
const TCHAR szEngineCurGear[] = TEXT("Gear: %d");
const TCHAR szGasPedal[] = TEXT("Throttle: %3.0f%%");
const TCHAR szRumbleIntensity[] = TEXT("Rumble: %3.0f%%");
const TCHAR szBraking[] = TEXT("Braking");

// List-view headings
const TCHAR szRace[] = TEXT("Race");
const TCHAR szDate[] = TEXT("Date");
const TCHAR szTime[] = TEXT("Time");
const TCHAR szMapIdentifier[] = TEXT("Map Identifier");
const TCHAR szTrackName[] = TEXT("Map Name");
const TCHAR szPlayerModel[] = TEXT("Player Model");
const TCHAR szSector[] = TEXT("Sector %d");
const TCHAR szCheckpoint[] = TEXT("CP %d");
const TCHAR szRaceTime[] = TEXT("Race Time");
const TCHAR szTopSpeed[] = TEXT("Top Speed");
const TCHAR szRespawns[] = TEXT("Respawns");

// General functions
BOOL FormatTime(LPTSTR lpszTime, SIZE_T cchStringLen, int nTime);
BOOL GetFileName(HWND hDlg, LPTSTR lpszFileName, SIZE_T cchStringLen, LPDWORD lpdwFilterIndex, BOOL bSave = FALSE);

// Status bar functions
BOOL StatusBar_SetText(HWND hwndCtl, UINT uIndexType, LPCTSTR lpszText, BOOL bCenter = FALSE);

// List-view functions
void ListView_SelectAll(HWND hwndCtl);
void ListView_InvertSelection(HWND hwndCtl);
void ListView_RenameSelectedItem(HWND hwndCtl);
void ListView_DeleteSelectedItems(HWND hwndCtl);

int  ListView_GetColumnCount(HWND hwndCtl);
void ListView_AutoSizeColumn(HWND hwndCtl, int iCol = INDEX_CURRENT);
void ListView_AutoSizeAllColumns(HWND hwndCtl);
BOOL ListView_DeleteAllColumns(HWND hwndCtl);

void ListView_DrawSortArrow(HWND hwndCtl, int mask = 0, int index = DSA_REMOVE_ALL);
LPARAM ListView_GetContextMenuPoint(HWND hwndCtl);

int  ListView_AddRace(HWND hwndCtl, int nRaceNumber, int nColumnWidth = COLUMN_AUTOFIT);
int  ListView_AddRaceTime(HWND hwndCtl, int nRaceNumber, int nColumnWidth, LPCTSTR lpszHeading, int nTime);
int  ListView_AddRaceText(HWND hwndCtl, int nRaceNumber, int nColumnWidth, LPCTSTR lpszHeading, LPCTSTR lpszText);
int  ListView_AddRaceData(HWND hwndCtl, int nRaceNumber, int nColumnWidth, LPCTSTR lpszHeading, LPCTSTR lpszFormat, int nData);
int  ListView_AddCheckpointTime(HWND hwndCtl, int nRaceNumber, int nColumnWidth, int nCheckpointNumber, int nCheckpointTime);
int  ListView_AddSectorTime(HWND hwndCtl, int nRaceNumber, int nColumnWidth, int nSectorNumber, int nCurrentTime, int nPreviousTime = 0);

BOOL ListView_DeleteRace(HWND hwndCtl, int nRaceNumber = INDEX_CURRENT);
BOOL ListView_DeleteAllRaces(HWND hwndCtl);

BOOL ListView_SaveAllItems(HWND hwndCtl, LPTSTR lpszFileName, BOOL bSaveAsCsv = FALSE);
