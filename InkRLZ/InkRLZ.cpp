
#include <SDKDDKVer.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <midlbase.h>
#include <gdiplus.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <windowsx.h>

static WCHAR szTitle[] = L"InkRLZ";
static WCHAR szWindowClass[] = L"InkRLZWnd";

struct Application;

static BOOL                AppInit( Application * app, HINSTANCE );
static LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );

enum
{
    HK_INK_MODE = 1,
};

class __declspec( uuid( "04453f9d-8071-4993-a308-a96688ce35d8" ) ) NotifyIcon;

static Gdiplus::Color s_penColors[] =
{
    Gdiplus::Color{ 255, 216, 244, 162, },
    Gdiplus::Color{ 255, 235, 180, 55, },
    Gdiplus::Color{ 255, 255, 255, 255, },
};

struct Line
{
    Gdiplus::Color m_color;
    float m_lineWidth;
    std::vector<Gdiplus::Point> m_points;
};

struct InkData
{
    uint32_t m_lineMax;
    std::vector<Line> m_lines;
};

struct Application
{
    HINSTANCE m_hInstance;
    HWND m_drawWnd; // Draws all graphics
    HWND m_inputWnd; // Captures input
    HCURSOR m_inkCursor;
    InkData m_inkData;
    Line * m_drawingLine = nullptr;
    uint32_t m_curColorIndex = 0;
    float m_curLineWidth = 4.0f;
    bool m_inkModeEnabled = false;
} g_app = {};

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );
    UNREFERENCED_PARAMETER( nCmdShow );

    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup( &gdiplusToken, &gdiplusStartupInput, nullptr );

    if( !AppInit( &g_app, hInstance ) )
    {
        return FALSE;
    }

    MSG msg;

    while( GetMessage( &msg, nullptr, 0, 0 ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    Gdiplus::GdiplusShutdown( gdiplusToken );
    return (int) msg.wParam;
}

static SIZE WndSize( HWND hWnd )
{
    RECT rc = { 0 };
    GetWindowRect( hWnd, &rc );
    OffsetRect( &rc, -rc.left, -rc.top );
    return { rc.right, rc.bottom };
}

static void PaintInkData( Gdiplus::Graphics & g, InkData * data )
{
    g.SetSmoothingMode( Gdiplus::SmoothingModeAntiAlias );

    for( uint32_t i = 0; i < data->m_lineMax; i++ )
    {
        const Line & line = data->m_lines[ i ];
        Gdiplus::Pen pen{ line.m_color, line.m_lineWidth };
        pen.SetLineCap( Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapFlat );
        g.DrawLines( &pen, line.m_points.data(), (INT)line.m_points.size() );
    }
}

static void PaintApplication( Application* app )
{
    HWND hWnd = app->m_drawWnd;
    HDC hdcWin = GetDC( hWnd );
    HDC hdcMem = CreateCompatibleDC( hdcWin );
    SIZE size = WndSize( hWnd );
    HBITMAP hMemBmp = CreateCompatibleBitmap( hdcWin, size.cx, size.cy );
    SelectObject( hdcMem, hMemBmp );

    Gdiplus::Graphics g{ hdcMem };
    PaintInkData( g, &app->m_inkData );

    BLENDFUNCTION blend;
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT ptZero{};
    UpdateLayeredWindow( hWnd, nullptr, nullptr, &size, hdcMem, &ptZero, 0, &blend, ULW_ALPHA );

    DeleteDC( hdcMem );
    DeleteObject( hMemBmp );

    ReleaseDC( hWnd, hdcWin );
}

static void SetInputEnabled( Application * app, bool input )
{
    ShowWindow( app->m_inputWnd, input ? SW_SHOW : SW_HIDE );
}

static BOOL AppInit( Application * app, HINSTANCE hInstance )
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = nullptr;
    wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
    wcex.hbrBackground = nullptr;
    wcex.lpszMenuName = L"";
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = nullptr;
    RegisterClassExW( &wcex );

    app->m_hInstance = hInstance; // Store instance handle in our global variable

    int sx = GetSystemMetrics( SM_CXSCREEN );
    int sy = GetSystemMetrics( SM_CYSCREEN );

    app->m_drawWnd = CreateWindowExW( WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        szWindowClass, szTitle, WS_POPUP,
        0, 0, sx, sy, nullptr, nullptr, hInstance, nullptr );
    app->m_inputWnd = CreateWindowExW( WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        szWindowClass, szTitle, WS_POPUP,
        0, 0, sx, sy, nullptr, nullptr, hInstance, nullptr );

    UpdateWindow( app->m_inputWnd );

    ShowWindow( app->m_drawWnd, SW_SHOW );
    SetWindowPos( app->m_drawWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE );
    UpdateWindow( app->m_drawWnd );

    SetWindowPos( app->m_inputWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE );
    SetLayeredWindowAttributes( app->m_inputWnd, 0, 1, LWA_ALPHA );

    RegisterHotKey( app->m_inputWnd, HK_INK_MODE, MOD_CONTROL | MOD_ALT, 'V' );

    return TRUE;
}

