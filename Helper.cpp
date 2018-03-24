// Helper.cpp - Helper functions
//
#include "stdafx.h"
#include "resource.h"

// List-view column index
static int nColumnIndex = 0;


BOOL FormatTime(LPTSTR lpszTime, SIZE_T cchStringLen, int nTime)
{
	if (lpszTime == NULL)
		return FALSE;

	if (nTime <= 0)
		_tcsncpy(lpszTime, TEXT("0:00.000"), cchStringLen);
	else if (nTime < 3600000)
	{
		UINT uMinute = (nTime / 60000);
		UINT uSecond = (nTime % 60000 / 1000);
		UINT uMilliSec = (nTime % 60000 % 1000);

		_sntprintf(lpszTime, cchStringLen, TEXT("%d:%02d.%003d"), uMinute, uSecond, uMilliSec);
	}
	else
	{
		UINT uHour = (nTime / 3600000);
		UINT uMinute = (nTime % 3600000 / 60000);
		UINT uSecond = (nTime % 3600000 % 60000 / 1000);
		UINT uMilliSec = (nTime % 3600000 % 60000 % 1000);

		_sntprintf(lpszTime, cchStringLen, TEXT("%d:%02d:%02d.%003d"), uHour, uMinute, uSecond, uMilliSec);
	}

	return TRUE;
}


