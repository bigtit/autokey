// hk.c
// 3.0 version
// use low level keybd hook
// to avoid keybd_event confusion with keybd hook
#include <windows.h>

#define WM_ONOFF WM_USER+2

// global and shared data
#pragma data_seg("hookdata")
int itv = 0;          // interval
char ky = 0;          // key
char sk = 0;
int md = 0;           // mode
HWND dlg = 0;         // save dlg handler
#pragma data_seg()

// no shared
HHOOK hook = 0;       // main hook
HINSTANCE inst = 0;   // dll instance
HWND wnd = 0;         // target wnd
BOOL ton = 0;         // is timer on
UINT_PTR tid = 0;     // timer id

LRESULT CALLBACK llkproc(int, WPARAM, LPARAM);
LRESULT CALLBACK tproc(HWND, UINT, UINT, DWORD);

BOOL APIENTRY DllMain(HINSTANCE hDll, DWORD dwReason, LPVOID lpReserved)
{
  switch(dwReason){
    case DLL_PROCESS_ATTACH:
      inst = hDll;
      DisableThreadLibraryCalls(inst);
      break;
    case DLL_PROCESS_DETACH:
      break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
  }
  return TRUE;
}
BOOL WINAPI __stdcall SetDlg(HWND hdlg)
{
  dlg = hdlg;
  return 0;
}
BOOL WINAPI __stdcall SetSkey(char skey)
{
  sk = skey;
  return 0;
}

BOOL WINAPI __stdcall SetHook(int mode, int iterval, char key)
{
  //int lerr;
  //char cinst[10];
  BOOL ok;
  itv = iterval;
  ky = key;
  md = mode;

  // timer should be off
  ton = 0;

  if(md){
    if(wnd) return 0;
    hook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)llkproc, inst, 0);
    ok=(hook!=0);
  }
  else{
    ok = UnhookWindowsHookEx(hook);
    if(ok){
      KillTimer(0, tid);
      hook = 0;
      wnd = 0;
    }
  }
  return ok;
}
LRESULT CALLBACK llkproc(int code, WPARAM wp, LPARAM lp)
{
  POINT pt;
  PKBDLLHOOKSTRUCT ks = (PKBDLLHOOKSTRUCT)lp;
  BOOL kup = WM_KEYUP==wp || WM_SYSKEYUP==wp;
  BOOL kdn = WM_KEYDOWN==wp || WM_SYSKEYDOWN==wp;
  GetCursorPos(&pt);
  // wnd = GetActiveWindow();
  wnd = WindowFromPoint(pt);

  if(code!=HC_ACTION)
    return CallNextHookEx(hook, code, wp, lp);
  
  // no problem with on/off mode(mode 1)
  if(md==1 && kup && ks->vkCode==sk){
    if(!ton){
      keybd_event(ky, 0, 0, 0);
      keybd_event(ky, 0, KEYEVENTF_KEYUP, 0);
      tid = SetTimer(0, 0, itv, tproc);
      SendMessage(dlg, WM_ONOFF, 1, 0);
      ton = 1;
    }
    else{
      KillTimer(0, tid);
      SendMessage(dlg, WM_ONOFF, 0, 0);
      ton = 0;
    }
    return 1;
  }
  else if(md==2 && ks->scanCode==MapVirtualKey(ky, MAPVK_VK_TO_VSC)){
    if(!ton && kdn){
      keybd_event(ky, 0, 0, 0);
      keybd_event(ky, 0, KEYEVENTF_KEYUP, 0);
      tid = SetTimer(0, 0, itv, tproc);
      ton = 1;
    }
    if(ton && kup){
      KillTimer(0, tid);
      ton = 0;
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
  // PostMessage is only availible in text area of jx3;
  // PostMessage(wnd, WM_KEYDOWN, ky, llp);
  // PostMessage(wnd, WM_KEYUP, ky, llp);
  /*
  INPUT inp[2];
  memset(inp, 0, sizeof(inp));
  inp[0].type = INPUT_KEYBOARD;
  inp[0].ki.wVk = ky;
  inp[0].ki.wScan = MapVirtualKey(ky, MAPVK_VK_TO_VSC);
  inp[1].type = inp[0].type;
  inp[1].ki.wVk = ky;
  inp[1].ki.wScan = inp[0].ki.wScan;
  inp[1].ki.dwFlags = KEYEVENTF_KEYUP;
  SendInput(2, inp, sizeof(INPUT));
  */
  return 1;
}
