// 窓の仕切り屋.cpp : アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include "窓の仕切り屋.h"
#include <shellapi.h>

#include <map>
#include <string>
#include <list>

#define MAX_LOADSTRING 100
#define MYMSG_TRAY WM_USER
#define ID_MYTRAY 101
#define DATA_FILE "save.dat"
#define SETTINGS_FILE "settings.ini"
#define VERSION "窓の仕切り屋 v1.0"

typedef std::basic_string<TCHAR> string;
typedef std::basic_string<TCHAR> tstring;


struct tagSettings {
	int maxWindow;
};

struct tagWindowsInfo {
	class tagWindow{
	public:
		bool first;
		BOOL min, max;
		int x, y, width, height;
		tagWindow()
		{
			first = true;
		}
		void save(HWND hWnd)
		{
			RECT rcWnd;
			GetWindowRect(hWnd, &rcWnd);
			this->x = rcWnd.left;
			this->y = rcWnd.top;
			this->width = rcWnd.right - rcWnd.left;
			this->height = rcWnd.bottom - rcWnd.top;
//			this->max = IsZoomed(hWnd);
//			this->min = IsIconic(hWnd);
			this->first = false;
		}
		void restore(HWND hWnd)
		{
			if (isNew())
			{
				save(hWnd);
				return;
			}
			max = IsZoomed(hWnd);
			min = IsIconic(hWnd);
			MoveWindow(
				hWnd,
				x,
				y,
				width,
				height,
				TRUE);
			if (max) ShowWindow(hWnd, SW_MAXIMIZE);
			if (min) ShowWindow(hWnd, SW_MINIMIZE);
		}
		bool isNew()
		{
			return first;
		}
	};
	std::map<string, tagWindow> windowList;
};

class cResolution
{
public:
	int width, height;

public:
	cResolution()
	{
		width = 0;
		height = 0;
	}
  bool operator <(const cResolution &t) const
  {
		if (width < t.width) return true;
		if (width == t.width) return height < t.height;
		return false;
  }
};

// グローバル変数:
HINSTANCE hInst;								// 現在のインターフェイス
TCHAR szTitle[MAX_LOADSTRING];					// タイトル バーのテキスト
TCHAR szWindowClass[MAX_LOADSTRING];			// メイン ウィンドウ クラス名
std::map<cResolution, tagWindowsInfo> windowsInfoList;
tagWindowsInfo *windowsInfo;
bool watchLock = false;
tagSettings settings;

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM							MyRegisterClass(HINSTANCE hInstance);
BOOL							InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
LRESULT						OnDisplayChange(void);
VOID CALLBACK			WinEventCallback(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
BOOL CALLBACK			ResotreAllWindows(HWND, LPARAM);
int								MakeRTrayMenu(HWND);

void taskTray(HWND hWnd, PNOTIFYICONDATA lpni)
{
    HICON hIcon;

    hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_MY));
    lpni->cbSize = sizeof(NOTIFYICONDATA);
    lpni->hIcon = hIcon;
    lpni->hWnd = hWnd;
    lpni->uCallbackMessage = MYMSG_TRAY;
    lpni->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    lpni->uID = ID_MYTRAY;
    strcpy(lpni->szTip, "窓の仕切り屋");
    
    Shell_NotifyIcon(NIM_ADD, lpni);
    return;
}

