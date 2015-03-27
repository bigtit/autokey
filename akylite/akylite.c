#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "vk.h"
#include "resource.h"

#define WM_TRAY WM_USER+1
#define WM_SWCH WM_USER+2
#define WM_EDIT WM_USER+3

// compact revision from original akylite.c
// with rationalized var/func names and structures

HINSTANCE g_hinst;
HWND g_hdlg;
WNDPROC edit1p;
WNDPROC edit3p;

HWND text1;
HWND text2;
HWND text3;

// shared hook data
#pragma data_seg("hookdata")
int g_itv;
char g_vk;
char g_vsk;
HHOOK g_hook = 0;
UINT_PTR timer0 = 0;
#pragma data_seg()

// funcs
BOOL read_cfg();
BOOL write_cfg();
LRESULT CALLBACK dlg_proc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK edit1_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK edit3_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
BOOL set_tray(HWND hwnd, PNOTIFYICONDATA tray);
BOOL rm_tray(HWND hwnd, PNOTIFYICONDATA tray);
BOOL set_tip(HWND hwnd, PNOTIFYICONDATA ballon, char* caption, char* content);
LRESULT CALLBACK llk_proc(int, WPARAM, LPARAM);
LRESULT CALLBACK timer_proc(HWND, UINT, UINT, DWORD);
BOOL sethk(int, char, char);
BOOL rmhk();
BOOL vk2str(char, char*);

// main callback loop
int APIENTRY WinMain(HINSTANCE hinst, HINSTANCE pre_hinst, LPSTR lpcmd, int icmdshow){
  HANDLE mutex = CreateMutex(0, 0, "sadopqng;ands5");
  if(GetLastError()==ERROR_ALREADY_EXISTS){
    MessageBox(0, "Only one instance could be executed.", "Error", MB_ICONWARNING);
    CloseHandle(mutex);
    mutex = 0;
    return -1;
  }
  g_hinst = hinst;
  read_cfg();
  DialogBoxParam(hinst, MAKEINTRESOURCE(IDD_MAIN), 0, (DLGPROC)dlg_proc, 0);
  write_cfg();
  return 0;
}

LRESULT CALLBACK dlg_proc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp){
  NOTIFYICONDATA tray, ballon;
  static HMENU menu;
  POINT pt;
  int wid;
  char tip[100];
  char buf[16];
  const UINT WM_TASKBARCREATED = RegisterWindowMessage(TEXT("TaskbarCreated"));
  sprintf_s(tip, 100, "Key: %d\nInterval: %d ms\nSKey: %d", g_vk, g_itv, g_vsk);
  switch(msg){
    case WM_CREATE:
      set_tray(hdlg, &tray);
      break;
    case WM_INITDIALOG:
      g_hdlg = hdlg;
      set_tray(hdlg, &tray);
      menu = LoadMenu(g_hinst, MAKEINTRESOURCE(IDC_TRAY));
      menu = GetSubMenu(menu, 0);

      vk2str(g_vk, buf);
      SetDlgItemText(hdlg, IDC_EDIT1, buf);
      vk2str(g_vsk, buf);
      SetDlgItemText(hdlg, IDC_EDIT3, buf);
      _itoa_s(g_itv, buf, 16, 10);
      SetDlgItemText(hdlg, IDC_EDIT2, buf);

      SendDlgItemMessage(hdlg, IDC_EDIT1, EM_LIMITTEXT, 1, 0);
      SendDlgItemMessage(hdlg, IDC_EDIT2, EM_LIMITTEXT, 4, 0);
      SendDlgItemMessage(hdlg, IDC_EDIT3, EM_LIMITTEXT, 1, 0);

      text1 = GetDlgItem(hdlg, IDC_EDIT1);
      text2 = GetDlgItem(hdlg, IDC_EDIT2);
      text3 = GetDlgItem(hdlg, IDC_EDIT3);

      edit1p = (WNDPROC)SetWindowLong(text1, 
        GWL_WNDPROC, (LONG)edit1_proc);
      edit3p = (WNDPROC)SetWindowLong(text3, 
        GWL_WNDPROC, (LONG)edit3_proc);

      // start hook
      if(sethk(g_itv, g_vk, g_vsk))
        set_tip(hdlg, &ballon, "Hook started", tip);
      SetActiveWindow(hdlg);
      return 1;
    case WM_TRAY:
      if(lp==WM_RBUTTONDOWN){
        GetCursorPos(&pt);
        SetForegroundWindow(hdlg);
        TrackPopupMenuEx(menu, TPM_CENTERALIGN, pt.x, pt.y, hdlg, 0);
      }else if(lp==WM_LBUTTONDOWN)
        SendMessage(hdlg, WM_COMMAND, IDC_BUTTON2, lp);
      break;
    case WM_COMMAND:
      wid = LOWORD(wp);
      if(wid==IDC_BUTTON1){
        GetDlgItemText(hdlg, IDC_EDIT2, buf, 5);
        g_itv = atoi(buf);
        if(g_itv>9999 || g_itv<1) g_itv = 400;
        sprintf_s(tip, 100, "Key: %d\nInterval: %d ms\nSKey: %d", g_vk, g_itv, g_vsk);
        
        // no need to restart hook anymore
        if(sethk(g_itv, g_vk, g_vsk))
          set_tip(hdlg, &ballon, "Hook set", tip);
      }else if(wid==IDM_TRAY_EXT) SendMessage(hdlg, WM_CLOSE, wp, lp);
      else if(wid==IDM_TRAY_ABO) set_tip(hdlg, &ballon, "About", "20150321\nTsubasa9\nVersion 3.0 lite");
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
    case WM_SWCH:
      if(!wp) set_tip(hdlg, &ballon, "Auto off", tip);
      else set_tip(hdlg, &ballon, "Auto on", tip);
      break;
    case WM_EDIT:
      if(!wp){
        vk2str(g_vk, buf);
        SetDlgItemText(hdlg, IDC_EDIT1, buf);
      }else{
        vk2str(g_vsk, buf);
        SetDlgItemText(hdlg, IDC_EDIT3, buf);}
      break;
    case WM_CLOSE:
      rmhk();
      rm_tray(hdlg, &tray);
      EndDialog(hdlg, 0);
      break;
    default:
      if(msg==WM_TASKBARCREATED)
        SendMessage(hdlg, WM_CREATE, wp, lp);
      break;
  }
  return 0;
}


