/*
 *      PROGRAM:        IB Server and Server Manager
 *      MODULE:         ibconfig.c
 *      DESCRIPTION:    IB Configuration implementation for both
 *                  Server Manager and Server Property sheet
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 */
#include <windows.h>
#include <shellapi.h>
#include <prsht.h>
#include <dbt.h>

#define BOOLEAN_DEFINED
#define MSG_DEFINED

#include "../jrd/common.h"
#include "../jrd/isc.h"
#include "../jrd/file_params.h"
#include "../ipserver/ips.h"

#include "../jrd/svc_undoc.h"
#include "../jrd/svc_proto.h"
#include "../remote/window_proto.h"
#include "../ipserver/ipsrv_proto.h"

#include "../remote/ibconfig.h"
#include "../remote/property.rh"

#include "../jrd/ibase.h"

#include "../remote/ibsvrhlp.h"

#ifdef GUI_TOOLS        // For server manager only
void ShowErrDlg(int, char*, HWND);
#endif

// Symbolic constant definitions
#define PASSWORD_LEN            33      // MAXIMUM PASSWORD LENGTH DEFINITION
#define TEMP_BUFLEN             33
#define STATUS_BUFLEN           20
#define SEND_BUFLEN             32
#define RESP_BUFLEN             128

#define SPB_BUFLEN              128
#define ERR_BUFLEN              1024
#define HDR_BUFLEN              512

static HINSTANCE hAppInstance = NULL;   // Handle to the app. instance
char   szSysDbaPasswd[PASSWORD_LEN];    // Pointer to hold the password
HBRUSH hIBGrayBrush = NULL;             // Handle to a gray brush
static BOOL bServerApp = FALSE;         // Flag indicating if Server application
static char szService[SERVICE_LEN];     // To store the service path

// Window procedure
LRESULT APIENTRY InterbasePage(HWND , UINT, WPARAM, LPARAM);
LRESULT APIENTRY OSPage(HWND , UINT, WPARAM, LPARAM);

// Static functions for reading and writing settings
static BOOL ReadIBSettings(HWND);
static BOOL WriteIBSettings(HWND);
static BOOL ReadOSSettings(HWND);
static BOOL WriteOSSettings(HWND);

static void RefreshIBControls(HWND, BOOL);
static void RefreshOSControls(HWND, BOOL);
static BOOL ValidateUser(HWND);
static void PrintCfgStatus (STATUS *, int, HWND);
static void FillSysdbaSPB(char* , char* );

// Define an array of dword pairs,
// where the first of each pair is the control ID,
// and the second is the context ID for a help topic,
// which is used in the help file.
static DWORD aMenuHelpIDs1[] =
{
   IDC_MODRES, ibs_modify,      // This has to be the first entry because
				// we modify the value of ibs_modify to 
				// ibs_reset when the button text changes.

   IDC_MINSIZETEXT, ibs_min_process_work_set,
   IDC_MAXSIZETEXT, ibs_min_process_work_set,
   IDC_CACHETEXT, ibs_db_cache,
   IDC_DBPAGES, ibs_db_cache,
   IDC_MAPSIZE, ibs_client_map_size,
   IDC_MAPTEXT, ibs_client_map_size,
   IDC_MINSIZE, ibs_min_process_work_set,
   IDC_MAXSIZE, ibs_min_process_work_set,
   IDC_PRIORITYHIGH, ibs_process_priority,
   IDC_PRIORITYNORM, ibs_process_priority, 
   0,    0
};

// IB Parameters to get/set
SLONG lCachePages = 0;
SLONG lMapSize = 0;