int MakeRTrayMenu(HWND hWnd)
{
    HMENU hMenu, hSubMenu;
    POINT pt;

    hMenu = LoadMenu(hInst, "RTRAYMENU");
    hSubMenu = GetSubMenu(hMenu, 0);
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    TrackPopupMenu(
        hSubMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
    return 0;
}

void LoadSettings(void)
{
	char str[128], dir[MAX_PATH], path[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, dir);
	wsprintf(path, "%s\\%s", dir, SETTINGS_FILE);

  GetPrivateProfileString("Global", "MaxWindow", NULL, str, 100, path) ? settings.maxWindow = atoi(str) : settings.maxWindow = 100;
}

void SaveSettings(void)
{
	char str[128], dir[MAX_PATH], path[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, dir);
	wsprintf(path, "%s\\%s", dir, SETTINGS_FILE);

	itoa(settings.maxWindow, str, 10);
	WritePrivateProfileString("Global", "MaxWindow", str, path);
}

void FileLoad(void)
{
	FILE *fp;

	if((fp = fopen(DATA_FILE, "rb")) == NULL)
	{
		return;
	}

	// バージョンチェック
	char ver[128];
	fread(ver, sizeof(char), 128, fp);
	if (strcmp(ver, VERSION))
	{
		return;
	}

	// 各解像度のデータ
	size_t size;
	fread(&size, sizeof(size_t), 1, fp);
	for (size_t i = 0; i < size; ++i)
	{
		cResolution resolution;
		fread(&resolution.width, sizeof(int), 1, fp);
		fread(&resolution.height, sizeof(int), 1, fp);
		tagWindowsInfo &windowsInfo = windowsInfoList[resolution];
		{
			size_t size;
			fread(&size, sizeof(size_t), 1, fp);

			// 各ウィンドウのデータ
			for (size_t j = 0; j < size; ++j)
			{
				char name[256];
				int nameLength;
				fread(&nameLength, sizeof(int), 1, fp);
				fread(name, sizeof(char), nameLength, fp);

				tagWindowsInfo::tagWindow &window = windowsInfo.windowList[name];
				fread(&window.x, sizeof(int), 1, fp);
				fread(&window.y, sizeof(int), 1, fp);
				fread(&window.width, sizeof(int), 1, fp);
				fread(&window.height, sizeof(int), 1, fp);
				window.first = false;
			}
		}
	}

	fclose(fp);
}

void FileSave(void)
{
	FILE *fp;

	if((fp = fopen(DATA_FILE, "wb")) == NULL ) {
		printf("ファイルオープンエラー\n");
		exit(EXIT_FAILURE);
	}

	// バージョン
	char ver[128] = VERSION;
	fwrite(ver, sizeof(char), 128, fp);

	// 各解像度のデータ
	size_t size = windowsInfoList.size();
	fwrite(&size, sizeof(size_t), 1, fp);
	std::map<cResolution, tagWindowsInfo>::iterator it = windowsInfoList.begin();
	for (; it != windowsInfoList.end(); ++it)
	{
		fwrite(&it->first.width, sizeof(int), 1, fp);
		fwrite(&it->first.height, sizeof(int), 1, fp);
		{
			std::map<string, tagWindowsInfo::tagWindow> &windowList = it->second.windowList;
			size_t size = windowList.size();
			fwrite(&size, sizeof(size_t), 1, fp);

			// 各ウィンドウのデータ
			std::map<string, tagWindowsInfo::tagWindow>::iterator it = windowList.begin();
			for (; it != windowList.end(); ++it)
			{
				const char *name = it->first.c_str();
				const int nameLength = strlen(name) + 1;
				fwrite(&nameLength, sizeof(int), 1, fp);
				fwrite(name, sizeof(char), nameLength, fp);

				tagWindowsInfo::tagWindow &window = it->second;
				fwrite(&window.x, sizeof(int), 1, fp);
				fwrite(&window.y, sizeof(int), 1, fp);
				fwrite(&window.width, sizeof(int), 1, fp);
				fwrite(&window.height, sizeof(int), 1, fp);
			}
		}
	}

	fclose(fp);
}

// グローバルフック
VOID CALLBACK WinEventMoveSize( 
	HWINEVENTHOOK hWinEventHook,
	DWORD event,
	HWND hWnd,
	LONG idObject,
  LONG idChild,
  DWORD dwEventThread,
  DWORD dwmsEventTime)
{
	if (watchLock) return;
	TCHAR szClassName[256];
	GetClassName(hWnd, szClassName, sizeof(szClassName));
	
	tagWindowsInfo::tagWindow *window = &windowsInfo->windowList[(string)szClassName];

	window->save(hWnd);
}

VOID CALLBACK WinEventMiniMize( 
	HWINEVENTHOOK hWinEventHook,
	DWORD event,
	HWND hWnd,
	LONG idObject,
  LONG idChild,
  DWORD dwEventThread,
  DWORD dwmsEventTime)
{
	if (watchLock) return;
	TCHAR szClassName[256];
	GetClassName(hWnd, szClassName, sizeof(szClassName));
	
	tagWindowsInfo::tagWindow *window = &windowsInfo->windowList[(string)szClassName];

	window->save(hWnd);
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LoadSettings();
	FileLoad();

	OnDisplayChange();

	SetWinEventHook(
		EVENT_SYSTEM_MOVESIZEEND,
    EVENT_SYSTEM_MOVESIZEEND,
    NULL,
    WinEventMoveSize,
    0,
    0,
    WINEVENT_SKIPOWNTHREAD | WINEVENT_SKIPOWNPROCESS);

	SetWinEventHook(
		EVENT_SYSTEM_MINIMIZESTART,
    EVENT_SYSTEM_MINIMIZEEND,
    NULL,
    WinEventMiniMize,
    0,
    0,
    WINEVENT_SKIPOWNTHREAD | WINEVENT_SKIPOWNPROCESS);

 	// TODO: ここにコードを挿入してください。
	MSG msg;
	HACCEL hAccelTable;

	// グローバル文字列を初期化しています
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_MY, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// アプリケーションの初期化を実行します:
	if (!InitInstance (hInstance, SW_HIDE))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MY));

	// メイン メッセージ ループ:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	FileSave();
	SaveSettings();

	return (int) msg.wParam;
}