// settings
BOOL read_cfg(){
  int len;
  char cfg[MAX_PATH] = "";
  len = GetCurrentDirectory(0, 0);
  GetCurrentDirectory(len, cfg);
  lstrcat(cfg, "\\aky.ini");

  g_vk = GetPrivateProfileInt("aky", "key", 82, cfg);
  g_vsk = GetPrivateProfileInt("aky", "skey", 123, cfg);
  g_itv = GetPrivateProfileInt("aky", "itv", 400, cfg);

  if(g_itv>9999 || g_itv<1) g_itv = 400;
  if(g_vk<8 || g_vk >254) g_vk = 87;
  if(g_vsk<8 || g_vsk >254) g_vsk = 123;
  if(g_vk==g_vsk) g_vsk = g_vk+1;

  return 1;
}

BOOL write_cfg(){
  int len;
  char cfg[MAX_PATH] = "";
  char buf[16];
  len = GetCurrentDirectory(0, 0);
  GetCurrentDirectory(len, cfg);
  lstrcat(cfg, "\\aky.ini");

  if(g_vk==g_vsk) g_vsk = g_vk+1;

  _itoa_s(g_vk, buf, 16, 10);
  WritePrivateProfileString("aky", "key", buf, cfg);
  _itoa_s(g_vsk, buf, 16, 10);
  WritePrivateProfileString("aky", "skey", buf, cfg);
  _itoa_s(g_itv, buf, 16, 10);
  WritePrivateProfileString("aky", "itv", buf, cfg);

  return 1;
}

// gui
LRESULT CALLBACK edit1_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
  if(msg==WM_KEYDOWN){
    g_vk = wp;
    SendMessage(g_hdlg, WM_EDIT, 0, 0);
    return 0;
  }
  return CallWindowProc(edit1p, hwnd, msg, wp, lp);
}

LRESULT CALLBACK edit3_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp){
  if(msg==WM_KEYDOWN){
    g_vsk = wp;
    SendMessage(g_hdlg, WM_EDIT, 1, 0);
    return 0;
  }
  return CallWindowProc(edit3p, hwnd, msg, wp, lp);
}


BOOL set_tray(HWND hwnd, PNOTIFYICONDATA tray){
  SecureZeroMemory(tray, sizeof(NOTIFYICONDATA));
  tray->cbSize = sizeof(tray);
  tray->hWnd = hwnd;
  tray->uID = 0;
  tray->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

  tray->uCallbackMessage = WM_TRAY;
  tray->hIcon = LoadIcon(g_hinst, (LPCTSTR)IDI_STAR);

  lstrcpy(tray->szTip, "AutoKey");
  lstrcpy(tray->szInfo, "AutoKey");
  lstrcpy(tray->szInfoTitle, ":)");
  tray->dwInfoFlags = NIIF_NONE;
  // show tray
  return Shell_NotifyIcon(NIM_ADD, tray);
}

