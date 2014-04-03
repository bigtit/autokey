#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "vk.h"
#include "resource.h"

#define WM_TRAY WM_USER+1
#define WM_ONOFF WM_USER+2
#define WM_EDITSHOW WM_USER+3

HINSTANCE inst;
HWND dlg;
WNDPROC edit3proc;
WNDPROC edit1proc;

// cfg file
char cfg[MAX_PATH] = "";
char itv[5] = "400";
int vk = 0;
char key[20] = "R"; // for display
int vsk = 0;
char skey[20] = "F12"; // for display

// custom proc func
LRESULT CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK Edit1Proc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK Edit3Proc(HWND, UINT, WPARAM, LPARAM);

// local func
BOOL GetCfg();
BOOL SetCfg();
BOOL ChkPrm();
BOOL vk2s(char, char*);
BOOL IsNum(char*);     // if string is natrual num
BOOL SetTray(HWND, PNOTIFYICONDATA);
BOOL RemoveTray(HWND, PNOTIFYICONDATA);
BOOL SetTip(char*, char*, HWND, PNOTIFYICONDATA);

// load dll dynamically
/*
HMODULE module;
BOOL (__stdcall *SetHook)(int, int, char);
BOOL (__stdcall *SetDlg)(HWND);
BOOL (__stdcall *SetSkey)(char);
*/


////////////////////////////////////////////////
/////////////////add from hk.dll////////////////
////////////////////////////////////////////////
// global and shared data
#pragma data_seg("hookdata")
int hitv = 0; // interval
char ky = 0; // key
char sk = 0;
int md = 0; // mode
HWND dlg = 0; // save dlg handler
#pragma data_seg()

// no shared
HHOOK hook = 0; // main hook
HINSTANCE inst = 0; // dll instance
HWND wnd = 0; // target wnd
BOOL ton = 0; // is timer on
int at = 0; // 2 mode switching
UINT_PTR tid = 0; // timer id

LRESULT CALLBACK llkproc(int, WPARAM, LPARAM);
LRESULT CALLBACK tproc(HWND, UINT, UINT, DWORD);
BOOL SetDlg(HWND hdlg);
BOOL SetSkey(char skey);
BOOL SetHook(int mode, int iterval, char key);

/////////////////////////////////////////////////


int APIENTRY WinMain(HINSTANCE hinst, HINSTANCE phinst, LPSTR lpcmdline, int icmdshow)
{
  // save global instance of the app
  inst = hinst;
  // load cfg file
  GetCfg();

  // load dll and func
  /*
  module = LoadLibrary(TEXT("hk.dll"));
  if(!module){
    MessageBox(0, "Could not load hk.dll", "Error", 0);
    return -1;
  }
  SetHook = (BOOL (__stdcall *)(int, int, char))GetProcAddress(module, "SetHook");
  SetDlg = (BOOL (__stdcall *)(HWND))GetProcAddress(module, "SetDlg");
  SetSkey = (BOOL (__stdcall *)(char))GetProcAddress(module, "SetSkey");

  if(!SetHook || !SetDlg || !SetSkey){
    MessageBox(0, "Could not load funcs", "Error", 0);
    return -1;
  }
  */

  // create dialog to send WM_INITDIALOG msg
  DialogBoxParam(hinst, MAKEINTRESOURCE(IDD_MAIN), 0, (DLGPROC)DlgProc, 0);

  //FreeLibrary(module);
  SetCfg();

  return 0;
}