//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
//  コメント:
//
//    この関数および使い方は、'RegisterClassEx' 関数が追加された
//    Windows 95 より前の Win32 システムと互換させる場合にのみ必要です。
//    アプリケーションが、関連付けられた
//    正しい形式の小さいアイコンを取得できるようにするには、
//    この関数を呼び出してください。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MY));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_MY);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します。
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // グローバル変数にインスタンス処理を格納します。

	// 二重起動防止
	_TCHAR path[1024], filename[128], ext[64];
  GetModuleFileName(0, path, 1024);
	_tsplitpath(path, NULL, NULL, filename, ext);
	tstring exe = (tstring)filename + ext;
	CreateMutex(NULL, TRUE, exe.c_str());
	if( GetLastError() == ERROR_ALREADY_EXISTS)
	{
		HWND hWnd = FindWindow(NULL, szTitle);
		if (hWnd) SetForegroundWindow(hWnd);

		return FALSE;
	}

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的:  メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND	- アプリケーション メニューの処理
//  WM_PAINT	- メイン ウィンドウの描画
//  WM_DESTROY	- 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	static NOTIFYICONDATA ni;

	switch (message)
	{
	case WM_CREATE:
		taskTray(hWnd, &ni);
		break;
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// 選択されたメニューの解析:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: 描画コードをここに追加してください...
		EndPaint(hWnd, &ps);
		break;
	case WM_DISPLAYCHANGE:
		OnDisplayChange();
		break;
  case MYMSG_TRAY:
    if (wParam == ID_MYTRAY)
		{
        switch (lParam)
				{
        case WM_RBUTTONDOWN:
          MakeRTrayMenu(hWnd);
          break;
        default:
            break;
        }
    }
    break;
	case WM_QUERYENDSESSION:
	case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &ni);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
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

LRESULT OnDisplayChange(void)
{
	static bool changed = false;
	static cResolution resolution;

	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	if (resolution.width == width && resolution.height == height) return 0;

	watchLock = true;

	resolution.width = width;
	resolution.height = height;

	Sleep(700);

	windowsInfo = &windowsInfoList[resolution];
	EnumWindows(ResotreAllWindows, 0);

	watchLock = false;

	return 0;
}

// すべてのウィンドウハンドルを取得
BOOL CALLBACK ResotreAllWindows(HWND hWnd, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	LONG style = GetWindowLong(hWnd, GWL_STYLE);
	if (!(style & WS_VISIBLE) /*|| !(style & WS_CAPTION)*/) return TRUE;

	TCHAR szClassName[256];
	GetClassName(hWnd, szClassName, sizeof(szClassName));

	tagWindowsInfo::tagWindow *window = &windowsInfo->windowList[(string)szClassName];

	window->restore(hWnd);

	return TRUE;
}