BOOL rm_tray(HWND hwnd, PNOTIFYICONDATA tray){
  if(!tray) return 0;
  tray->uID = 0;
  tray->hWnd = hwnd;
  return Shell_NotifyIcon(NIM_DELETE, tray);
}


BOOL set_tip(HWND hwnd, PNOTIFYICONDATA ballon, char* caption, char* content){
  SecureZeroMemory(ballon, sizeof(NOTIFYICONDATA));
  ballon->cbSize = NOTIFYICONDATA_V2_SIZE;
  ballon->hWnd = hwnd;
  ballon->uFlags = NIF_INFO;
  ballon->uVersion = NOTIFYICON_VERSION;
  ballon->dwInfoFlags = NIIF_INFO;
  
  lstrcpy(ballon->szInfoTitle, caption);
  lstrcpy(ballon->szInfo, content);

  return Shell_NotifyIcon(NIM_MODIFY, ballon);
}


BOOL vk2str(char vk, char* str){
  memset(str, 0, 20);
  lstrcpy(str, vkcode[vk]);
  return 1;
}


// hook
LRESULT CALLBACK llk_proc(int code, WPARAM wp, LPARAM lp){
  POINT pt;
  HWND fcs_wnd;
  PKBDLLHOOKSTRUCT ks = (PKBDLLHOOKSTRUCT)lp;
  BOOL kup = WM_KEYUP==wp || WM_SYSKEYUP==wp;
  BOOL kdn = WM_KEYDOWN==wp || WM_SYSKEYDOWN==wp;
  GetCursorPos(&pt);

  fcs_wnd = WindowFromPoint(pt);
  if(!fcs_wnd || fcs_wnd!=FindWindow("KGWin32App", 0)){
  /*if(!fcs_wnd || fcs_wnd==g_hdlg || fcs_wnd==text1 ||
      fcs_wnd==text2 || fcs_wnd==text3 || 
      fcs_wnd==GetDlgItem(g_hdlg, IDC_BUTTON1) ||
      fcs_wnd==GetDlgItem(g_hdlg, IDC_BUTTON2)){*/
    if(timer0){
      KillTimer(0, timer0);
      timer0 = 0;
    }
    return CallNextHookEx(g_hook, code, wp, lp);
  }
  if(code!=HC_ACTION)
    return CallNextHookEx(g_hook, code, wp, lp);
  if(ks->vkCode==VK_LWIN){ // disable left winkey in game window
    //CallNextHookEx(g_hook, code, wp, lp);
    return 1;
  }
  if(kup && ks->vkCode==g_vsk){ // switch key pressed
    if(!timer0){
      keybd_event(g_vk, 0, 0, 0);
      keybd_event(g_vk, 0, KEYEVENTF_KEYUP, 0);
      timer0 = SetTimer(0, 0, g_itv, timer_proc);
      SendMessage(g_hdlg, WM_SWCH, 1, 0); // show tip
    }else{
      KillTimer(0, timer0);
      timer0 = 0;
      SendMessage(g_hdlg, WM_SWCH, 0, 0); // show tip
    }
  }else if(ks->scanCode==MapVirtualKey(g_vk, MAPVK_VK_TO_VSC)){ // holding key pressed
    if(!timer0 && kdn){
      keybd_event(g_vk, 0, 0, 0);
      keybd_event(g_vk, 0, KEYEVENTF_KEYUP, 0);
      timer0 = SetTimer(0, 0, g_itv, timer_proc);
      SendMessage(g_hdlg, WM_SWCH, 0, 0);
    }else if(timer0 && kup){
      KillTimer(0, timer0);
      timer0 = 0;
    }
    return 1;
  }
  return CallNextHookEx(g_hook, code, wp, lp);
}

LRESULT CALLBACK timer_proc(HWND hwnd, UINT msg, UINT id, DWORD time){
  keybd_event(g_vk, 0, 0, 0);
  keybd_event(g_vk, 0, KEYEVENTF_KEYUP, 0);
  return 1;
}

BOOL sethk(int itv, char vk, char vsk){
  if(vk==vsk){
    NOTIFYICONDATA ballon;
    set_tip(g_hdlg, &ballon, "Error", "Identical key&skey");
    return 0;
  }
  g_itv = itv;
  g_vk = vk;
  g_vsk = vsk;
  if(!g_hook)
    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)llk_proc, g_hinst, 0);
  return (g_hook!=0);
}

BOOL rmhk(){
  if(UnhookWindowsHookEx(g_hook)){
    KillTimer(0, timer0);
    timer0 = 0;
    g_hook = 0;
    return 1;
  }
  return 0;
}