LRESULT CALLBACK DlgProc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
  NOTIFYICONDATA tray, ballon;
  static HMENU menu;
  POINT pt;
  int wid;
  char tip[100];
  const UINT WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
  // local global
  sprintf_s(tip, 100, "Key: %s\nInterval: %s ms\nSKey: %s", key, itv, skey);

  switch(msg){
    case WM_CREATE:
      SetTray(hdlg, &tray);
      break;
    case WM_INITDIALOG:
      // send hdlg to dll
      SetDlg(hdlg);
      // save dlg handler
      dlg = hdlg;
      // load tray and menu
      SetTray(hdlg, &tray);
      menu = LoadMenu(inst, MAKEINTRESOURCE(IDC_TRAY));
      menu = GetSubMenu(menu, 0);

      // editbox contents
      SetDlgItemText(hdlg, IDC_EDIT1, key);
      SetDlgItemText(hdlg, IDC_EDIT2, itv);
      SetDlgItemText(hdlg, IDC_EDIT3, skey);
      // editbox settings
      SendDlgItemMessage(hdlg, IDC_EDIT1, EM_LIMITTEXT, 1, 0);
      SendDlgItemMessage(hdlg, IDC_EDIT2, EM_LIMITTEXT, 4, 0);
      SendDlgItemMessage(hdlg, IDC_EDIT3, EM_LIMITTEXT, 1, 0);
      // editbox processors
      edit1proc = (WNDPROC)SetWindowLong(GetDlgItem(hdlg, IDC_EDIT1), 
        GWL_WNDPROC, (LONG)Edit1Proc);
      edit3proc = (WNDPROC)SetWindowLong(GetDlgItem(hdlg, IDC_EDIT3), 
        GWL_WNDPROC, (LONG)Edit3Proc);

      // start hook directly
      if(SetHook(1, atoi(itv), vk)){
        SetSkey(vsk);
        SetTip(tip, "Manual", hdlg, &ballon);
      }

      SetActiveWindow(hdlg);
      
      return 1;
    case WM_TRAY:
      if(lp==WM_RBUTTONDOWN){
        GetCursorPos(&pt);
        SetForegroundWindow(hdlg);
        TrackPopupMenuEx(menu, TPM_CENTERALIGN, pt.x, pt.y, hdlg, 0);
      }
      else if(lp==WM_LBUTTONDOWN)
        SendMessage(hdlg, WM_COMMAND, IDC_BUTTON2, lp);
      break;
    case WM_COMMAND:
      wid = LOWORD(wp);
      if(wid==IDC_BUTTON1){
        // getting phase with checking
        GetDlgItemText(hdlg, IDC_EDIT2, itv, 5);
        ChkPrm();
        // setting phase
        // need reboot hook
        sprintf_s(tip, 100, "Key: %s\nInterval: %s ms\nSKey: %s", key, itv, skey);
        SetDlgItemText(hdlg, IDC_EDIT2, itv);

        SetHook(0, 0, 0);
        if(SetHook(1, atoi(itv), vk)){
          SetSkey(vsk);
          SetTip(tip, "Manual", hdlg, &ballon);
        }
        else SetTip(tip, "Auto", hdlg, &ballon);
      }
      else if(wid==IDM_TRAY_EXT) SendMessage(hdlg, WM_CLOSE, wp, lp);
      else if(wid==IDM_TRAY_ABO) SetTip("2013-9-1\nTsubasa9\nVersion: 2.1 lite", "About", hdlg, &ballon);
      else if(wid==IDM_TRAY_SET || wid==IDC_BUTTON2){
        if(!IsWindowVisible(hdlg)){
          ShowWindow(hdlg, SW_SHOW);
          CheckMenuItem(menu, IDM_TRAY_SET, MF_CHECKED);
        }
        else{
          ShowWindow(hdlg, SW_HIDE);
          CheckMenuItem(menu, IDM_TRAY_SET, MF_UNCHECKED);
        }
      }
      break;
    case WM_ONOFF:
      if(!wp) SetTip(tip, "Manual", hdlg, &ballon);
      else SetTip(tip, "Auto", hdlg, &ballon);
      break;
    case WM_EDITSHOW:
      if(!wp) SetDlgItemText(hdlg, IDC_EDIT1, key);
      else SetDlgItemText(hdlg, IDC_EDIT3, skey);
      break;
    case WM_CLOSE:
      SetHook(0, 0, 0);
      RemoveTray(hdlg, &tray);
      EndDialog(hdlg, 0);
      break;
    default:
      if(msg==WM_TASKBARCREATED)
        SendMessage(hdlg, WM_CREATE, wp, lp);
      break;
  }
  return 0;
}