BOOL GetFileName(HWND hWnd, LPTSTR lpszFileName, SIZE_T cchStringLen, LPDWORD lpdwFilterIndex, BOOL bSave)
{
	if (lpszFileName == NULL || lpdwFilterIndex == NULL)
		return FALSE;

	// Filter string
	TCHAR szFilter[512];
	szFilter[0] = TEXT('\0');
	LoadString(GetWindowInstance(hWnd), IDS_FILTER, szFilter, _countof(szFilter));
	TCHAR* psz = szFilter;
	while (psz = _tcschr(psz, TEXT('|')))
		*psz++ = TEXT('\0');

	// Initial directory
	TCHAR szInitialDir[MAX_PATH];
	TCHAR* pszInitialDir = szInitialDir;
	if (lpszFileName[0] != TEXT('\0'))
	{
		_tcsncpy(pszInitialDir, lpszFileName, _countof(szInitialDir));
		TCHAR* token = _tcsrchr(pszInitialDir, TEXT('\\'));
		if (token != NULL)
			pszInitialDir[token - szInitialDir] = TEXT('\0');
		else
			pszInitialDir = NULL;
	}
	else
		pszInitialDir = NULL;

	// File name
	TCHAR szFile[MAX_PATH];
	if (bSave)
	{
		if (lpszFileName[0] != TEXT('\0'))
			_tcsncpy(szFile, lpszFileName, _countof(szFile));
		else
			_tcsncpy(szFile, TEXT("*"), _countof(szFile));
		TCHAR* token = _tcsrchr(szFile, TEXT('.'));
		if (token != NULL)
			szFile[token - szFile] = TEXT('\0');
		_tcsncat(szFile, (*lpdwFilterIndex == FI_CSV) ? TEXT(".csv") : TEXT(".txt"),
			_countof(szFile) - _tcslen(szFile) - 1);
	}
	else
		szFile[0] = TEXT('\0');

	OPENFILENAME of = { 0 };
	of.lStructSize = sizeof(OPENFILENAME);
	of.hwndOwner = hWnd;
	of.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	of.lpstrFile = szFile;
	of.nMaxFile = _countof(szFile);
	of.lpstrFilter = szFilter;
	of.nFilterIndex = *lpdwFilterIndex;
	of.lpstrDefExt = (*lpdwFilterIndex == FI_CSV) ? TEXT("csv") : TEXT("txt");
	of.lpstrInitialDir = pszInitialDir;

	BOOL bRet = FALSE;

	__try
	{
		if (bSave)
		{
			of.Flags |= OFN_OVERWRITEPROMPT;
			bRet = GetSaveFileName(&of);
		}
		else
		{
			of.Flags |= OFN_FILEMUSTEXIST;
			bRet = GetOpenFileName(&of);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { ; }

	if (bRet)
	{
		_tcsncpy(lpszFileName, szFile, cchStringLen);
		*lpdwFilterIndex = of.nFilterIndex;
	}

	return bRet;
}


BOOL StatusBar_SetText(HWND hwndCtl, UINT uIndexType, LPCTSTR lpszText, BOOL bCenter)
{
	TCHAR szText[MAX_CONTROLTEXT];

	if (hwndCtl == NULL)
		return FALSE;

	// Set ToolTip
	SendMessage(hwndCtl, SB_SETTIPTEXT, (WPARAM)LOBYTE(LOWORD(uIndexType)), (LPARAM)lpszText);

	if (!bCenter)
		return (BOOL)SendMessage(hwndCtl, SB_SETTEXT, (WPARAM)uIndexType, (LPARAM)lpszText);
	else
	{
		// Center text
		_sntprintf(szText, _countof(szText), TEXT("\t%s"), lpszText);
		return (BOOL)SendMessage(hwndCtl, SB_SETTEXT, (WPARAM)uIndexType, (LPARAM)szText);
	}
}


void ListView_SelectAll(HWND hwndCtl)
{
	if (hwndCtl != NULL)
	{
		SetFocus(hwndCtl);
		ListView_SetItemState(hwndCtl, -1, LVIS_SELECTED, LVIS_SELECTED);
	}
}

void ListView_InvertSelection(HWND hwndCtl)
{
	if (hwndCtl != NULL)
	{
		int iItem = -1;
		UINT uFlag = 0;

		SetFocus(hwndCtl);
		while ((iItem = ListView_GetNextItem(hwndCtl, iItem, LVNI_ALL)) != -1)
		{
			uFlag = ListView_GetItemState(hwndCtl, iItem, LVIS_SELECTED);
			uFlag ^= LVIS_SELECTED;
			ListView_SetItemState(hwndCtl, iItem, uFlag, LVIS_SELECTED);
		}
	}
}

void ListView_RenameSelectedItem(HWND hwndCtl)
{
	if (hwndCtl == NULL || ListView_GetSelectedCount(hwndCtl) != 1 ||
		ListView_GetEditControl(hwndCtl) != NULL)
		return;

	int iItem = ListView_GetNextItem(hwndCtl, -1, LVNI_ALL | LVNI_SELECTED);
	if (iItem != -1)
	{
		SetFocus(hwndCtl);
		ListView_EditLabel(hwndCtl, iItem);
	}
}

void ListView_DeleteSelectedItems(HWND hwndCtl)
{
	if (hwndCtl != NULL)
	{
		int iItem = -1;
		while ((iItem = ListView_GetNextItem(hwndCtl, -1, LVNI_ALL | LVNI_SELECTED)) != -1)
			ListView_DeleteItem(hwndCtl, iItem);
	}
}

int ListView_GetColumnCount(HWND hwndCtl)
{
	if (hwndCtl == NULL)
		return -1;

	return Header_GetItemCount(ListView_GetHeader(hwndCtl));
}

void ListView_AutoSizeColumn(HWND hwndCtl, int iCol)
{
	int nColumnsCnt = ListView_GetColumnCount(hwndCtl);
	if (nColumnsCnt <= 0 || iCol >= nColumnsCnt)
		return;

	if (iCol < 0)
		iCol = nColumnIndex;

	SetWindowRedraw(hwndCtl, FALSE);

	// Add a bogus column so SetColumnWidth doesn't do
	// strange things with the last real column
	LV_COLUMN col = { 0 };
	ListView_InsertColumn(hwndCtl, nColumnsCnt, &col);

	// Autosize the column to fit header/text tightly:
	ListView_SetColumnWidth(hwndCtl, iCol, LVSCW_AUTOSIZE_USEHEADER);
	int nWidthHeader = ListView_GetColumnWidth(hwndCtl, iCol);
	ListView_SetColumnWidth(hwndCtl, iCol, LVSCW_AUTOSIZE);
	int nWidthText = ListView_GetColumnWidth(hwndCtl, iCol);
	if (nWidthText < nWidthHeader)
		ListView_SetColumnWidth(hwndCtl, iCol, nWidthHeader);

	// Delete the bogus column that was created
	ListView_DeleteColumn(hwndCtl, nColumnsCnt);

	SetWindowRedraw(hwndCtl, TRUE);
	RedrawWindow(hwndCtl, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void ListView_AutoSizeAllColumns(HWND hwndCtl)
{
	int nColumnsCnt = ListView_GetColumnCount(hwndCtl);
	if (nColumnsCnt <= 0)
		return;

	SetWindowRedraw(hwndCtl, FALSE);

	// Add a bogus column so SetColumnWidth doesn't do
	// strange things with the last real column
	LV_COLUMN col = { 0 };
	ListView_InsertColumn(hwndCtl, nColumnsCnt, &col);

	// Autosize all columns to fit header/text tightly:
	int nWidthHeader, nWidthText;
	for (int iCol = 0;; iCol++)
	{
		if (!ListView_SetColumnWidth(hwndCtl, iCol, LVSCW_AUTOSIZE_USEHEADER))
			break;

		nWidthHeader = ListView_GetColumnWidth(hwndCtl, iCol);
		ListView_SetColumnWidth(hwndCtl, iCol, LVSCW_AUTOSIZE);
		nWidthText = ListView_GetColumnWidth(hwndCtl, iCol);

		if (nWidthText < nWidthHeader)
			ListView_SetColumnWidth(hwndCtl, iCol, nWidthHeader);
	}

	// Delete the bogus column that was created
	ListView_DeleteColumn(hwndCtl, nColumnsCnt);

	SetWindowRedraw(hwndCtl, TRUE);
	RedrawWindow(hwndCtl, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

BOOL ListView_DeleteAllColumns(HWND hwndCtl)
{
	if (hwndCtl == NULL)
		return FALSE;

	BOOL bSuccess = TRUE;
	for (int nColumnsCnt = ListView_GetColumnCount(hwndCtl); nColumnsCnt > 0; nColumnsCnt--)
		bSuccess &= ListView_DeleteColumn(hwndCtl, nColumnsCnt - 1);

	return bSuccess;
}

// Use mask = HDF_SORTUP or mask = HDF_SORTDOWN to set an arrow.
// Use index = DSA_REMOVE_ALL to remove all arrows.
void ListView_DrawSortArrow(HWND hwndCtl, int mask, int index)
{
	if (hwndCtl == NULL)
		return;

	HWND hwndHD = ListView_GetHeader(hwndCtl);
	if (hwndHD == NULL)
		return;

	HD_ITEM hdi = { 0 };
	hdi.mask = HDI_FORMAT;

	for (int i = 0; i < Header_GetItemCount(hwndHD); i++)
	{
		if (Header_GetItem(hwndHD, i, &hdi))
		{
			int fmt = hdi.fmt;
			fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);

			if (i == index)
			{
				if (mask & HDF_SORTDOWN)
					fmt |= HDF_SORTDOWN;
				if (mask & HDF_SORTUP)
					fmt |= HDF_SORTUP;
			}

			if (fmt != hdi.fmt)
			{
				hdi.fmt = fmt;
				Header_SetItem(hwndHD, i, &hdi);
			}
		}
	}
}

LPARAM ListView_GetContextMenuPoint(HWND hwndCtl)
{
	POINT pt = { 0 };

	if (hwndCtl != NULL)
	{
		int iItem = ListView_GetNextItem(hwndCtl, -1, LVNI_ALL | LVNI_SELECTED);
		if (iItem != -1)
		{
			RECT rc = { 0 };
			if (ListView_GetItemRect(hwndCtl, iItem, &rc, LVIR_ICON))
			{
				pt.x = (rc.left + rc.right) / 2;
				pt.y = (rc.top + rc.bottom) / 2;
			}
		}

		MapWindowPoints(hwndCtl, NULL, &pt, 1);
	}

	return MAKELPARAM(pt.x, pt.y);
}

int ListView_AddRace(HWND hwndCtl, int nRaceNumber, int nColumnWidth)
{
	if (hwndCtl == NULL)
		return -1;

	// Reset the list-view column index
	nColumnIndex = 0;

	// We need at least one column
	int nColumnsCnt = ListView_GetColumnCount(hwndCtl);
	if (nColumnsCnt <= 0)
	{
		LV_COLUMN col = { 0 };
		col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		col.fmt = LVCFMT_LEFT;
		col.cx = nColumnWidth < 0 ? 0 : nColumnWidth;
		col.pszText = (LPTSTR)szRace;
		col.iSubItem = nColumnIndex;

		if (ListView_InsertColumn(hwndCtl, col.iSubItem, &col) == -1)
			return -1;
		else if (nColumnWidth < 0)
			ListView_AutoSizeColumn(hwndCtl, col.iSubItem);
	}

	// Increment the list-view column index
	nColumnIndex++;

	TCHAR szItemText[MAX_CONTROLTEXT];
	_sntprintf(szItemText, _countof(szItemText), TEXT("%d"), nRaceNumber);

	// Create a new race item
	LV_ITEM item = { 0 };
	item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
	item.iItem = ListView_GetItemCount(hwndCtl);
	item.iImage = 0;
	item.lParam = nRaceNumber;
	item.pszText = szItemText;

	int iIndex = ListView_InsertItem(hwndCtl, &item);
	if (iIndex != -1)
	{
		ListView_EnsureVisible(hwndCtl, iIndex, FALSE);
		ListView_DrawSortArrow(hwndCtl, 0, DSA_REMOVE_ALL);
	}

	return iIndex;
}

int ListView_AddRaceText(HWND hwndCtl, int nRaceNumber, int nColumnWidth, LPCTSTR lpszHeading, LPCTSTR lpszText)
{
	if (hwndCtl == NULL)
		return -1;

	// We need at least one item
	int nItemCount = ListView_GetItemCount(hwndCtl);
	if (nItemCount == 0 && ListView_AddRace(hwndCtl, 0, COLUMN_AUTOFIT) != -1)
		nItemCount++;

	if (ListView_GetColumnCount(hwndCtl) <= nColumnIndex)
	{
		// If necessary, create a column for this sub item
		LV_COLUMN col = { 0 };
		col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		col.fmt = LVCFMT_CENTER;
		col.cx = nColumnWidth < 0 ? 0 : nColumnWidth;
		col.pszText = (LPTSTR)lpszHeading;
		col.iSubItem = nColumnIndex;

		ListView_InsertColumn(hwndCtl, nColumnIndex, &col);
	}
	else
	{
		// If necessary, rename the header if the application
		// was started while a race was already in progress
		TCHAR szHeading[MAX_CONTROLTEXT];

		LV_COLUMN col = { 0 };
		col.mask = LVCF_TEXT;
		col.pszText = szHeading;
		col.cchTextMax = _countof(szHeading) - 1;
		col.iSubItem = nColumnIndex;

		if (ListView_GetColumn(hwndCtl, nColumnIndex, &col) &&
			_tcsncmp(col.pszText, lpszHeading, _countof(szHeading) - 1) != 0)
		{
			col.mask = LVCF_TEXT;
			col.pszText = (LPTSTR)lpszHeading;

			ListView_SetColumn(hwndCtl, nColumnIndex, &col);
		}
	}

	// Search the item with the given race number
	LV_FINDINFO lvfi = { 0 };
	lvfi.flags = LVFI_PARAM | LVFI_WRAP;
	lvfi.lParam = nRaceNumber;

	int iIndex = ListView_FindItem(hwndCtl, nItemCount - 2, &lvfi);
	if (iIndex == -1)
		iIndex = nItemCount - 1;

	// Set the text of the sub item
	LV_ITEM item = { 0 };
	item.mask = LVIF_TEXT;
	item.pszText = (LPTSTR)lpszText;
	item.iItem = iIndex;
	item.iSubItem = nColumnIndex;

	ListView_SetItemText(hwndCtl, item.iItem, item.iSubItem, item.pszText);
	if (nColumnWidth < 0)
		ListView_AutoSizeColumn(hwndCtl, nColumnIndex);

	// Increment the list-view column index
	nColumnIndex++;

	return nColumnIndex;
}

int ListView_AddRaceTime(HWND hwndCtl, int nRaceNumber, int nColumnWidth, LPCTSTR lpszHeading, int nTime)
{
	TCHAR szTime[MAX_CONTROLTEXT];
	FormatTime(szTime, _countof(szTime), nTime);

	return ListView_AddRaceText(hwndCtl, nRaceNumber, nColumnWidth, lpszHeading, szTime);
}

int ListView_AddRaceData(HWND hwndCtl, int nRaceNumber, int nColumnWidth, LPCTSTR lpszHeading, LPCTSTR lpszFormat, int nData)
{
	TCHAR szData[MAX_CONTROLTEXT];
	_sntprintf(szData, _countof(szData), lpszFormat, nData);

	return ListView_AddRaceText(hwndCtl, nRaceNumber, nColumnWidth, lpszHeading, szData);
}

int ListView_AddCheckpointTime(HWND hwndCtl, int nRaceNumber, int nColumnWidth, int nCheckpointNumber, int nCheckpointTime)
{
	TCHAR szHeading[MAX_CONTROLTEXT];
	TCHAR szTime[MAX_CONTROLTEXT];

	_sntprintf(szHeading, _countof(szHeading), szCheckpoint, nCheckpointNumber);
	FormatTime(szTime, _countof(szTime), nCheckpointTime);

	return ListView_AddRaceText(hwndCtl, nRaceNumber, nColumnWidth, szHeading, szTime);
}

int ListView_AddSectorTime(HWND hwndCtl, int nRaceNumber, int nColumnWidth, int nSectorNumber, int nCurrentTime, int nPreviousTime)
{
	TCHAR szHeading[MAX_CONTROLTEXT];
	TCHAR szTime[MAX_CONTROLTEXT];

	_sntprintf(szHeading, _countof(szHeading), szSector, nSectorNumber);
	FormatTime(szTime, _countof(szTime), nCurrentTime - nPreviousTime);

	return ListView_AddRaceText(hwndCtl, nRaceNumber, nColumnWidth, szHeading, szTime);
}

BOOL ListView_DeleteRace(HWND hwndCtl, int nRaceNumber)
{
	if (hwndCtl == NULL)
		return FALSE;

	int nItemCount = ListView_GetItemCount(hwndCtl);

	if (nRaceNumber < 0)
		return ListView_DeleteItem(hwndCtl, nItemCount - abs(nRaceNumber));

	// Search the item with the given race number
	LV_FINDINFO lvfi = { 0 };
	lvfi.flags = LVFI_PARAM | LVFI_WRAP;
	lvfi.lParam = nRaceNumber;

	int iIndex = ListView_FindItem(hwndCtl, nItemCount - 2, &lvfi);
	if (iIndex == -1)
		return FALSE;

	return ListView_DeleteItem(hwndCtl, iIndex);
}

BOOL ListView_DeleteAllRaces(HWND hwndCtl)
{
	if (hwndCtl == NULL)
		return FALSE;

	BOOL bSuccess = TRUE;
	if (ListView_GetItemCount(hwndCtl) > 0)
		bSuccess = ListView_DeleteAllItems(hwndCtl);

	ListView_DeleteAllColumns(hwndCtl);

	return bSuccess;
}

BOOL ListView_SaveAllItems(HWND hwndCtl, LPTSTR lpszFileName, BOOL bSaveAsCsv)
{
	TCHAR szText[MAX_CONTROLTEXT];

	if (hwndCtl == NULL)
		return FALSE;

	// Open an empty file for writing in text mode with UTF8 encoding
	FILE* pFile = _tfopen(lpszFileName, TEXT("wt, ccs=UTF-8"));
	if (pFile == NULL)
		return FALSE;

	// Save the header
	LV_COLUMN col = { 0 };
	col.mask = LVCF_TEXT;
	col.pszText = szText;
	col.cchTextMax = _countof(szText) - 1;

	int iCol;
	int nColumnCount = ListView_GetColumnCount(hwndCtl);
	for (iCol = 0; iCol < nColumnCount; iCol++)
	{
		col.iSubItem = iCol;
		if (ListView_GetColumn(hwndCtl, iCol, &col))
			_fputts(col.pszText, pFile);
		_fputts((iCol < nColumnCount - 1) ? (bSaveAsCsv ? TEXT(",") : TEXT("\t")) : TEXT("\n"), pFile);
	}

	// Save the items and sub items
	LV_ITEM item = { 0 };
	item.mask = LVIF_TEXT;
	item.pszText = szText;
	item.cchTextMax = _countof(szText) - 1;

	int iItem;
	int nItemCount = ListView_GetItemCount(hwndCtl);
	for (iItem = 0; iItem < nItemCount; iItem++)
	{
		for (iCol = 0; iCol < nColumnCount; iCol++)
		{
			item.iItem = iItem;
			item.iSubItem = iCol;
			if (ListView_GetItem(hwndCtl, &item))
				_fputts(item.pszText, pFile);
			_fputts((iCol < nColumnCount - 1) ? (bSaveAsCsv ? TEXT(",") : TEXT("\t")) : TEXT("\n"), pFile);
		}
	}

	fclose(pFile);

	return TRUE;
}
