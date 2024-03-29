//Mp4Filelayer.cpp: 定义应用程序的入口点。
//

#include "stdafx.h"
#include "Mp4Filelayer.h"
#include "dec_thread.h"
#include "vren_thread.h"
#include "aren_thread.h"
#include "media_track.h"
#include "r_string.h"

#define MAX_LOADSTRING 100
#define SHIFTED 0x8000 

// 全局变量: 
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
HWND g_hWnd = NULL;
dec_thread* g_decThread = NULL;
vren_thread* g_vrenThread = NULL;
aren_thread* g_arenThread = NULL;

// 此代码模块中包含的函数的前向声明: 
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此放置代码。

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MP4FILELAYER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化: 
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

	g_decThread = new dec_thread();
	g_arenThread = new aren_thread();
	g_vrenThread = new vren_thread();
	media_track *pMediaTrack = new media_track();
	//r_string url("C:\\Users\\cuidx\\Documents\\Visual Studio 2010\\Projects\\testgl\\glplayer\\1.mp4");
	r_string url("C:\\Users\\cuidx\\Desktop\\hainanyidong\\nginx-1.10.2\\html\\111.mp4");
	g_decThread->SetParam(g_vrenThread, g_arenThread, pMediaTrack, url);
	g_vrenThread->SetParam(g_hWnd);
	g_decThread->Start();
	g_arenThread->Start();
	g_vrenThread->Start();

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MP4FILELAYER));

    MSG msg;

    // 主消息循环: 
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目的: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MP4FILELAYER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_MP4FILELAYER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目的: 保存实例句柄并创建主窗口
//
//   注释: 
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // 将实例句柄存储在全局变量中

   g_hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!g_hWnd)
   {
      return FALSE;
   }

   ShowWindow(g_hWnd, nCmdShow);
   UpdateWindow(g_hWnd);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的:    处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 分析菜单选择: 
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
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
            EndPaint(hWnd, &ps);
        }
        break;
	case WM_SIZE:
		{
			if (g_vrenThread)
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				g_vrenThread->TryResizeViewport(rect.right, rect.bottom);
			}
		}
		break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
	case WM_CHAR:
		{
			if (g_vrenThread)
			{
				int vKey = wParam;
				if (vKey >= 'x' && vKey <= 'z' || vKey >= 'X' && vKey <= 'Z') // X
				{
					g_vrenThread->KeyboardMsg(vKey);
				}
				if (vKey == 'v')
				{
					g_vrenThread->SetViewMode(0);
				}
				else if (vKey == 'V')
				{
					g_vrenThread->SetViewMode(1);
				}
			}
		}
		break;
	case WM_KEYDOWN:
		{
			if (g_vrenThread)
			{
				int vKey = wParam;
				if (vKey == VK_LEFT)
				{
					g_vrenThread->KeyboardMsg('Y');
				}
				else if (vKey == VK_RIGHT)
				{
					g_vrenThread->KeyboardMsg('y');
				}
				else if (vKey == VK_UP)
				{
					short nVirtKey = GetKeyState(VK_SHIFT);
					if (nVirtKey & SHIFTED)
					{
						g_vrenThread->KeyboardMsg('Z');
					}
					else
					{
						g_vrenThread->KeyboardMsg('X');
					}

				}
				else if (vKey == VK_DOWN)
				{
					short nVirtKey = GetKeyState(VK_SHIFT);
					if (nVirtKey & SHIFTED)
					{
						g_vrenThread->KeyboardMsg('z');
					}
					else
					{
						g_vrenThread->KeyboardMsg('x');
					}

				}
			}
			
		}
		break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// “关于”框的消息处理程序。
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