///////////////////////////////////////////
BOOL GetCfg()
{
  int len;
  if(!cfg) return 0;
  len = GetCurrentDirectory(0, NULL);
  GetCurrentDirectory(len, cfg);
  lstrcat(cfg, "\\aky.ini");

  vk = GetPrivateProfileInt("aky", "key", 82, cfg);
  vsk = GetPrivateProfileInt("aky", "skey", 123, cfg);
  GetPrivateProfileString("aky", "interval", "400", itv, 5, cfg);

  if(!ChkPrm()) return 0;

  vk2s(vk, key);
  vk2s(vsk, skey);

  return 1;
}

BOOL SetCfg()
{
  int len;
  len = GetCurrentDirectory(0, NULL);
  GetCurrentDirectory(len, cfg);
  lstrcat(cfg, "\\aky.ini");

  if(!ChkPrm()) return 0;

  _itoa_s(vk, key, 20, 10);
  _itoa_s(vsk, skey, 20, 10);
  WritePrivateProfileString("aky", "key", key, cfg);
  WritePrivateProfileString("aky", "skey", skey, cfg);
  WritePrivateProfileString("aky", "interval", itv, cfg);

  return 1;
}

// check parameters
BOOL ChkPrm()
{
  // check&correct key[], itv[], mode, cfg
  int intv = atoi(itv);
  if(!cfg) return 0;
  if(!IsNum(itv) || intv>9999 || intv<1)
    lstrcpy(itv, "400");
  if(vk<8 || vk>254) vk = 87;
  if(vsk<8 || vsk>254) vsk = 123;

  return 1;
}


// whether a c-style string is a number
BOOL IsNum(char* nu)
{
  int i = 0;
  if(nu[0]=='\0')
    return 0;
  while(nu[i]!='\0'){
    if(!isdigit(nu[i++])) return 0;
  }
  return 1;
}

///////////////////////////////////////////////////////

// processor for editboxes
LRESULT CALLBACK Edit1Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  if(msg==WM_KEYDOWN){
    vk = wp;
    vk2s(vk, key);
    SendMessage(dlg, WM_EDITSHOW, 0, 0);
    return 0;
  }
  return CallWindowProc(edit1proc, hwnd, msg, wp, lp);
}

LRESULT CALLBACK Edit3Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
  if(msg==WM_KEYDOWN){
    vsk = wp;
    vk2s(vsk, skey);
    SendMessage(dlg, WM_EDITSHOW, 1, 0);
    return 0;
  }
  return CallWindowProc(edit3proc, hwnd, msg, wp, lp);
}


////////////////////////////////////////////////////
// show tray icon
BOOL SetTray(HWND hwnd, PNOTIFYICONDATA tray)
{
  SecureZeroMemory(tray, sizeof(NOTIFYICONDATA));
  tray->cbSize = sizeof(tray);
  tray->hWnd = hwnd;
  tray->uID = 0;
  tray->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

  tray->uCallbackMessage = WM_TRAY;
  tray->hIcon = LoadIcon(inst, (LPCTSTR)IDI_STAR);

  lstrcpy(tray->szTip, "AutoKey");
  lstrcpy(tray->szInfo, "AutoKey");
  lstrcpy(tray->szInfoTitle, ":)");
  tray->dwInfoFlags = NIIF_NONE;
  // show tray
  return Shell_NotifyIcon(NIM_ADD, tray);
}

BOOL RemoveTray(HWND hwnd, PNOTIFYICONDATA tray)
{
  if(!tray) return 0;
  tray->uID = 0;
  tray->hWnd = hwnd;
  return Shell_NotifyIcon(NIM_DELETE, tray);
}

