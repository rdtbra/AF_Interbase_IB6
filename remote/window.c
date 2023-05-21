/*
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
#include "../jrd/license.h"
#include "../jrd/isc.h"
#include "../jrd/file_params.h"
#include "../remote/remote_def.h"
#include "../remote/window.rh"
#include "../remote/property.rh"
#include "../ipserver/ips.h"

#include "../jrd/svc_proto.h"
#include "../jrd/thd.h"
#include "../jrd/jrd_proto.h"
#include "../remote/window_proto.h"
#include "../remote/propty_proto.h"
#include "../ipserver/ipsrv_proto.h"

#ifdef DEV_BUILD
#include "../jrd/gds_proto.h"
#endif

#include "../remote/window.h"

#define JRD_info_drivemask 1 // TEMPORARY ONLY 

char			szWindowText[WIN_TEXTLEN];
HWND			hPSDlg = NULL;
static HINSTANCE	hInstance = NULL;
static USHORT		usServerFlags;

static ULONG	WND_ipc_map;
static ULONG	WND_priority;
static ULONG	WND_working_min;
static ULONG	WND_working_max;
#ifdef SUPERSERVER
extern SLONG    free_map_debug;
extern SLONG    trace_pools;
#endif

static struct ipccfg	WND_hdrtbl [] = {
	ISCCFG_IPCMAP,	-1,	&WND_ipc_map,		0,	0,
	ISCCFG_PRIORITY,-1,	&WND_priority,		0,	0,
	ISCCFG_MEMMIN,	-1,	&WND_working_min,	0,	0,
	ISCCFG_MEMMAX,	-1,	&WND_working_max,	0,	0,
        ISCCFG_TRACE_POOLS, 0,  &trace_pools,           0,      0,
	NULL,		-1,	NULL,			0,	0
};

// Static functions to be called from this file only.
static char *GetDriveLetter(ULONG, char *);
static char *MakeVersionString(char *, int, USHORT);
static BOOL CanEndServer(HWND, BOOL);

#define SVC_CONFIG 4	  // Startup Configuration Entry point for ibcfg.exe.

// Window Procedure
void __stdcall WINDOW_shutdown(HANDLE);
LRESULT CALLBACK WindowFunc(HWND, UINT, WPARAM, LPARAM);


int WINDOW_main (
    HINSTANCE	hThisInst,
    int		nWndMode,
    USHORT	usServerFlagMask)
{
/******************************************************************************
 *
 *  W I N D O W _ m a i n
 *
 ******************************************************************************
 *
 *  Input:  hThisInst - Handle to the current instance
 *          nWndMode - The mode to be used in the call ShowWindow()
 *          usServerFlagMask - The Server Mask specifying various server flags
 *
 *  Return: (int) 0 if returning before entering message loop
 *          wParam if returning after WM_QUIT
 *
 *  Description: This function registers the main window class, creates the
 *               window and it also contains the message loop. This func. is a
 *               substitute for the regular WinMain() function.
 *****************************************************************************/
HWND		hWnd = NULL;
MSG		msg;
WNDCLASS	wcl;

hInstance = hThisInst;
usServerFlags = usServerFlagMask;

/* initialize interprocess server */

ISC_get_config(LOCK_HEADER, WND_hdrtbl);

if (!(usServerFlagMask & SRVR_ipc))
    szClassName = "IB_Disabled";