void AddConfigPages(HWND hPropSheet, HINSTANCE hInst)
{
/******************************************************************************
 *
 *  A d d C o n f i g P a g e s
 *
 ******************************************************************************
 *
 *  Input:  hPropSheet - Handle to the parent property sheet dialog
 *          hInst - Handle to the application instance
 *
 *  Return: void
 *
 *  Description: This function acts as an entry point into the config
 *               dialogs implementation from the Prop. sheet side. That is,
 *               the dialogs are added as pages in the property sheet.
 *****************************************************************************/
PROPSHEETPAGE IBPage;

hIBGrayBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
hAppInstance = hInst;
bServerApp = TRUE;
szService[0] = '\0';
szSysDbaPasswd[0] = '\0';

IBPage.dwSize = sizeof(PROPSHEETPAGE);
IBPage.dwFlags = PSP_USETITLE | PSP_HASHELP;
IBPage.hInstance = hInst;
#ifdef  __BORLANDC__            // Anonymous unions not working in BC
IBPage.DUMMYUNIONNAME.pszTemplate = MAKEINTRESOURCE(CONFIG_DLG);
#else
IBPage.pszTemplate = MAKEINTRESOURCE(CONFIG_DLG);
#endif
IBPage.pszTitle = "IB Settings";
IBPage.pfnDlgProc = (DLGPROC) InterbasePage;
IBPage.pfnCallback = NULL;

PropSheet_AddPage(hPropSheet, CreatePropertySheetPage(&IBPage));
}

LRESULT CALLBACK InterbasePage(HWND hDlg, UINT unMsg, WPARAM wParam, LPARAM lParam)
{
/******************************************************************************
 *
 *  I n t e r B a s e P a g e
 *
 ******************************************************************************
 *
 *  Input:  hDlg - Handle to the page dialog
 *          unMsg - Message ID
 *          wParam - WPARAM message parameter
 *          lParam - LPARAM message parameter
 *
 *  Return: FALSE if message is not processed
 *          TRUE if message is processed here
 *
 *  Description: This is the window procedure for - the IB Settings page in the
 *               Server property sheet , or for the IB Settings dialog in the
 *               Server manager. Contains logic to view and modify the
 *               the settings. When the user clicks on the modify button for
 *               the first time, it SYSDBA password is validated.
 *****************************************************************************/
static BOOL bModifyMode = FALSE;
static BOOL bDirty = FALSE;

switch (unMsg)
    {
    case WM_INITDIALOG:
	{
	if (!ReadIBSettings(hDlg))
	    return FALSE;
	RefreshIBControls(hDlg, FALSE);
	bModifyMode = FALSE;
	bDirty = FALSE;
	}
	break;
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORBTN:
	{
	OSVERSIONINFO   OsVersionInfo;

	OsVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (GetVersionEx ((LPOSVERSIONINFO) &OsVersionInfo) &&
		OsVersionInfo.dwMajorVersion < 4)
	    {
	    SetBkMode((HDC) wParam, TRANSPARENT);
	    return (LRESULT) hIBGrayBrush;
	    }
	}
	break;
    case WM_HELP:
	{
	LPHELPINFO lphi;

	lphi = (LPHELPINFO)lParam;
	if (lphi->iContextType == HELPINFO_WINDOW)   // must be for a control
	    {
	    WinHelp (lphi->hItemHandle,
		     "IBSERVER.HLP",
		     HELP_WM_HELP,
		     (DWORD)(LPVOID)aMenuHelpIDs1);
	    }
	return TRUE;
	}
    case WM_CONTEXTMENU:
	{
	WinHelp ((HWND)wParam,
		  "IBSERVER.HLP",
		  HELP_CONTEXTMENU,
		  (DWORD)(LPVOID)aMenuHelpIDs1);
	return TRUE;
	}
    case WM_COMMAND:
	switch (LOWORD(wParam))
	    {
	    case IDC_MODRES:
		if (bModifyMode)        // User clicked on RESET
		    {
		    bModifyMode = FALSE;
		    bDirty = FALSE;
		    ReadIBSettings(hDlg);
		    RefreshIBControls(hDlg, FALSE);
		    PropSheet_UnChanged(GetParent(hDlg), hDlg);
		    SetFocus(GetDlgItem(hDlg, IDC_MODRES));
		    }
		else                    // User clicked on MODIFY
		    {
		    if (szSysDbaPasswd[0] || ValidateUser(hDlg))
			{
			RefreshIBControls(hDlg, TRUE);
			bModifyMode = TRUE;
			bDirty = FALSE;
			SendDlgItemMessage(hDlg, IDC_DBPAGES, EM_SETSEL,
				0, (LPARAM) -1);
			SetFocus(GetDlgItem(hDlg, IDC_DBPAGES));
			}
		    }
		break;
	    case IDC_MAPSIZE:
		if (bModifyMode && HIWORD(wParam) == CBN_SELCHANGE)
		    {
		    PropSheet_Changed(GetParent(hDlg), hDlg);
		    bDirty = TRUE;
		    }
		break;
	    case IDC_DBPAGES:
		if (bModifyMode && HIWORD(wParam) == EN_UPDATE)
		    {
		    PropSheet_Changed(GetParent(hDlg), hDlg);
		    bDirty = TRUE;
		    }
		break;
	    }
	break;
    case WM_NOTIFY:
	switch (((LPNMHDR)lParam)->code)
	    {
	    case PSN_KILLACTIVE:      // When the page is about to lose focus
		{
		SetWindowLong(hDlg, DWL_MSGRESULT, FALSE);
		break;
		}
	    case PSN_SETACTIVE:       // When the page is about to recieve 
		{                     // focus
		// This modifies the bubble help table to bring up 
		// the appropriate help topic based on whether the text 
		// is "Modify..." or "Reset".  It assumes that the first 
		// entry is the first one.
		aMenuHelpIDs1[0,1] = bModifyMode ? ibs_reset : ibs_modify;
		break;
		}
	    case PSN_APPLY:           // When 'OK' or 'Apply Now' are clicked
		if (bDirty)
		    if (WriteIBSettings(hDlg))  // Values written successfully
			{
			bModifyMode = FALSE;
			bDirty = FALSE;
			ReadIBSettings(hDlg);
			RefreshIBControls(hDlg, FALSE);
			SetFocus(GetDlgItem(hDlg, IDC_MODRES));
			}
		    else              // Error writing the values
			{
			SetWindowLong(hDlg, DWL_MSGRESULT, TRUE);
			return TRUE;
			}
		break;
	    case PSN_HELP:
		HelpCmd(hDlg, hAppInstance, ibsp_IB_Settings_Properties);
		break;
	    }
	break;
    }
return FALSE;
}