// balloon tip
// show specific tray tip
BOOL SetTip(char* tip, char* title, HWND hwnd, PNOTIFYICONDATA ballon)
{
  SecureZeroMemory(ballon, sizeof(NOTIFYICONDATA));
  ballon->cbSize = NOTIFYICONDATA_V2_SIZE;
  ballon->hWnd = hwnd;
  ballon->uFlags = NIF_INFO;
  ballon->uVersion = NOTIFYICON_VERSION;
  ballon->dwInfoFlags = NIIF_INFO;
  
  lstrcpy(ballon->szInfoTitle, title);
  lstrcpy(ballon->szInfo, tip);

  return Shell_NotifyIcon(NIM_MODIFY, ballon);
}

BOOL vk2s(char vk, char* s)
{
  memset(s, 0, 20);
  lstrcpy(s, vkcode[vk]);
  return 0;
}

///////////////////////////////////////////////
//////////////add from hk.dll//////////////////
///////////////////////////////////////////////


LRESULT CALLBACK llkproc(int code, WPARAM wp, LPARAM lp)
{
  POINT pt;
  PKBDLLHOOKSTRUCT ks = (PKBDLLHOOKSTRUCT)lp;
  BOOL kup = WM_KEYUP==wp || WM_SYSKEYUP==wp;
  BOOL kdn = WM_KEYDOWN==wp || WM_SYSKEYDOWN==wp;
  GetCursorPos(&pt);

  // the case of swithing window when timer is on
  wnd = WindowFromPoint(pt);
  
  if(!wnd || wnd != FindWindow("KGWin32App", "剑侠情缘网络版叁")){
    if(ton){
      KillTimer(0, tid);
      // SendMessage(dlg, WM_ONOFF, 0, 0);
      ton = 0;
      at = 0;
    }
    return CallNextHookEx(hook, code, wp, lp);
  }
  

  if(code!=HC_ACTION)
    return CallNextHookEx(hook, code, wp, lp);
  
  // switch mode
  if(md==1 && kup && ks->vkCode==sk){
    if(!ton){
      keybd_event(ky, 0, 0, 0);
      keybd_event(ky, 0, KEYEVENTF_KEYUP, 0);
      tid = SetTimer(0, 0, hitv, tproc);
      SendMessage(dlg, WM_ONOFF, 1, 0);
      ton = 1;
      at = 1;
    }
    else{
      KillTimer(0, tid);
      SendMessage(dlg, WM_ONOFF, 0, 0);
      ton = 0;
      at = 0;
    }
    return 1;
  }
  // holding mode
  else if(md==1 && ks->scanCode==MapVirtualKey(ky, MAPVK_VK_TO_VSC)){
    if(!ton && kdn){
      if(at) SendMessage(dlg, WM_ONOFF, 0, 0);
      keybd_event(ky, 0, 0, 0);
      keybd_event(ky, 0, KEYEVENTF_KEYUP, 0);
      tid = SetTimer(0, 0, hitv, tproc);
      ton = 1;
      at = 0;
    }
    if(ton && kup){
      KillTimer(0, tid);
      ton = 0;
      at = 0;
    }
    return 1;
  }

  return CallNextHookEx(hook, code, wp, lp);
}

LRESULT CALLBACK tproc(HWND hwnd, UINT msg, UINT id, DWORD time)
{
  // keybd_event is avalible in jx3 with scan_code as 2nd parameter
  // 0 in 3rd parameter means KEYEVENTF_KEYDOWN
  keybd_event(ky, 0, 0, 0);
  keybd_event(ky, 0, KEYEVENTF_KEYUP, 0);
  return 1;
}

BOOL SetDlg(HWND hdlg)
{
  dlg = hdlg;

  return 0;
}
BOOL SetSkey(char skey)
{
  sk = skey;
  return 0;
}
BOOL SetHook(int mode, int iterval, char key)
{
  //int lerr;
  //char cinst[10];
  BOOL ok;
  hitv = iterval;
  ky = key;
  md = mode;

  // timer should be off
  ton = 0;

  if(md){
    // if(wnd) return 0;
    hook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)llkproc, inst, 0);
    ok=(hook!=0);
  }
  else{
    ok = UnhookWindowsHookEx(hook);
    if(ok){
      KillTimer(0, tid);
      hook = 0;
      // wnd = 0;
    }
  }
  return ok;
}