else
    {

#ifndef XNET

    if (!IPS_init( hWnd, 0, WND_ipc_map, 0))
	{
	// The initialization failed.  Check to see if there is another
	// server running.  If so, bring up it's property sheet and quit
	// otherwise assume that a stale client is still around and tell
	// the user to terminate it.
	char szMsgString[TMP_STRINGLEN];
	hWnd = FindWindow(szClassName, APP_NAME);
	if (hWnd)
	    {
	    LoadString(hInstance, IDS_ALREADYSTARTED, szMsgString, TMP_STRINGLEN);
    	    if (usServerFlagMask & SRVR_non_service)
	   	MessageBox(NULL, szMsgString, APP_NAME, MB_OK | MB_ICONHAND);
	    gds__log(szMsgString);
	    }
	else
	    {
	    LoadString(hInstance, IDS_MAPERROR, szMsgString, TMP_STRINGLEN);
    	    if (usServerFlagMask & SRVR_non_service)
	   	MessageBox(NULL, szMsgString, APP_NAME, MB_OK | MB_ICONHAND);
	    gds__log(szMsgString);
	    }
	return 0;
	}

#else

    if (!XNET_init( hWnd, 0, 0, 0))
        {
        char szMsgString[TMP_STRINGLEN];
        hWnd = FindWindow(szClassName, APP_NAME);
        if (hWnd)
	    {
	    LoadString(hInstance, IDS_ALREADYSTARTED, szMsgString, TMP_STRINGLEN);
    	    if (usServerFlagMask & SRVR_non_service)
	   	MessageBox(NULL, szMsgString, APP_NAME, MB_OK | MB_ICONHAND);
	    gds__log(szMsgString);
	    }
        else
            {
            LoadString(hInstance, IDS_MAPERROR, szMsgString, TMP_STRINGLEN);
            if (usServerFlagMask & SRVR_non_service)
                MessageBox(NULL, szMsgString, APP_NAME, MB_OK | MB_ICONHAND);
            gds__log(szMsgString);
            }
        return 0;
        }
#endif
    }

/* 
 * Call ISC_set_config with the values we just read to
 * make them take effect. 
*/
ISC_set_config(NULL, WND_hdrtbl);
WND_ipc_map /= 1024;

MakeVersionString(szWindowText, WIN_TEXTLEN, usServerFlagMask);

/* initialize main window */
wcl.hInstance     = hInstance;
wcl.lpszClassName = szClassName;
wcl.lpfnWndProc   = WindowFunc;
wcl.style	  = 0;
wcl.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_IBSVR));
wcl.hCursor       = LoadCursor(NULL, IDC_ARROW);
wcl.lpszMenuName  = NULL;
wcl.cbClsExtra    = 0;
wcl.cbWndExtra    = 0;

wcl.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

if(!RegisterClass(&wcl))
    {
    char szMsgString[MSG_STRINGLEN];
    LoadString(hInstance, IDS_REGERROR, szMsgString, MSG_STRINGLEN);
    if (usServerFlagMask & SRVR_non_service)
	MessageBox(NULL, szMsgString, APP_NAME, MB_OK);
    gds__log(szMsgString);
    return 0;
    }

hWnd = CreateWindowEx(
    0,
    szClassName,
    APP_NAME,
    WS_DLGFRAME | WS_SYSMENU | WS_MINIMIZEBOX,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    APP_HSIZE,
    APP_VSIZE,
    HWND_DESKTOP,
    NULL,
    hInstance,
    NULL );

#ifdef  __BORLANDC__
SVC_shutdown_init((STDCALL_FPTR_VOID)WINDOW_shutdown, (UINT)hWnd);
#else
SVC_shutdown_init(WINDOW_shutdown, (UINT)hWnd);
#endif

// Do the proper ShowWindow depending on if the app is an icon on
// the desktop, or in the task bar.

SendMessage(hWnd, WM_COMMAND, IDM_CANCEL, 0);
UpdateWindow(hWnd);

while(GetMessage(&msg, NULL, 0, 0))
    {
    if (hPSDlg) // If property sheet dialog is open
	{
	// Check if the message is property sheet dialog specific
	BOOL bPSMsg = PropSheet_IsDialogMessage(hPSDlg, &msg);

	// Check if the property sheet dialog is still valid, if not destroy it
	if (!PropSheet_GetCurrentPageHwnd(hPSDlg))
   	    {
	    DestroyWindow(hPSDlg);
	    hPSDlg = NULL;
	    }
	if (bPSMsg)
	    continue;
	}
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    }

return msg.wParam;
}