BOOL ReadIBSettings(HWND hDlg)
{
/******************************************************************************
 *
 *  R e a d I B S e t t i n g s
 *
 ******************************************************************************
 *
 *  Input:  hDlg - Handle to the IB settings page dialog
 *
 *  Return: void
 *
 *  Description: This method calls the ISC_get_config() function to read the
 *               current IB settings in the config file and then sets these
 *               values to the corresponding controls.
 *****************************************************************************/
BOOL           bSuccess;
char           pchTmp[TEMP_BUFLEN];
long           pdwStatus[STATUS_BUFLEN];
isc_svc_handle hService = NULL;
char           pchRcvBuf[SEND_BUFLEN], pchResBuf[RESP_BUFLEN];
char*          pchPtr;

HCURSOR hOldCursor = NULL;

lCachePages = 0;
lMapSize = 0;
bSuccess = FALSE;

// Attach to "anonymous" service
hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
pchPtr = szService + strlen(szService);
strcat(szService, "anonymous");
isc_service_attach(pdwStatus, 0, szService, &hService, 0, "");

*pchPtr = '\0';

if (pdwStatus[1])
    {
    SetCursor(hOldCursor);
    PrintCfgStatus(pdwStatus, IDS_CFGATTACH_FAILED, hDlg);
    SetDlgItemInt(hDlg, IDC_DBPAGES, lCachePages, TRUE);
    SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_RESETCONTENT, 0, 0);

	/* To eliminate the flicker while closing the Property Sheet */
	SetWindowPos(GetParent(hDlg), HWND_DESKTOP,
				  0, 0, 0, 0, SWP_HIDEWINDOW);
	/* Close the Property Sheet */
	SendMessage(GetParent(hDlg), WM_CLOSE, 0, 0);

    return bSuccess;
    }

