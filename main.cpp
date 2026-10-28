#define UNICODE
#define _UNICODE
#define _CRT_NON_CONFORMING_WCSTOK
#include <windows.h>
#include <shellapi.h>
#include <winreg.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <thread>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comdlg32.lib")
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_RELOAD 1002
#define ID_TRAY_APPLY 1003
#define ID_TRAY_OPENCFG 1004
#define ID_TRAY_SET_LIGHT_WALL 1005
#define ID_TRAY_SET_DARK_WALL 1006
#define ID_TRAY_LIGHT_CURSOR_SCHEME 1007
#define ID_TRAY_DARK_CURSOR_SCHEME 1008
#define ID_TRAY_AUTO_SWITCH 1009
#define ID_TRAY_RESTORE_CURSORS 1010
#define ID_INPUT_EDIT 2001
#define ID_INPUT_OK 2002
#define ID_INPUT_CANCEL 2003
using namespace std;
namespace fs = std::filesystem;
struct ModeConfig { wstring wallpaper; wstring cursorScheme; };
struct AppConfig { int check_interval_ms = 1000; bool auto_switch = true; ModeConfig light; ModeConfig dark; };
static HWND g_hWnd = NULL;
static NOTIFYICONDATAW g_nid = {0};
static UINT g_uID = 1;
static AppConfig g_cfg;
static wstring g_exeDir;
static wstring g_configPath;
static bool g_lastIsLight = true;
static bool g_running = true;
static HWND g_hInputDlg = NULL;
static HWND g_hInputEdit = NULL;
static wstring g_inputResult = L"";
static bool g_inputDone = false;
static wstring g_inputPrompt = L"";
static wstring g_inputTitle = L"";
static string WtoA(const wstring &w){ if(w.empty())return string(); int len=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),0,0,0,0); string s(len,0); WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&s[0],len,0,0); return s; }
static wstring AtoW(const string &a){ if(a.empty())return wstring(); int len=MultiByteToWideChar(CP_UTF8,0,a.c_str(),(int)a.size(),0,0); wstring w(len,0); MultiByteToWideChar(CP_UTF8,0,a.c_str(),(int)a.size(),&w[0],len); return w; }
static wstring ReadFileToWString(const wstring &path){ wifstream f(path,ios::binary); if(!f)return L""; wstringstream ss; ss<<f.rdbuf(); return ss.str(); }
static string ExtractString(const string &json,const string &key){ string pat="\""+key+"\""; size_t p=json.find(pat); if(p==string::npos)return""; p=json.find(':',p); if(p==string::npos)return""; p=json.find('"',p); if(p==string::npos)return""; size_t e=json.find('"',p+1); if(e==string::npos)return""; return json.substr(p+1,e-(p+1)); }
static bool ExtractBool(const string &json,const string &key,bool def){ string pat="\""+key+"\""; size_t p=json.find(pat); if(p==string::npos)return def; p=json.find(':',p); if(p==string::npos)return def; while(p<json.size()&&isspace((unsigned char)json[p]))p++; if(p+4<=json.size()&&json.compare(p,4,"true")==0)return true; if(p+5<=json.size()&&json.compare(p,5,"false")==0)return false; return def; }
static int ExtractInt(const string &json,const string &key,int def){ string pat="\""+key+"\""; size_t p=json.find(pat); if(p==string::npos)return def; p=json.find(':',p); if(p==string::npos)return def; while(p<json.size()&&(isspace((unsigned char)json[p])||json[p]==':'))p++; if(p>=json.size()||(!isdigit((unsigned char)json[p])&&json[p]!='-'))return def; size_t s=p; if(json[p]=='-')p++; while(p<json.size()&&isdigit((unsigned char)json[p]))p++; try{return stoi(json.substr(s,p-s));}catch(...){return def;} }
static void SaveConfig(){ wofstream f(g_configPath,ios::binary); if(!f)return; f<<L"{\n"; f<<L"  \"check_interval_ms\": "<<g_cfg.check_interval_ms<<L",\n"; f<<L"  \"auto_switch\": "<<(g_cfg.auto_switch?L"true":L"false")<<L",\n"; f<<L"  \"light\": {\n"; f<<L"    \"wallpaper\": \""<<AtoW(WtoA(g_cfg.light.wallpaper))<<L"\",\n"; f<<L"    \"cursorScheme\": \""<<AtoW(WtoA(g_cfg.light.cursorScheme))<<L"\"\n"; f<<L"  },\n"; f<<L"  \"dark\": {\n"; f<<L"    \"wallpaper\": \""<<AtoW(WtoA(g_cfg.dark.wallpaper))<<L"\",\n"; f<<L"    \"cursorScheme\": \""<<AtoW(WtoA(g_cfg.dark.cursorScheme))<<L"\"\n"; f<<L"  }\n"; f<<L"}\n"; }
static void LoadConfig(){ AppConfig cfg; cfg.check_interval_ms=1000; cfg.auto_switch=true; wstring raw=ReadFileToWString(g_configPath); string json=WtoA(raw); if(!json.empty()){ cfg.check_interval_ms=ExtractInt(json,"check_interval_ms",1000); cfg.auto_switch=ExtractBool(json,"auto_switch",true); auto lm=[&](const string &name,ModeConfig &m){ size_t p=json.find("\""+name+"\""); if(p==string::npos)return; size_t b=json.find('{',p); if(b==string::npos)return; size_t e=json.find('}',b); if(e==string::npos)return; string block=json.substr(b,e-b+1); m.wallpaper=AtoW(ExtractString(block,"wallpaper")); m.cursorScheme=AtoW(ExtractString(block,"cursorScheme")); }; lm("light",cfg.light); lm("dark",cfg.dark); }else SaveConfig(); g_cfg=cfg; }
static bool IsLightMode(){ HKEY hKey; DWORD val=1; DWORD size=sizeof(DWORD); LONG res=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",0,KEY_READ,&hKey); if(res!=ERROR_SUCCESS)return true; res=RegQueryValueExW(hKey,L"AppsUseLightTheme",NULL,NULL,(LPBYTE)&val,&size); RegCloseKey(hKey); if(res!=ERROR_SUCCESS)return true; return val==1; }
static void ApplyWallpaper(const wstring &path){ if(path.empty())return; wstring full=path; if(full.size()<2||!(full[1]==L':'||full[0]==L'\\')){ full=(fs::path(g_exeDir)/fs::path(path)).wstring(); } if(!fs::exists(full))return; SystemParametersInfoW(SPI_SETDESKWALLPAPER,0,(PVOID)full.c_str(),SPIF_UPDATEINIFILE|SPIF_SENDCHANGE); }
static void RestoreSystemCursors(){ SystemParametersInfoW(SPI_SETCURSORS,0,NULL,SPIF_UPDATEINIFILE|SPIF_SENDCHANGE); SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"UserEnvironment"); }
static wstring ExpandEnv(const wchar_t *s){ WCHAR buf[32768]; DWORD n=ExpandEnvironmentStringsW(s,buf,32768); if(n>0&&n<32768)return wstring(buf); return wstring(s); }
static void ApplyCursorScheme(const wstring &schemeName){ if(schemeName.empty())return;
    HKEY hSchemes = NULL;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Control Panel\\Cursors\\Schemes",0,KEY_READ,&hSchemes)==ERROR_SUCCESS){
        WCHAR buf[4096]={0}; DWORD sz=sizeof(buf);
        if(RegQueryValueExW(hSchemes,schemeName.c_str(),NULL,NULL,(LPBYTE)buf,&sz)==ERROR_SUCCESS){
            wchar_t *p = buf;
            wchar_t *next = wcstok(p,L",");
            int idx=0;
            const wchar_t* names[] = {L"Arrow",L"Help",L"AppStarting",L"Wait",L"Crosshair",L"IBeam",L"NWPen",L"No",L"SizeNS",L"SizeWE",L"SizeNWSE",L"SizeNESW",L"SizeAll",L"UpArrow",L"Hand",L"Pin",L"Person"};
            HKEY hCursors = NULL;
            if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Control Panel\\Cursors",0,NULL,REG_OPTION_NON_VOLATILE,KEY_WRITE,NULL,&hCursors,NULL)==ERROR_SUCCESS){
                while(next && idx < 17){
                    wstring expanded=ExpandEnv(next); RegSetValueExW(hCursors,names[idx],0,REG_SZ,(const BYTE*)expanded.c_str(),(DWORD)((expanded.size()+1)*sizeof(WCHAR)));
                    next = wcstok(NULL,L",");
                    idx++;
                }
                RegSetValueExW(hCursors,L"",0,REG_SZ,(const BYTE*)schemeName.c_str(),(DWORD)((schemeName.size()+1)*sizeof(WCHAR)));
                DWORD src=1;
                RegSetValueExW(hCursors,L"Scheme Source",0,REG_DWORD,(const BYTE*)&src,sizeof(DWORD));
                RegCloseKey(hCursors);
            }
        }
        RegCloseKey(hSchemes);
    }
    SystemParametersInfoW(SPI_SETCURSORS,0,NULL,SPIF_UPDATEINIFILE|SPIF_SENDCHANGE);
    SendNotifyMessage(HWND_BROADCAST, WM_SETTINGCHANGE, SPI_SETICONS, (LPARAM)L"windows"); }