LRESULT CALLBACK WindowFunc (
    HWND	hWnd,
    UINT	message,
    WPARAM	wParam,
    LPARAM	lParam)
{
/******************************************************************************
 *
 *  W i n d o w F u n c
 *
 ******************************************************************************
 *
 *  Input:  hWnd - Handle to the window
 *          message - Message ID
 *          wParam - WPARAM parameter for the message
 *          lParam - LPARAM parameter for the message
 *
 *  Return: FALSE indicates that the message has not been handled
 *          TRUE indicates the message has been handled 
 *
 *  Description: This is main window procedure for the Interbase server. This
 *               traps all the interbase significant messages and processes
 *               them.
 *****************************************************************************/
static ULONG        ulLastMask = 0L;
static BOOLEAN      bInTaskBar = FALSE;
static BOOLEAN      bStartup   = FALSE;

ULONG			ulInUseMask = 0L;
PDEV_BROADCAST_VOLUME	pdbcv;
char			szDrives[DRV_STRINGLEN];
USHORT			num_att = 0;
USHORT			num_dbs = 0;

switch (message)
    {
    case WM_QUERYENDSESSION:
	/* If we are running as a non-service server, then query the user
	 * to determine if we should end the session.  Otherwise, assume that
	 * the server is a service  and could be servicing remote clients and
	 * therefore should not be shut down.  
	 */ 
	if (usServerFlags & SRVR_non_service)
	    return CanEndServer(hWnd, TRUE);
	else
	    return (TRUE);

    case WM_CLOSE:
	/* If we are running as a non-service server, then query the user
	 * to determine if we should end the session.  Otherwise, assume that
	 * the server is a service  and could be servicing remote clients and
	 * therefore should not be shut down.  The DestroyWindow() will destroy
	 * the hidden window created by the server for IPC.  This should get
	 * destroyed when the user session ends. 
	 */ 
	if (usServerFlags & SRVR_non_service)
	    {
	    if (CanEndServer(hWnd, FALSE))
		{
	    	if (GetPriorityClass(GetCurrentProcess()) != NORMAL_PRIORITY_CLASS)
		    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
#ifdef DEV_BUILD
		gds_alloc_report(ALLOC_verbose, "from server", 0);
#endif
		THREAD_ENTER;
		JRD_shutdown_all();
		DestroyWindow(hWnd);

		/* There is no THREAD_EXIT to help ensure that no
		 * threads will get in and try to access the data-
		 * structures we released in JRD_shutdown_all()
		 */
		}
	    }
	break;    

    case WM_COMMAND:
	switch (wParam)
	    {
	    case IDM_CANCEL:
 	        if ((usServerFlags & SRVR_non_service) && (!(usServerFlags & SRVR_no_icon)))
		    ShowWindow(hWnd, bInTaskBar ? SW_HIDE : SW_MINIMIZE);
		else
		    ShowWindow(hWnd, SW_HIDE);
		return TRUE;

	    case IDM_OPENPOPUP:
		{
	 	HMENU	hPopup = NULL;
		POINT	curPos;
    		char szMsgString[MSG_STRINGLEN];

		// The SetForegroundWindow() has to be called because our window
		// does not become the Foreground one (inspite of clicking on
		//the icon).  This is so because the icon is painted on the task
		//bar and is not the same as a minimized window.
		SetForegroundWindow(hWnd);

		hPopup = CreatePopupMenu();
    		LoadString(hInstance, IDS_SHUTDOWN, szMsgString, MSG_STRINGLEN);
		AppendMenu(hPopup, MF_STRING, IDM_SHUTDOWN, szMsgString);
    		LoadString(hInstance, IDS_PROPERTIES, szMsgString, MSG_STRINGLEN);
		AppendMenu(hPopup, MF_STRING, IDM_PROPERTIES, szMsgString);
		SetMenuDefaultItem(hPopup, IDM_PROPERTIES, FALSE);

		GetCursorPos(&curPos);
		TrackPopupMenu(hPopup, TPM_LEFTALIGN | TPM_RIGHTBUTTON, curPos.x, curPos.y, 0, hWnd, NULL);
		DestroyMenu(hPopup);
		return TRUE;
		}

	    case IDM_SHUTDOWN:
		SendMessage(hWnd, WM_CLOSE, 0, 0);
		return TRUE;

	    case IDM_PROPERTIES:
		if (!hPSDlg)
		    hPSDlg = DisplayProperties(hWnd, hInstance, usServerFlags);
		else
		    SetForegroundWindow(hPSDlg);
	    	return TRUE;

	    case IDM_GUARDED:
		{
		/* Since we are going to be guarded, we do not need to
		 * show the server icon.  The guardian will show its own. */
	        NOTIFYICONDATA  nid;
    	    	nid.cbSize     = sizeof(NOTIFYICONDATA); 
	    	nid.hWnd       = hWnd; 
	    	nid.uID        = IDI_IBSVR;
	    	nid.uFlags     = 0; 
	        Shell_NotifyIcon(NIM_DELETE, &nid); 
		}
		SendMessage((HWND) lParam, WM_COMMAND, (WPARAM) IDM_SET_SERVER_PID, 
			    (LPARAM) GetCurrentProcessId());
		return TRUE;
	    }
	break;

    	case ON_NOTIFYICON:
	    if (bStartup)
		{
		SendMessage(hWnd, WM_COMMAND, 0, 0);
		return TRUE;
		}
	    switch (lParam)
	    	{
	    	POINT	curPos;

	    	case WM_LBUTTONDOWN:
		    break;

	    	case WM_LBUTTONDBLCLK:
		    PostMessage(hWnd, WM_COMMAND, (WPARAM) IDM_PROPERTIES, 0);
		    break;

	    	case WM_RBUTTONUP:
		    // The TrackPopupMenu() is inconsistant if called from here? 
		    // This is the only way I could make it work.
		    PostMessage(hWnd, WM_COMMAND, (WPARAM) IDM_OPENPOPUP, 0);
		    break;
	    	}
	    break;

    	case WM_CREATE:
 	    if ((usServerFlags & SRVR_non_service) && (!(usServerFlags & SRVR_no_icon)))
		{ 
	        HICON           hIcon;
	        NOTIFYICONDATA  nid;

            	hIcon = LoadImage(hInstance, 
				  MAKEINTRESOURCE(IDI_IBSVR_SMALL),
				  IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);

    	    	nid.cbSize     = sizeof(NOTIFYICONDATA); 
	    	nid.hWnd       = hWnd; 
	    	nid.uID        = IDI_IBSVR;
	    	nid.uFlags     = NIF_TIP | NIF_ICON | NIF_MESSAGE; 
	    	nid.uCallbackMessage = ON_NOTIFYICON; 
	    	nid.hIcon      = hIcon;

	    	lstrcpy(nid.szTip, APP_NAME);
	        lstrcat(nid.szTip, "-");
	        lstrcat(nid.szTip, GDS_VERSION);
	
	    	// This will be true in the explorer interface 
	        bInTaskBar = Shell_NotifyIcon(NIM_ADD, &nid); 
	 
	        if (hIcon) 
		    DestroyIcon(hIcon);

		// This will be true in the Program Manager interface.
	    	if (!bInTaskBar) 
	            {
    		    char szMsgString[MSG_STRINGLEN];

	    	    HMENU hSysMenu = GetSystemMenu(hWnd, FALSE);
	    	    DeleteMenu(hSysMenu, SC_RESTORE, MF_BYCOMMAND);
	    	    AppendMenu(hSysMenu, MF_SEPARATOR, 0, NULL);
    		    LoadString(hInstance, IDS_SHUTDOWN, szMsgString, MSG_STRINGLEN);
		    AppendMenu(hSysMenu, MF_STRING, IDM_SHUTDOWN, szMsgString);
   		    LoadString(hInstance, IDS_PROPERTIES, szMsgString, MSG_STRINGLEN);
		    AppendMenu(hSysMenu, MF_STRING, IDM_PROPERTIES, szMsgString);
	  	    DestroyMenu(hSysMenu);
	   	    }
		}
	    break;

    	case WM_QUERYOPEN:
	    if (!bInTaskBar)
	   	return FALSE;
	    return DefWindowProc(hWnd, message, wParam, lParam); 
	case WM_SYSCOMMAND:
	    if (!bInTaskBar)
	   	switch (wParam)
		    {
		    case SC_RESTORE:
		    	return TRUE;

		    case IDM_SHUTDOWN:
		   	PostMessage(hWnd, WM_CLOSE, 0, 0);
		    	return TRUE;

		    case IDM_PROPERTIES:
		   	if (!hPSDlg)
		    	    hPSDlg = DisplayProperties(hWnd, hInstance, usServerFlags);
			else
		    	    SetFocus(hPSDlg);
		    	return TRUE;
		    }
		return DefWindowProc(hWnd, message, wParam, lParam);

    	case WM_DESTROY:
	    if (bInTaskBar)
	        {
	        NOTIFYICONDATA	nid;

	    	nid.cbSize = sizeof(NOTIFYICONDATA); 
	    	nid.hWnd   = hWnd; 
	    	nid.uID    = IDI_IBSVR; 
	    	nid.uFlags = 0;

	    	Shell_NotifyIcon(NIM_DELETE, &nid); 
	    	}

	    PostQuitMessage(0);
	    break;

    	case IP_CONNECT_MESSAGE:
#ifndef XNET
            return IPS_start_thread( lParam);
#else
            return SRVR_xnet_start_thread( lParam);
#endif

    	case WM_DEVICECHANGE:
	    pdbcv = (PDEV_BROADCAST_VOLUME) lParam;
	    JRD_num_attachments(&ulInUseMask, sizeof(ULONG), JRD_info_drivemask, &num_att, &num_dbs);

	    switch (wParam)
	        {

	    	case DBT_DEVICEARRIVAL:
		    return TRUE;

	    	case DBT_DEVICEQUERYREMOVE:
		    if (CHECK_VOLUME(pdbcv) && (ulLastMask != pdbcv->dbcv_unitmask))
		    	if (CHECK_USAGE(pdbcv))
			    {
			    char tmp[TMP_STRINGLEN];
			    char *p = tmp;
			    int len;

			    len = LoadString(hInstance, IDS_PNP1, p, TMP_STRINGLEN);
			    p += len;
			    *p++ = '\r';
			    *p++ = '\n';

			    len = LoadString(hInstance, IDS_PNP2, p, p - tmp + TMP_STRINGLEN);
			    p += len;
			    *p++ = '\r';
			    *p++ = '\n';
			    len = LoadString(hInstance, IDS_PNP3, p, p - tmp + TMP_STRINGLEN);
			    ulLastMask = pdbcv->dbcv_unitmask;
			    GetDriveLetter(pdbcv->dbcv_unitmask, szDrives);
			    if (MessageBox(hWnd, tmp,
					   szDrives,
					   MB_OKCANCEL | MB_ICONHAND) == IDCANCEL)
			    	return FALSE;

			    JRD_shutdown_all();
			    PostMessage(hWnd, WM_DESTROY, 0, 0);
			    return TRUE;
			    }
		   /* Fall through to MOVEPENDING if we receive a QUERYDEVICE for the
          	    * same device twice.  This will occur if we say yes to the removal
          	    * of a controller.  The OS will prompt for the removal of all 
          	    * devices connected to that controller.  If you respond no, the
  	       	    * OS will prompt you again, and then remove the device anyway...
          	    */
	    	case DBT_DEVICEREMOVEPENDING:
		    if (CHECK_VOLUME(pdbcv) && CHECK_USAGE(pdbcv))
			{
			char tmp[TMP_STRINGLEN];
			char *p = tmp;
			int len;

			len = LoadString(hInstance, IDS_PNP1, p, TMP_STRINGLEN);
			p += len;
			*p++ = '\r';
			*p++ = '\n';
			len = LoadString(hInstance, 
		 			 IDS_PNP2, 
		 			 p, 
		 			 TMP_STRINGLEN-(p-tmp));
			GetDriveLetter(pdbcv->dbcv_unitmask, szDrives);
			MessageBox(hWnd, 
	 			   tmp,
				   szDrives,
				   MB_OK | MB_ICONHAND);
			JRD_shutdown_all();
			PostMessage(hWnd, WM_DESTROY, 0, 0);
		        }
		    return TRUE;

	    	case DBT_DEVICEQUERYREMOVEFAILED:
		    return TRUE;

	    	case DBT_DEVICEREMOVECOMPLETE:
		    return TRUE;

	    	case DBT_DEVICETYPESPECIFIC:
		    return TRUE;

	    	case DBT_CONFIGCHANGED:
		    return TRUE;
	    	}
	    return TRUE;
	         
        default:
	    return DefWindowProc(hWnd, message, wParam, lParam);
    }
return FALSE;
}