pchRcvBuf[0] = isc_info_svc_get_config;

// Query the service with get_config
isc_service_query(pdwStatus, &hService, NULL, 0, NULL, 1, pchRcvBuf, sizeof(pchResBuf), pchResBuf);
if (pdwStatus[1])
    PrintCfgStatus(pdwStatus, IDS_CFGREAD_FAILED, hDlg);
else
    {
    char *pchTmp = pchResBuf;
    short len = 0, chTmp = 0, key;
    ULONG ulConfigInfo;

    if (*pchTmp++ == isc_info_svc_get_config)
	{
        while (key = *pchTmp++)
            {
       	    ulConfigInfo = (ULONG) isc_vax_integer (pchTmp, sizeof (ULONG));
	    pchTmp += sizeof (ULONG);
	    switch (key)
	        {
	        case ISCCFG_IPCMAP_KEY:
		    lMapSize = ulConfigInfo;
		    break;
	        case ISCCFG_DBCACHE_KEY:
		    lCachePages = ulConfigInfo;
		    break;
  	        }
            }
        bSuccess = TRUE;
        }
    }

// Empty the Map Size Combo Box
SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_RESETCONTENT, 0, 0);

// Fill the Map Size Combo Box
SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_ADDSTRING, 0,(LPARAM)"1024");
SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_ADDSTRING, 0,(LPARAM)"2048");
SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_ADDSTRING, 0,(LPARAM)"4096");
SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_ADDSTRING, 0,(LPARAM)"8192");

// Select the right Map Size, inserting it if necessary
if (SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_SELECTSTRING, -1,
		(LPARAM)itoa(lMapSize, pchTmp, 10)) == CB_ERR)
    {
    SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_ADDSTRING, 0, (LPARAM)pchTmp);
    SendDlgItemMessage(hDlg, IDC_MAPSIZE, CB_SELECTSTRING, 0, (LPARAM)pchTmp);
    }

// Display the proper cache size
SetDlgItemInt(hDlg, IDC_DBPAGES, lCachePages, TRUE);

SetCursor(hOldCursor);
isc_service_detach(pdwStatus, &hService);

if (!bSuccess)
	{
	/* To eliminate the flicker while closing the Property Sheet */
	SetWindowPos(GetParent(hDlg), HWND_DESKTOP,
				  0, 0, 0, 0, SWP_HIDEWINDOW);
	/* Close the Property Sheet */
	SendMessage(GetParent(hDlg), WM_CLOSE, 0, 0);
	}

return bSuccess;
}

void RefreshIBControls(HWND hDlg, BOOL bEnable)
{
/******************************************************************************
 *
 *  R e f r e s h I B C o n t r o l s
 *
 ******************************************************************************
 *
 *  Input:  hDlg - Handle to the IB settings page dialog
 *          bEnable - Flag to indicate an enable/disable
 *
 *  Return: void
 *
 *  Description: This method enables or disables the page controls.
 *****************************************************************************/
char    szButtonName[32];

EnableWindow(GetDlgItem(hDlg, IDC_MAPSIZE), bEnable);
EnableWindow(GetDlgItem(hDlg, IDC_DBPAGES), bEnable);

LoadString(hAppInstance, bEnable ? IDS_RESETBTN : IDS_MODIFYBTN, szButtonName,
	sizeof(szButtonName));

SendDlgItemMessage(hDlg, IDC_MODRES, WM_SETTEXT, 0, (LPARAM) szButtonName);

// This modifies the bubble help table to bring up the appropriate help 
// topic based on whether the text is "Modify..." or "Reset".  It assumes
// that the first entry is the first one.
aMenuHelpIDs1[0,1] = bEnable ? ibs_reset : ibs_modify;

}