static void ApplyForMode(bool isLight){ if(isLight){ApplyWallpaper(g_cfg.light.wallpaper);ApplyCursorScheme(g_cfg.light.cursorScheme);}else{ApplyWallpaper(g_cfg.dark.wallpaper);ApplyCursorScheme(g_cfg.dark.cursorScheme);} }
static wstring BrowseForFile(const wchar_t *filter,const wchar_t *title){ OPENFILENAMEW ofn={0}; wchar_t szFile[MAX_PATH]={0}; ofn.lStructSize=sizeof(OPENFILENAMEW); ofn.hwndOwner=g_hWnd; ofn.lpstrFilter=filter; ofn.lpstrFile=szFile; ofn.nMaxFile=MAX_PATH; ofn.lpstrTitle=title; ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR; if(GetOpenFileNameW(&ofn))return wstring(szFile); return L""; }
static LRESULT CALLBACK InputWndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){
    switch(msg){
    case WM_CREATE:{
        HFONT hf=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);
        HWND hSt = CreateWindowExW(0, L"STATIC", L"Prompt", WS_CHILD|WS_VISIBLE,10,10,340,40,hWnd,(HMENU)1001,hInst,NULL);
        if(hSt) SetWindowTextW(hSt, g_inputPrompt.c_str()); if(hSt) SendMessage(hSt,WM_SETFONT,(WPARAM)hf,TRUE);
        g_hInputEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,10,55,340,24,hWnd,(HMENU)ID_INPUT_EDIT,hInst,NULL);
        SendMessage(g_hInputEdit,WM_SETFONT,(WPARAM)hf,TRUE);
        HWND hOK = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,190,90,75,24,hWnd,(HMENU)ID_INPUT_OK,hInst,NULL);
        SendMessage(hOK,WM_SETFONT,(WPARAM)hf,TRUE);
        HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD|WS_VISIBLE|WS_TABSTOP,275,90,75,24,hWnd,(HMENU)ID_INPUT_CANCEL,hInst,NULL);
        SendMessage(hCancel,WM_SETFONT,(WPARAM)hf,TRUE);
        SetFocus(g_hInputEdit);
        break;
    }
    case WM_COMMAND:
        if(LOWORD(wParam)==ID_INPUT_OK){
            wchar_t buf[512]={0};
            GetWindowTextW(g_hInputEdit,buf,511);
            g_inputResult = buf;
            g_inputDone = true;
            DestroyWindow(hWnd);
            return 0;
        }
        if(LOWORD(wParam)==ID_INPUT_CANCEL){
            g_inputResult = L"";
            g_inputDone = true;
            DestroyWindow(hWnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        g_inputResult = L"";
        g_inputDone = true;
        DestroyWindow(hWnd);
        return 0;
    default:
        return DefWindowProcW(hWnd,msg,wParam,lParam);
    }
    return 0;
}
static wstring InputBox(const wchar_t *title,const wchar_t *prompt){
    g_inputResult = L"";
    g_inputDone = false;
    g_inputTitle = title;
    g_inputPrompt = prompt;
    HINSTANCE hInst = GetModuleHandleW(NULL);
    WNDCLASSEXW wc={0};
    wc.cbSize=sizeof(WNDCLASSEXW);
    wc.lpfnWndProc=InputWndProc;
    wc.hInstance=hInst;
    wc.lpszClassName=L"LidaInputDlg";
    RegisterClassExW(&wc);
    g_hInputDlg = CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_TOPMOST, L"LidaInputDlg", title, WS_POPUP|WS_CAPTION|WS_SYSMENU,
        (GetSystemMetrics(SM_CXSCREEN)-370)/2, (GetSystemMetrics(SM_CYSCREEN)-150)/2, 370, 150, g_hWnd, NULL, hInst, NULL);
    if(!g_hInputDlg) return L"";
    EnableWindow(g_hWnd,FALSE);
    ShowWindow(g_hInputDlg,SW_SHOW);
    UpdateWindow(g_hInputDlg);
    SetForegroundWindow(g_hInputDlg);
    MSG msg;
    while(GetMessageW(&msg,NULL,0,0)){
        if(msg.message==WM_QUIT) break;
        if(!IsDialogMessage(g_hInputDlg,&msg)){
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if(g_inputDone) break;
    }
    EnableWindow(g_hWnd,TRUE);
    SetForegroundWindow(g_hWnd);
    return g_inputResult;
}
static void AddTrayIcon(){ ZeroMemory(&g_nid,sizeof(g_nid)); g_nid.cbSize = sizeof(NOTIFYICONDATAW); g_nid.hWnd=g_hWnd; g_nid.uID=g_uID; g_nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP; g_nid.uCallbackMessage=WM_TRAYICON; HINSTANCE hInst = GetModuleHandleW(NULL); g_nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(101)); if(!g_nid.hIcon) g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); wcscpy_s(g_nid.szTip, L"Lida"); Shell_NotifyIconW(NIM_ADD,&g_nid); }
static void RemoveTrayIcon(){ Shell_NotifyIconW(NIM_DELETE,&g_nid); }
static void ShowTrayMenu(){ POINT pt; GetCursorPos(&pt); HMENU hMenu=CreatePopupMenu(); HMENU hLight=CreatePopupMenu(); HMENU hDark=CreatePopupMenu(); AppendMenuW(hLight,MF_STRING,ID_TRAY_SET_LIGHT_WALL,L"Set Wallpaper..."); AppendMenuW(hLight,MF_STRING,ID_TRAY_LIGHT_CURSOR_SCHEME,L"Set Cursor Scheme by Name..."); AppendMenuW(hDark,MF_STRING,ID_TRAY_SET_DARK_WALL,L"Set Wallpaper..."); AppendMenuW(hDark,MF_STRING,ID_TRAY_DARK_CURSOR_SCHEME,L"Set Cursor Scheme by Name..."); AppendMenuW(hMenu,MF_POPUP,(UINT_PTR)hLight,L"Light Mode"); AppendMenuW(hMenu,MF_POPUP,(UINT_PTR)hDark,L"Dark Mode"); AppendMenuW(hMenu,MF_SEPARATOR,0,NULL); AppendMenuW(hMenu,MF_STRING|(g_cfg.auto_switch?MF_CHECKED:MF_UNCHECKED),ID_TRAY_AUTO_SWITCH,L"Auto Switch on Theme Change"); AppendMenuW(hMenu,MF_STRING,ID_TRAY_APPLY,L"Apply Current Theme Now"); AppendMenuW(hMenu,MF_STRING,ID_TRAY_RESTORE_CURSORS,L"Restore Default Cursors"); AppendMenuW(hMenu,MF_SEPARATOR,0,NULL); AppendMenuW(hMenu,MF_STRING,ID_TRAY_RELOAD,L"Reload Config"); AppendMenuW(hMenu,MF_STRING,ID_TRAY_OPENCFG,L"Open Config Folder"); AppendMenuW(hMenu,MF_SEPARATOR,0,NULL); AppendMenuW(hMenu,MF_STRING,ID_TRAY_EXIT,L"Exit"); SetForegroundWindow(g_hWnd); TrackPopupMenu(hMenu,TPM_RIGHTBUTTON,pt.x,pt.y,0,g_hWnd,NULL); DestroyMenu(hMenu); }
static void OpenConfigFolder(){ ShellExecuteW(NULL,L"open",g_exeDir.c_str(),NULL,NULL,SW_SHOWDEFAULT); }
LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){ switch(msg){ case WM_TRAYICON: if(lParam==WM_RBUTTONUP||lParam==WM_LBUTTONUP)ShowTrayMenu(); break; case WM_COMMAND: switch(LOWORD(wParam)){ case ID_TRAY_EXIT:g_running=false;PostQuitMessage(0);break; case ID_TRAY_RELOAD:LoadConfig();MessageBoxW(g_hWnd,L"Config reloaded",L"Lida",MB_OK|MB_ICONINFORMATION);break; case ID_TRAY_APPLY:ApplyForMode(IsLightMode());break; case ID_TRAY_OPENCFG:OpenConfigFolder();break; case ID_TRAY_AUTO_SWITCH:g_cfg.auto_switch=!g_cfg.auto_switch;SaveConfig();break; case ID_TRAY_RESTORE_CURSORS:RestoreSystemCursors();MessageBoxW(g_hWnd,L"Restored default system cursors",L"Lida",MB_OK|MB_ICONINFORMATION);break; case ID_TRAY_SET_LIGHT_WALL:{wstring f=BrowseForFile(L"Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.gif\0All Files\0*.*\0",L"Select Light Mode Wallpaper"); if(!f.empty()){fs::path base(g_exeDir);error_code ec;fs::path rel=fs::relative(f,base,ec);if(!ec&&rel.wstring().find(L"..")==wstring::npos)g_cfg.light.wallpaper=rel.wstring();else g_cfg.light.wallpaper=f;SaveConfig();if(IsLightMode())ApplyWallpaper(g_cfg.light.wallpaper);}break;} case ID_TRAY_SET_DARK_WALL:{wstring f=BrowseForFile(L"Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.gif\0All Files\0*.*\0",L"Select Dark Mode Wallpaper"); if(!f.empty()){fs::path base(g_exeDir);error_code ec;fs::path rel=fs::relative(f,base,ec);if(!ec&&rel.wstring().find(L"..")==wstring::npos)g_cfg.dark.wallpaper=rel.wstring();else g_cfg.dark.wallpaper=f;SaveConfig();if(!IsLightMode())ApplyWallpaper(g_cfg.dark.wallpaper);}break;} case ID_TRAY_LIGHT_CURSOR_SCHEME:{wstring name=InputBox(L"Light Mode Cursor Scheme",L"Enter exact cursor scheme name (as in Mouse Properties > Pointers):"); if(!name.empty()){g_cfg.light.cursorScheme=name;SaveConfig();if(IsLightMode())ApplyCursorScheme(g_cfg.light.cursorScheme);}break;} case ID_TRAY_DARK_CURSOR_SCHEME:{wstring name=InputBox(L"Dark Mode Cursor Scheme",L"Enter exact cursor scheme name (as in Mouse Properties > Pointers):"); if(!name.empty()){g_cfg.dark.cursorScheme=name;SaveConfig();if(!IsLightMode())ApplyCursorScheme(g_cfg.dark.cursorScheme);}break;} }break; case WM_DESTROY:RemoveTrayIcon();PostQuitMessage(0);break; default:return DefWindowProcW(hWnd,msg,wParam,lParam);} return 0; }
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow){ WCHAR path[MAX_PATH]; GetModuleFileNameW(NULL,path,MAX_PATH); g_exeDir=fs::path(path).parent_path().wstring(); g_configPath=(fs::path(g_exeDir)/L"config.json").wstring(); LoadConfig(); WNDCLASSEXW wc={0}; wc.cbSize=sizeof(WNDCLASSEXW); wc.lpfnWndProc=WndProc; wc.hInstance=hInstance; wc.lpszClassName=L"LidaClass"; wc.hIcon=(HICON)LoadImageW(hInstance,MAKEINTRESOURCEW(101),IMAGE_ICON,0,0,LR_DEFAULTSIZE|LR_SHARED); wc.hIconSm=wc.hIcon; RegisterClassExW(&wc); g_hWnd=CreateWindowExW(0,L"LidaClass",L"Lida",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,0,0,NULL,NULL,hInstance,NULL); if(wc.hIcon){ SendMessageW(g_hWnd,WM_SETICON,ICON_BIG,(LPARAM)wc.hIcon); SendMessageW(g_hWnd,WM_SETICON,ICON_SMALL,(LPARAM)wc.hIconSm); } AddTrayIcon(); g_lastIsLight=IsLightMode(); ApplyForMode(g_lastIsLight); MSG msg; while(g_running){ while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){ if(msg.message==WM_QUIT){g_running=false;break;} TranslateMessage(&msg); DispatchMessageW(&msg); } if(!g_running)break; if(g_cfg.auto_switch){ bool cur=IsLightMode(); if(cur!=g_lastIsLight){g_lastIsLight=cur;ApplyForMode(cur);} } std::this_thread::sleep_for(std::chrono::milliseconds(g_cfg.check_interval_ms)); } RemoveTrayIcon(); return 0; }