void __stdcall WINDOW_shutdown(HANDLE hWnd)
{
/******************************************************************************
 *
 *  W I N D O W _ s h u t d o w n
 *
 ******************************************************************************
 *
 *  Input:  hWnd - Handle to the window
 *
 *  Return: none
 *
 *  Description: This is a callback function which is called at shutdown time.
 *               This function post the WM_DESTROY message in appl. queue.
 *****************************************************************************/
PostMessage(hWnd, WM_DESTROY, 0, 0);
}

static char *GetDriveLetter(ULONG ulDriveMask, char *pchBuf)
{
/******************************************************************************
 *
 *  G e t D r i v e L e t t e r
 *
 ******************************************************************************
 *
 *  Input:  ulDriveMask - Bit flag mask encoding the drive letters
 *          pchBuf - Buffer to be filled
 *
 *  Return: Buffer containing the drive letters
 *
 *  Description: This function checks for the drives in Drive mask and returns
 *               the buffer filled with the drive letters which are on.
 *****************************************************************************/
char chDrive = 'A';
char *p = pchBuf;

while (ulDriveMask)
    {
    if (ulDriveMask & 1)
	*p++ = chDrive;
    chDrive++;
    ulDriveMask >>= 1;
    }
*p = '\0';
return pchBuf;
}