BOOL WriteIBSettings(HWND hDlg)
{
/******************************************************************************
 *
 *  W r i t e I B S e t t i n g s
 *
 ******************************************************************************
 *
 *  Input:  hDlg - Handle to the OS settings page dialog
 *
 *  Return: void
 *
 *  Description: This method calls the ISC_set_config() function to write the
 *               current OS settings in the config file.
 *****************************************************************************/
long            pdwStatus[STATUS_BUFLEN];
isc_svc_handle  hService = NULL;
char            pchSendBuf[SEND_BUFLEN];
char            *pchPtr;
short           *psLen;
char            szSpb[SPB_BUFLEN];      // Server parameter block
char            szResBuf[2];            // Response buffer
BOOL            bRetFlag = TRUE;

HCURSOR hOldCursor = NULL;

lCachePages = GetDlgItemInt(hDlg, IDC_DBPAGES, &bRetFlag, FALSE);
if (!bRetFlag || lCachePages < 50)
	{
	PrintCfgStatus(NULL, IDS_CFGSETIBERR, hDlg);
    return FALSE;
	}
lMapSize = GetDlgItemInt(hDlg, IDC_MAPSIZE, &bRetFlag, TRUE);
if (!bRetFlag || lMapSize < 1024)
    {
    SetFocus(GetDlgItem(hDlg, IDC_MAPSIZE));
	PrintCfgStatus(NULL, IDS_CFGSETIBERR, hDlg);
    return FALSE;
    }

// Encode the SPB for SysDba
FillSysdbaSPB(szSpb, szSysDbaPasswd);

// Build the complete name to the service
hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
pchPtr = szService + strlen(szService);
strcat(szService, "query_server");

// Attach to "query_server" service
isc_service_attach(pdwStatus, 0, szService, &hService, strlen(szSpb), szSpb);

*pchPtr = '\0';

if (pdwStatus[1])
    {
    SetCursor(hOldCursor);
    PrintCfgStatus(pdwStatus, IDS_CFGATTACH_FAILED, hDlg);

	/* To eliminate the flicker while closing the Property Sheet */
	SetWindowPos(GetParent(hDlg), HWND_DESKTOP,
				  0, 0, 0, 0, SWP_HIDEWINDOW);
	/* Close the Property Sheet */
	SendMessage(GetParent(hDlg), WM_CLOSE, 0, 0);

    return FALSE;
    }

// Fill in the Send buffer's header
pchPtr = pchSendBuf;
*pchPtr++ = isc_info_svc_set_config;
psLen = pchPtr;
pchPtr += sizeof(USHORT);

// Fill in all the records
*pchPtr++ = ISCCFG_IPCMAP_KEY;
*pchPtr++ = sizeof(long);
*(ULONG *) pchPtr = isc_vax_integer(&lMapSize, sizeof(long));
pchPtr += sizeof(long);

*pchPtr++ = ISCCFG_DBCACHE_KEY;
*pchPtr++ = sizeof(long);
*(ULONG *) pchPtr = isc_vax_integer(&lCachePages, sizeof(long));
pchPtr += sizeof(long);

*psLen = pchPtr - pchSendBuf - sizeof(short) - sizeof(char);
*psLen = isc_vax_integer(psLen, sizeof(short));
// Query service with set_config

isc_service_query(pdwStatus, &hService, NULL, 0, NULL, pchPtr - pchSendBuf,
	pchSendBuf, sizeof(szResBuf), szResBuf);
if (pdwStatus[1] || szResBuf[1])
    {
    PrintCfgStatus(pdwStatus[1] ? pdwStatus : NULL, IDS_CFGWRITE_FAILED, hDlg);

	/* To eliminate the flicker while closing the Property Sheet */
	SetWindowPos(GetParent(hDlg), HWND_DESKTOP,
				  0, 0, 0, 0, SWP_HIDEWINDOW);
	/* Close the Property Sheet */
	SendMessage(GetParent(hDlg), WM_CLOSE, 0, 0);

    bRetFlag = FALSE;
    }

isc_service_detach(pdwStatus, &hService);

SetCursor(hOldCursor);
return bRetFlag;
}

