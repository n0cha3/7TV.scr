#define _USE_MATH_DEFINES
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ScrnSave.h>
#include <math.h>
#include <mmsystem.h>

#include "resource.h"

INT32 g_Scale = 1;
MCIDEVICEID g_DevID = 0;
PWSTR g_FilePath[MAX_PATH];
BOOL MusicEnabled = TRUE;

#define SECMIN_TO_RAD(T) (FLOAT)(T * (M_PI / 30))
#define HOUR_TO_RAD(T) (FLOAT)(T * (M_PI / 6))

BOOL WINAPI SetOpt(BOOL Mus) {
    HKEY SettingsKey;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\7TVClock", 0, KEY_SET_VALUE, &SettingsKey) != ERROR_SUCCESS) {
        RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\7TVClock", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &SettingsKey, NULL);
    }
    LSTATUS Status1 = RegSetValueExW(SettingsKey, L"MusicEnabled", 0, REG_DWORD, (PBYTE)&Mus, sizeof(BOOL));

    if (Status1 != ERROR_SUCCESS) return FALSE;
    RegCloseKey(SettingsKey);
    return TRUE;
}  

BOOL WINAPI GetOpts(VOID) {
    HKEY SettingsKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\7TVClock", 0, KEY_QUERY_VALUE, &SettingsKey) != ERROR_SUCCESS) return FALSE;

    DWORD Size = sizeof(BOOL),
        Type = REG_DWORD;

    LSTATUS Status1 = RegQueryValueExW(SettingsKey, L"MusicEnabled", NULL, &Type, (PBYTE)&MusicEnabled, &Size);

    if (Status1 != ERROR_SUCCESS) return FALSE;
    RegCloseKey(SettingsKey);
    return TRUE;
}