static HCURSOR CreateCircleCursor( UINT w, UINT h, UINT colorIndex, float size )
{
    HDC hDC = GetDC( NULL );

    HDC andDC = CreateCompatibleDC( hDC );
    HDC xorDC = CreateCompatibleDC( hDC );

    HBITMAP andBmp = CreateCompatibleBitmap( hDC, w, h );
    HBITMAP xorBmp = CreateCompatibleBitmap( hDC, w, h );
    SelectObject( andDC, andBmp );
    SelectObject( xorDC, xorBmp );

    const Gdiplus::Color & c = s_penColors[ colorIndex ];
    COLORREF dst = RGB( c.GetR(), c.GetG(), c.GetB() );

    int cx = (int)( w / 2 );
    int cy = (int)( h / 2 );

    for( int y = 0; y < (int)h; y++ )
    {
        for( int x = 0; x < (int)w; x++ )
        {
            float dist = hypotf( (float)( x - cx ), (float)( y - cy ) );
            if( dist * 2.0f <= size )
            {
                SetPixel( andDC, x, y, RGB( 0, 0, 0 ) );
                SetPixel( xorDC, x, y, dst );
            }
            else
            {
                SetPixel( andDC, x, y, RGB( 255, 255, 255 ) );
                SetPixel( xorDC, x, y, RGB( 0, 0, 0 ) );
            }
        }
    }

    ICONINFO icon;
    icon.fIcon = false;
    icon.hbmColor = xorBmp;
    icon.hbmMask = andBmp;
    icon.xHotspot = w / 2;
    icon.yHotspot = h / 2;

    DeleteDC( xorDC );
    DeleteDC( andDC );
    ReleaseDC( nullptr, hDC );

    return CreateIconIndirect( &icon );
}

static void SetInkCursor( Application * app )
{
    if( app->m_inkCursor )
    {
        DestroyCursor( app->m_inkCursor );
    }
    app->m_inkCursor = CreateCircleCursor( 64, 64, app->m_curColorIndex, app->m_curLineWidth );
    SetCursor( app->m_inkCursor );
}

static void InkMode( Application * app )
{
    app->m_inkModeEnabled = !app->m_inkModeEnabled;
    SetInputEnabled( app, app->m_inkModeEnabled );
    if( app->m_inkModeEnabled )
    {
        SetWindowPos( app->m_inputWnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW );
        SetInkCursor( app );
    }
}

static void StartDrawingLine( Application * app )
{
    if( app->m_inkData.m_lines.size() != app->m_inkData.m_lineMax )
    {
        // Clip off undo history.
        app->m_inkData.m_lines.resize( app->m_inkData.m_lineMax );
    }

    app->m_inkData.m_lines.emplace_back();
    app->m_drawingLine = &app->m_inkData.m_lines[ app->m_inkData.m_lines.size() - 1 ];
    app->m_drawingLine->m_color = s_penColors[ app->m_curColorIndex ];
    app->m_drawingLine->m_lineWidth = app->m_curLineWidth;
    app->m_inkData.m_lineMax++;
}

static void AppendToLine( Application* app, LPARAM lParam )
{
    INT x = GET_X_LPARAM( lParam );
    INT y = GET_Y_LPARAM( lParam );
    Gdiplus::Point p{ x, y };
    app->m_drawingLine->m_points.push_back( p );

    PaintApplication( app );
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Application * app = &g_app;

    switch (message)
    {
    case WM_LBUTTONDOWN:
        StartDrawingLine( app );
        AppendToLine( app, lParam );
        break;
    case WM_LBUTTONUP:
        app->m_drawingLine = nullptr;
        break;
    case WM_MOUSEMOVE:
        if( hWnd == app->m_inputWnd && app->m_drawingLine )
        {
            AppendToLine( app, lParam );
        }
        break;
    case WM_KEYDOWN:
        {
            if( wParam == 'Z' )
            {
                if( app->m_inkData.m_lineMax > 0 )
                {
                    app->m_inkData.m_lineMax--;
                    PaintApplication( app );
                }
            }
            else if( wParam == 'X' )
            {
                if( app->m_inkData.m_lineMax < app->m_inkData.m_lines.size() )
                {
                    app->m_inkData.m_lineMax++;
                    PaintApplication( app );
                }
            }
            else if( wParam == 'C' )
            {
                app->m_inkData.m_lineMax = 0;
                PaintApplication( app );
            }
            else if( wParam == VK_OEM_4 ) // [
            {
                if( app->m_curLineWidth > 0.0f )
                {
                    app->m_curLineWidth -= 2.0f;
                }
                SetInkCursor( app );
            }
            else if( wParam == VK_OEM_6 ) // ]
            {
                if( app->m_curLineWidth < 20.0f )
                {
                    app->m_curLineWidth += 2.0f;
                }
                SetInkCursor( app );
            }
            else if( wParam >= '1' && wParam <= '9' )
            {
                uint32_t penIndex = (uint32_t)( wParam - '1' );
                if( penIndex >= ARRAYSIZE( s_penColors ) )
                {
                    break;
                }

                app->m_curColorIndex = penIndex;
                SetInkCursor( app );
            }
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SETCURSOR:
        SetInkCursor( app );
        break;
    case WM_HOTKEY:
        {
            switch( wParam )
            {
            case HK_INK_MODE:
                InkMode( app );
                break;
            }
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