BOOL ValidateUser(HWND hParentWnd)
{
/******************************************************************************
 *
 *  V a l i d a t e U s e r
 *
 ******************************************************************************
 *
 *  Input:  hDlg - Handle to the Parent window
 *
 *  Return: TRUE if the user is the system administrator
 *          FALSE if the user failed the sys. admin validation
 *
 *  Description: This method calls the Password dialog to check if the user
 *               is the system administrator. However, if under server manager
 *               it just returns a FALSE if not already connected. 
 *****************************************************************************/
BOOL CALLBACK PasswordDlgProc(HWND, UINT, WPARAM, LPARAM);

if (!bServerApp)
    {
    PrintCfgStatus(NULL, IDS_CFGNOT_SYSDBA, hParentWnd);
    return FALSE;
    }
else
    {
    szSysDbaPasswd[0] = '\0';
    return (DialogBox((HINSTANCE) GetWindowLong(hParentWnd, GWL_HINSTANCE),
	    MAKEINTRESOURCE(PASSWORD_DLG), hParentWnd, PasswordDlgProc) > 0);
    }
}

BOOL CALLBACK PasswordDlgProc(HWND hDlg, UINT unMsg, WPARAM wParam, LPARAM lParam)
{
/******************************************************************************
 *
 *  P a s s w o r d D l g P r o c
 *
 ******************************************************************************
 *
 *  Input:  hDlg - Handle to the page dialog
 *          unMsg - Message ID
 *          wParam - WPARAM message parameter
 *          lParam - LPARAM message parameter
 *
 *  Return: FALSE if message is not processed
 *          TRUE if message is processed here
 *
 *  Description: This is the window procedure for the "Password" dialog box.
 *****************************************************************************/
switch (unMsg)
    {
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORBTN:
	{
	OSVERSIONINFO   OsVersionInfo;

	OsVersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (GetVersionEx ((LPOSVERSIONINFO) &OsVersionInfo) &&
		OsVersionInfo.dwMajorVersion < 4)
	    {
	    SetBkMode((HDC) wParam, TRANSPARENT);
	    return (LRESULT) hIBGrayBrush;
	    }
	}
	break;
    case WM_COMMAND:
	if (wParam == IDOK)
	    {
	    char                szPassword[PASSWORD_LEN];
	    long                pdwStatus[STATUS_BUFLEN];
	    isc_svc_handle      hService = NULL;
	    char                *pchPtr,
				szSpb[SPB_BUFLEN];
	    HCURSOR             hOldCursor = NULL;

	    szPassword[0] = '\0';

	    SendDlgItemMessage(hDlg, IDC_DBAPASSWORD, WM_GETTEXT, PASSWORD_LEN,
		    (LPARAM) szPassword);
			// Fill the Spb
	    FillSysdbaSPB(szSpb, szPassword);

	    // Attach service to check for password validity
	    hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
	    isc_service_attach(pdwStatus, 0, "query_server",
			       &hService, strlen(szSpb), szSpb);
	    SetCursor(hOldCursor);

	    if (pdwStatus[1])
		{
		szSysDbaPasswd[0] = '\0';
		PrintCfgStatus(NULL, IDS_CFGBAD_PASSWD, hDlg);
		}
	    else
		{
		isc_service_detach(pdwStatus, &hService);
		strcpy(szSysDbaPasswd, szPassword);
		EndDialog(hDlg, 1);
		}

	    SendDlgItemMessage(hDlg, IDC_DBAPASSWORD, EM_SETSEL, 0, (LPARAM) -1);
	    SetFocus(GetDlgItem(hDlg, IDC_DBAPASSWORD));
	    return TRUE;
	    }
	else 
	    {
	    if (wParam == IDCANCEL)
		{
		EndDialog(hDlg, 0);
		return TRUE;
		}
	    }
	break;
    case WM_CLOSE:
	{
	EndDialog(hDlg, 0);
	return TRUE;
	}
    default:
	return FALSE;
    }

return FALSE;
}