static char *MakeVersionString(char *pchBuf, int nLen, USHORT usServerFlagMask)
{
/******************************************************************************
 *
 *  M a k e V e r s i o n S t r i n g
 *
 ******************************************************************************
 *
 *  Input:  pchBuf - Buffer to be filled into
 *          nLen - Length of the buffer
 *          usServerFlagMask - Bit flag mask encoding the server flags
 *
 *  Return: Buffer containing the version string
 *
 *  Description: This method is called to get the Version String. This string
 *               is based on the flag set in usServerFlagMask.
 *****************************************************************************/
char* p = pchBuf;
char* end = p + nLen;
int   l;

if (usServerFlagMask & SRVR_inet)
    {
    p += LoadString(hInstance, IDS_TCP, p, end - p);
    if (p < end)
	*p++ = '\r';
    if (p < end)
	*p++ = '\n';
    }
if (usServerFlagMask & SRVR_pipe)
    {
    p += LoadString(hInstance, IDS_NP, p, end - p);
    if (p < end)
        *p++ = '\r';
    if (p < end)
        *p++ = '\n';
    }
if (usServerFlagMask & SRVR_ipc)
    {
    p += LoadString(hInstance, IDS_IPC, p, end - p);
    }
return pchBuf;
}

BOOL CanEndServer(HWND hWnd, BOOL bSysExit)
{
/******************************************************************************
 *
 *  C a n E n d S e r v e r
 *
 ******************************************************************************
 *
 *  Input:  hWnd     - Handle to main application window.
 *	    bSysExit - Flag indicating if the system is going down.
 *
 *  Return: FALSE if user does not want to end the server session
 *          TRUE if user confirmed on server session termination
 *
 *  Description: This function displays a message mox and queries the user if
 *               the server can be shutdown.
 *****************************************************************************/
char szMsgString[MSG_STRINGLEN];
USHORT	usNumAtt;
USHORT  usNumDbs;

JRD_num_attachments(NULL, 0, 0, &usNumAtt, &usNumDbs);
	
sprintf(szMsgString, "%lu ", usNumAtt);

if (!usNumAtt) /* IF 0 CONNECTIONS, JUST SHUTDOWN */
    return TRUE;

LoadString(hInstance, IDS_QUIT, szMsgString + strlen(szMsgString),
                        MSG_STRINGLEN - strlen(szMsgString));
return (MessageBox(hWnd, szMsgString, APP_NAME,
                        MB_ICONQUESTION | MB_OKCANCEL) == IDOK);
}
