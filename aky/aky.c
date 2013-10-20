#include <windows.h>
#include <commctrl.h>

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
char mode = 0;

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
int GetMode(HWND, int, int);
BOOL SetTray(HWND, PNOTIFYICONDATA);
BOOL RemoveTray(HWND, PNOTIFYICONDATA);
BOOL SetTip(char*, char*, HWND, PNOTIFYICONDATA);

// load dll dynamically
HMODULE module;
BOOL (__stdcall *SetHook)(int, int, char);
BOOL (__stdcall *SetDlg)(HWND);
BOOL (__stdcall *SetSkey)(char);

int APIENTRY WinMain(HINSTANCE hinst, HINSTANCE phinst, LPSTR lpcmdline, int icmdshow)
{
  // save global instance of the app
  inst = hinst;
  // load cfg file
  GetCfg();

  // load dll and func
  module = LoadLibrary(TEXT("hk.dll"));
  if(!module){
    MessageBox(0, "�޷�����hk.dll", "����", 0);
    return -1;
  }
  SetHook = (BOOL (__stdcall *)(int, int, char))GetProcAddress(module, "SetHook");
  SetDlg = (BOOL (__stdcall *)(HWND))GetProcAddress(module, "SetDlg");
  SetSkey = (BOOL (__stdcall *)(char))GetProcAddress(module, "SetSkey");
  if(!SetHook || !SetDlg || !SetSkey){
    MessageBox(0, "���뺯������", "����", 0);
    return -1;
  }

  // create dialog to send WM_INITDIALOG msg
  DialogBoxParam(hinst, MAKEINTRESOURCE(IDD_MAIN), 0, (DLGPROC)DlgProc, 0);

  FreeLibrary(module);
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
  lstrcpy(tip, key);
  lstrcat(tip, "\n");
  lstrcat(tip, itv);
  lstrcat(tip, " ms");

  switch(msg){
    case WM_INITDIALOG:
      // send hdlg to dll
      SetDlg(hdlg);
      dlg = hdlg;
      // load tray and menu
      SetTray(hdlg, &tray);
      menu = LoadMenu(inst, MAKEINTRESOURCE(IDC_TRAY));
      menu = GetSubMenu(menu, 0);

      // components settings
      SetDlgItemText(hdlg, IDC_EDIT1, key);
      SetDlgItemText(hdlg, IDC_EDIT2, itv);
      SetDlgItemText(hdlg, IDC_EDIT3, skey);
      SendDlgItemMessage(hdlg, IDC_EDIT1, EM_LIMITTEXT, 1, 0);
      SendDlgItemMessage(hdlg, IDC_EDIT2, EM_LIMITTEXT, 4, 0);
      SendDlgItemMessage(hdlg, IDC_EDIT3, EM_LIMITTEXT, 1, 0);

      edit1proc = (WNDPROC)SetWindowLong(GetDlgItem(hdlg, IDC_EDIT1), 
        GWL_WNDPROC, (LONG)Edit1Proc);
      edit3proc = (WNDPROC)SetWindowLong(GetDlgItem(hdlg, IDC_EDIT3), 
        GWL_WNDPROC, (LONG)Edit3Proc);
      CheckRadioButton(hdlg, IDC_RADIO1, IDC_RADIO3, mode+IDC_RADIO1);

      // start hook with mode
      if(SetHook(mode, atoi(itv), vk)){
        if(mode==1){
          lstrcat(tip, "\n");
          lstrcat(tip, skey);
          SetSkey(vsk);
          SetTip(tip, "����: �ر�", hdlg, &ballon);
        }
        else if(mode==2) SetTip(tip, "��ס", hdlg, &ballon);
        else SetTip(tip, "�ر�", hdlg, &ballon);
      }
      else SetTip(tip, "�ر�", hdlg, &ballon);

      if(mode!=1) EnableWindow(GetDlgItem(hdlg, IDC_EDIT3), 0);
      // topmost
      SetActiveWindow(hdlg);
      // SetWindowPos(hdlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
      
      return 1;
    case WM_TRAY:
      if(lp==WM_RBUTTONDOWN){
        GetCursorPos(&pt);
        SetForegroundWindow(hdlg);
        TrackPopupMenuEx(menu, TPM_CENTERALIGN, pt.x, pt.y, hdlg, 0);
      }
      else if(lp==WM_LBUTTONDOWN){
      switch(mode){
        case 0: SetTip(tip, "�ر�", hdlg, &ballon); break;
        case 1:
          lstrcat(tip, "\n");
          lstrcat(tip, skey);
          SetTip(tip, "����", hdlg, &ballon);
          break;
        case 2: SetTip(tip, "��ס", hdlg, &ballon); break;
        default: break;
      }
      }
      break;
    case WM_COMMAND:
      wid = LOWORD(wp);
      if(wid==IDC_BUTTON1){
        // getting phase with checking
        GetDlgItemText(hdlg, IDC_EDIT2, itv, 5);
        mode = GetMode(hdlg, IDC_RADIO1, 3);
        ChkPrm();

        // setting phase
        // need reboot hook
        lstrcpy(tip, key);
        lstrcat(tip, "\n");
        lstrcat(tip, itv);
        lstrcat(tip, " ms");
        SetDlgItemText(hdlg, IDC_EDIT2, itv);

        if(mode) SetHook(0, atoi(itv), vk);
        if(SetHook(mode, atoi(itv), vk)){
          if(mode==1){
            lstrcat(tip, "\n");
            lstrcat(tip, skey);
            SetSkey(vsk);
            SetTip(tip, "����", hdlg, &ballon);
            EnableWindow(GetDlgItem(hdlg, IDC_EDIT3), 1);
          }
          else{
            if(!mode) SetTip(tip, "�ر�", hdlg, &ballon);
            else SetTip(tip, "��ס", hdlg, &ballon);
            EnableWindow(GetDlgItem(hdlg, IDC_EDIT3), 0);
          }
        }
        else SetTip(tip, "�ر�", hdlg, &ballon);
      }
      else if(wid==IDM_TRAY_EXT) SendMessage(hdlg, WM_CLOSE, wp, lp);
      else if(wid==IDM_TRAY_ABO) SetTip("2013-8-1\nTsubasa9", "����", hdlg, &ballon);
      else if(wid==IDM_TRAY_SET || wid==IDC_BUTTON2){
        if(!IsWindowVisible(hdlg)){
          ShowWindow(hdlg, SW_SHOW);
          CheckMenuItem(menu, IDM_TRAY_SET, MF_CHECKED);
        }
        else{
          ShowWindow(hdlg, SW_HIDE);
          CheckMenuItem(menu, IDM_TRAY_SET, MF_UNCHECKED);
          switch(mode){
            case 0: SetTip(tip, "�ر�", hdlg, &ballon); break;
            case 1:
            lstrcat(tip, "\n");
            lstrcat(tip, skey);
            SetTip(tip, "����", hdlg, &ballon);
            break;
            case 2: SetTip(tip, "��ס", hdlg, &ballon); break;
            default: break;
          }
        }
      }
      break;
    case WM_ONOFF:
      lstrcat(tip, "\n");
      lstrcat(tip, skey);
      if(!wp) SetTip(tip, "����:�ر�", hdlg, &ballon);
      else SetTip(tip, "����:����", hdlg, &ballon);
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
  mode = GetPrivateProfileInt("aky", "mode", 0, cfg);

  if(!ChkPrm()) return 0;

  vk2s(vk, key);
  vk2s(vsk, skey);

  return 1;
}

BOOL SetCfg()
{
  int len;
  char cmode[2];
  len = GetCurrentDirectory(0, NULL);
  GetCurrentDirectory(len, cfg);
  lstrcat(cfg, "\\aky.ini");

  if(!ChkPrm()) return 0;

  _itoa(mode, cmode, 10);
  _itoa(vk, key, 10);
  _itoa(vsk, skey, 10);
  WritePrivateProfileString("aky", "key", key, cfg);
  WritePrivateProfileString("aky", "skey", skey, cfg);
  WritePrivateProfileString("aky", "interval", itv, cfg);
  WritePrivateProfileString("aky", "mode", cmode, cfg);
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
  if(mode<0 || mode>3) mode = 0;

  return 1;
}
// get mode from radiobutton group
// buttons in one group have to ensure continuous ids
int GetMode(HWND hwnd, int startid, int n)
{
  int id;
  for(id=startid; id<startid+n; ++id){
    if(IsDlgButtonChecked(hwnd, id))
      return (id-startid);
  }
  return -1;
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
  else return CallWindowProc(edit1proc, hwnd, msg, wp, lp);
}
//LRESULT CALLBACK Edit2Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
//{
//  if(msg==WM_CHAR && (wp<'0' || wp>'9') && wp!=VK_BACK)
//    return 0;
//  else return CallWindowProc(edit2proc, hwnd, msg, wp, lp);
//}

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