void PrintCfgStatus (STATUS *status_vector, int nErrCode, HWND hDlg)
{
/**************************************
 *
 *      P r i n t C f g S t a t u s
 *
 **************************************
 *
 * Functional description
 *      Print error message. Use isc_interprete
 *      to allow redirecting output.
 *
 **************************************/
STATUS  *vector;
SCHAR   szErrStr[ERR_BUFLEN];
SCHAR   szHdrStr[HDR_BUFLEN];

szErrStr[0] = '\0';

if (status_vector)
    {
    vector = status_vector;
    if (isc_interprete (szErrStr, &vector))
	{
	SCHAR* ptr = szErrStr + strlen(szErrStr);;

	*ptr++ = '\r'; *ptr++ = '\n'; *ptr++ = '-';
	while (isc_interprete (ptr, &vector))
	    {
	    ptr += strlen(ptr);
	    *ptr++ = '\r'; *ptr++ = '\n'; *ptr++ = '-';
	    }
	*(ptr-3) = '\0';
	}
    }

#ifdef GUI_TOOLS
	ShowErrDlg (nErrCode, szErrStr[0]? szErrStr: NULL, hDlg);
#else
if (szErrStr[0])
    {
    strcpy(szHdrStr, "IB Configuration - ");
    LoadString(hAppInstance, nErrCode, szHdrStr+strlen(szHdrStr),
	       sizeof(szHdrStr)-strlen(szHdrStr) );
    MessageBox(hDlg, szErrStr, szHdrStr, MB_ICONSTOP | MB_OK);
    }
else
    {
    LoadString(hAppInstance, nErrCode, szErrStr, sizeof(szErrStr));
    MessageBox(hDlg, szErrStr, "IB Configuration", MB_ICONSTOP | MB_OK);
    }
#endif
}

void FillSysdbaSPB(char* szSpb, char* szPasswd)
{
/**************************************
 *
 *      F i l l S y s d b a S P B
 *
 **************************************
 *
 * Functional description
 *      Fill in the SPB buffer for SYSDBA
 *
 **************************************/
*szSpb++ = isc_spb_version1;
*szSpb++ = isc_spb_user_name;
*szSpb++ = strlen("SYSDBA");
strcpy(szSpb, "SYSDBA");                        
szSpb += strlen(szSpb);
*szSpb++ = isc_spb_password;
*szSpb++ = strlen(szPasswd);
strcpy(szSpb, szPasswd);
}

void HelpCmd ( 
	 HWND           hWnd,
	 HINSTANCE      hInst,
	 WPARAM         wId)
{
/****************************************************************
 *                                              
 *  H e l p C m d
 *
/****************************************************************
 *
 *  Input:     hWnd     - Handle of dialog box from which help was
 *                        invoked.
 *              hInst   - Instance of application.
 *              nId       - The help message Id.
 *
 *  Description:  Invoke the Windows Help facility with context of nId.
 *      
 *****************************************************************/
HCURSOR         hOldCursor;
SCHAR           szPathFileName[1024 + 256 + 1];

GetModuleFileName(hInst, szPathFileName, sizeof(szPathFileName));

/* Show hour glass cursor */    
hOldCursor = SetCursor (LoadCursor (NULL, IDC_WAIT));

strcpy (strrchr(szPathFileName,'\\') +1, SERVER_HELP_FILE);
WinHelp (hWnd, szPathFileName, HELP_CONTEXT, wId);

/* Restore old cursor */
SetCursor (hOldCursor);

return;
}