BOOL WINAPI ExtractMusic(VOID) {
    PWSTR TempPath[MAX_PATH];

    if (GetTempPathW(MAX_PATH, (PWSTR)TempPath) == 0) return FALSE;
    if (GetTempFileNameW((PWSTR)TempPath, L"7tv", 0, (PWSTR)g_FilePath) != 0) DeleteFileW((PWSTR)g_FilePath); 

    wcscat((PWSTR)g_FilePath, L".wav");

    HRSRC Res = FindResourceW(NULL, MAKEINTRESOURCEW(IDC_WAVE), L"WAVE");

    if (Res == NULL) return FALSE;
    HGLOBAL ResHand = LoadResource(NULL, Res);
    SIZE_T Size = SizeofResource(NULL, Res);

    if (ResHand == NULL) return FALSE;
    LPVOID ResMem = LockResource(ResHand);

    HANDLE TempFile = CreateFileW((PWSTR)g_FilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (TempFile == NULL) {
        FreeResource(ResHand);
        return FALSE;
    }

    DWORD Written = 0;

    WriteFile(TempFile, ResMem, (DWORD)Size, &Written, NULL);
    CloseHandle(TempFile);
    FreeResource(ResHand);
    return TRUE;
}

VOID WINAPI PlayMusicFromOffset(UINT32 Offset, HWND Wnd) {
    if (g_DevID == 0) {
        MCI_OPEN_PARMSW MciParm = {
            .lpstrDeviceType = L"waveaudio",
            .lpstrElementName = (PWSTR)g_FilePath
        };

        if (mciSendCommandW(0, MCI_OPEN, MCI_OPEN_ELEMENT, (LPARAM)&MciParm)) return;

        g_DevID = MciParm.wDeviceID;
    }
    
    MCI_PLAY_PARMS PlayParm = {
        .dwFrom = Offset * 1000,
        .dwCallback = (DWORD_PTR) Wnd
    };

    mciSendCommandW(g_DevID, MCI_PLAY, MCI_NOTIFY | MCI_FROM, (LPARAM)&PlayParm);
}

VOID WINAPI DrawWatchFace(HDC DC, RECT WindowRect);

LRESULT WINAPI ScreenSaverProcW(HWND Wnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    SYSTEMTIME Time;
    GetLocalTime(&Time);
    switch (Msg) {
        case WM_CREATE: {
            SetTimer(Wnd, 1, 1000/5, NULL);

            if (GetOpts() == FALSE) {
                SetOpt(TRUE);
            }

            if (MusicEnabled == TRUE && fChildPreview == FALSE) {
                if (ExtractMusic() == TRUE) PlayMusicFromOffset(Time.wSecond, Wnd);
            }

            return DefScreenSaverProc(Wnd, Msg, wParam, lParam);
        }

        case MM_MCINOTIFY: {
            PlayMusicFromOffset(Time.wSecond, Wnd);
            break;
        }

        case WM_DESTROY: {
            KillTimer(Wnd, 1);
            if (g_DevID != 0) mciSendCommand(g_DevID, MCI_CLOSE, 0, (DWORD_PTR)NULL);
            if (MusicEnabled == TRUE && fChildPreview == FALSE) DeleteFileW((PWSTR)g_FilePath);
            break;
        }

        case WM_TIMER: {
            InvalidateRect(Wnd, NULL, TRUE);
            break;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT: {
            PAINTSTRUCT Paint;
            HDC PaintDC = BeginPaint(Wnd, &Paint);
            HBRUSH Brush = CreateSolidBrush(RGB(14, 15, 129));
            
            HDC MemDC = CreateCompatibleDC(PaintDC);
            INT32 Width = Paint.rcPaint.right - Paint.rcPaint.left, 
                Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

            if (Width < 800) g_Scale = 2;
            if (fChildPreview) g_Scale = 7;
            

            HBITMAP MemBM = CreateCompatibleBitmap(PaintDC, Width * g_Scale, Height * g_Scale),
                OldBM = SelectObject(MemDC, MemBM);

            RECT WindowRect;
            GetClientRect(Wnd, &WindowRect);

            WindowRect.bottom *= g_Scale;
            WindowRect.left *= g_Scale;
            WindowRect.right *= g_Scale;
            WindowRect.top *= g_Scale;

            FillRect(MemDC, &WindowRect, Brush);
            DeleteObject(Brush);

            DrawWatchFace(MemDC, WindowRect);

            StretchBlt(PaintDC, 0, 0, Width, Height, MemDC, 0, 0, Width * g_Scale, Height * g_Scale, SRCCOPY);

            SelectObject(MemDC, OldBM);
            DeleteObject(MemBM);
            DeleteDC(MemDC);

            EndPaint(Wnd, &Paint);
            break;
        }

        default: {
            return DefScreenSaverProc(Wnd, Msg, wParam, lParam);
        }
    }
   return 0;
}

BOOL CALLBACK ScreenSaverConfigureDialog(HWND Wnd, UINT Message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);

    switch (Message) {
        case WM_INITDIALOG: {
            if (GetOpts() == FALSE) {
                SetOpt(TRUE);
            }
            
            if (MusicEnabled)
                SendDlgItemMessageA(Wnd, IDC_CHECK1, BM_SETCHECK, 1, 0);
            return TRUE;
        }

        case WM_COMMAND: {
			switch(LOWORD(wParam)) {
				case IDOK: {
                    LRESULT MusicCheck = SendDlgItemMessageA(Wnd, IDC_CHECK1, BM_GETCHECK, 0, 0);

                    MusicEnabled = MusicCheck ? TRUE : FALSE;

                    if (SetOpt(MusicEnabled) == FALSE) 
                        MessageBoxW(Wnd, L"Failed to save settings", L"7TV Screensaver", MB_ICONERROR | MB_OK);

					EndDialog(Wnd, LOWORD(wParam));
				    break;
                }
				case IDCANCEL: {
					EndDialog(Wnd, LOWORD(wParam));
				    break;
                }
			}
		    return TRUE;
        }
        
    }

    return FALSE;
}

BOOL WINAPI RegisterDialogClasses(HANDLE Instance) {
    UNREFERENCED_PARAMETER(Instance);
	return TRUE;
}

VOID WINAPI DrawArcR(HDC DC, INT32 XCentre, INT32 YCentre, FLOAT ArcStart, FLOAT ArcEnd, INT32 Radius, BOOL Clockwise);
VOID WINAPI DrawHand(HDC DC, INT32 XCentre, INT32 YCentre, FLOAT RotationAngle, INT32 Width, INT32 Height, INT32 EllipseWidth, INT32 EllipseHeight);

VOID WINAPI DrawWatchFace(HDC DC, RECT WindowRect) {
    CONST INT32 XCentre = (WindowRect.right - WindowRect.left) / 2 , 
        YCentre = (WindowRect.bottom - WindowRect.top) / 2,
        ClockR = 272;

    HBRUSH Brush = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(DC, Brush);

    FLOAT BoldAngles[4] = {
        0, 
        (FLOAT)M_PI, 
        (FLOAT)M_PI / 2, 
        3 * (FLOAT)M_PI / 2
    };

    INT32 BoldCircleR = 13;

    for (INT32 a = 0; a < 4; a++) {
        INT32 X = XCentre + (INT32)(ClockR * cos(BoldAngles[a])),
            Y = YCentre - (INT32)(ClockR * sin(BoldAngles[a]));

        Ellipse(DC, X - BoldCircleR, Y - BoldCircleR, X + BoldCircleR, Y + BoldCircleR);
    }

    FLOAT SmallAngles[6] = {
        4 * (FLOAT)M_PI / 3, 
        7 * (FLOAT)M_PI / 6, 
        11 * (FLOAT)M_PI / 6, 
        5 * (FLOAT)M_PI / 3, 
        (FLOAT)M_PI / 6, 
        (FLOAT)M_PI / 3
    };

    INT32 SmallCircleR = 5;

    for (INT32 a = 0; a < 6; a++) {
        INT32 X = XCentre + (INT32)((ClockR * cosf(SmallAngles[a]))),
            Y = YCentre - (INT32)(ClockR * sinf(SmallAngles[a]));

        Ellipse(DC, X - SmallCircleR, Y - SmallCircleR, X + SmallCircleR, Y + SmallCircleR);
    }

    for (FLOAT a = (FLOAT)M_PI - 0.1f; a > M_PI / 2; a -= 0.1f) {
        INT32 X = (INT32)(XCentre + ClockR * cosf(a)),
            Y = (INT32)(YCentre - ClockR * sinf(a));

        Ellipse(DC, X - SmallCircleR, Y - SmallCircleR, X + SmallCircleR, Y + SmallCircleR);
    }

    DeleteObject(Brush);
    
    HPEN Pen = CreatePen(PS_SOLID, 12, RGB(255, 255, 255));

    SelectObject(DC, Pen);
    
    CONST INT32 ArcR = ClockR + 50;

    CONST FLOAT ArcAngStart = (FLOAT)-M_PI, 
        ArcAngEnd = (FLOAT)M_PI / 2;

    CONST INT32 StartX = XCentre + (INT32)(ArcR * cosf(ArcAngStart)), 
        StartY = YCentre - (INT32)(ArcR * sinf(ArcAngStart)),
        EndX = XCentre + (INT32)(ArcR * cosf(ArcAngEnd)),
        EndY = YCentre - (INT32)(ArcR * sinf(ArcAngEnd));

    Arc(DC, XCentre - ArcR, YCentre - ArcR, XCentre + ArcR, YCentre + ArcR, 
        StartX, StartY, EndX, EndY);
    
    Rectangle(DC, StartX + 2, StartY, StartX, StartY - 2);
    Rectangle(DC, EndX + 2, EndY, EndX, EndY + 2);
    
    DeleteObject(Pen);

    if (!fChildPreview) {
        Pen = CreatePen(PS_SOLID, 4, RGB(255, 255, 255));
    
        SelectObject(DC, Pen);

        CONST INT32 SecondsArcR = ArcR + 16;

        SYSTEMTIME Time;
        GetLocalTime(&Time);

        FLOAT SecondsRad = SECMIN_TO_RAD(Time.wSecond),
            MinuteRad = SECMIN_TO_RAD(Time.wMinute),
            HourRad = HOUR_TO_RAD(Time.wHour);
    
        if (SecondsRad == 0) {
            DrawArcR(DC, XCentre, YCentre, 0, 3 * (FLOAT)M_PI / 2, SecondsArcR, TRUE);
        }

        if (SecondsRad <= M_PI && SecondsRad <= M_PI / 2 && SecondsRad != 0) {
            DrawArcR(DC, XCentre, YCentre, (FLOAT)M_PI / 2, (FLOAT)M_PI / 2 - SecondsRad, SecondsArcR, TRUE);
            DrawArcR(DC, XCentre, YCentre, 0, 3 * (FLOAT)M_PI / 2, SecondsArcR, TRUE);
            DrawArcR(DC, XCentre, YCentre, 3 * (FLOAT)M_PI / 2 - SecondsRad, 3 * (FLOAT)M_PI / 2, SecondsArcR, FALSE);
        }

        if (SecondsRad <= M_PI && SecondsRad >= M_PI / 2) {
            DrawArcR(DC, XCentre, YCentre, (FLOAT)M_PI - SecondsRad, 0, SecondsArcR, TRUE);
            DrawArcR(DC, XCentre, YCentre, -SecondsRad + (FLOAT)M_PI / 2, -SecondsRad - (FLOAT)M_PI / 2, SecondsArcR, TRUE);
        }
        
        if (SecondsRad >= M_PI && SecondsRad <= 3 * M_PI / 2) {
            DrawArcR(DC, XCentre, YCentre, -SecondsRad + (FLOAT)M_PI / 2, (FLOAT)M_PI / 2, SecondsArcR, TRUE);
        }

        if (SecondsRad >= M_PI && SecondsRad >= 3 * M_PI / 2) {
            DrawArcR(DC, XCentre, YCentre, -SecondsRad + (FLOAT)M_PI / 2, (FLOAT)M_PI / 2, SecondsArcR, TRUE);
            DrawArcR(DC, XCentre, YCentre, 0.000001f, 3 * (FLOAT)M_PI / 2 - SecondsRad, SecondsArcR, TRUE);
        }

        DrawHand(DC, XCentre, YCentre, SecondsRad, 3, 224, 3, 5);
        DrawHand(DC, XCentre, YCentre, MinuteRad, 13, 256, 15, 13);
        DrawHand(DC, XCentre, YCentre, HourRad, 17, 190, 18, 18);

        DeleteObject(Pen);
    }
}

VOID WINAPI DrawArcR(HDC DC, INT32 XCentre, INT32 YCentre, FLOAT ArcStart, FLOAT ArcEnd, INT32 Radius, BOOL Clockwise) {
    if (Clockwise)
        SetArcDirection(DC, AD_CLOCKWISE);
    else
        SetArcDirection(DC, AD_COUNTERCLOCKWISE);

    INT32 StartX = (INT32)(XCentre + Radius * cosf(ArcStart)),
        StartY = (INT32)(YCentre - Radius * sinf(ArcStart)),
        EndX = (INT32)(XCentre + Radius * cosf(ArcEnd)),
        EndY = (INT32)(YCentre - Radius * sinf(ArcEnd));

    Arc(DC, XCentre - Radius, YCentre - Radius, XCentre + Radius, YCentre + Radius, 
            StartX, StartY, EndX, EndY);
}

VOID WINAPI DrawHand(HDC DC, INT32 XCentre, INT32 YCentre, FLOAT RotationAngle, INT32 Width, INT32 Height, INT32 EllipseWidth, INT32 EllipseHeight) {
    SaveDC(DC);
    SetViewportOrgEx(DC, XCentre, YCentre, NULL);
    XFORM Rot = { 
            cosf(-RotationAngle), sinf(RotationAngle),
            sinf(-RotationAngle),  cosf(-RotationAngle), 
            0.0f, 0.0f
        };

    SetGraphicsMode(DC, GM_ADVANCED);
    SetWorldTransform(DC, &Rot);

    RoundRect(DC, 0, 24, -Width, -Height, EllipseWidth, EllipseHeight);

    SetGraphicsMode(DC, GM_COMPATIBLE);
    RestoreDC(DC, -1);
}
