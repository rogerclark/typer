#include <windows.h>

#include "resource.h"

typedef struct {
	wchar_t filename[MAX_PATH];
	LOGFONT font;
	BOOL hasUnsavedChanges;
} DOCUMENT_STATE;

typedef struct {
	DOCUMENT_STATE document;
	HWND hWnd;
	HWND hWndEdit;
	HINSTANCE hInstance;
	HICON hIcon;
	HFONT hFont;
} APP_STATE;

APP_STATE app;

const UINT TYPER_EDIT_ID = 0x573;
const wchar_t* TYPER_WNDCLASS = L"Typer";
const wchar_t* TYPER_CONFIG_FILENAME = L"C:\\typer.ini";

void Typer_UpdateTitlebar() {
	const wchar_t* suffixUnchanged = L"";
	const wchar_t* suffixChanged = L"*";

	enum { BUFSIZE = MAX_PATH * 2 };

	wchar_t titleBuffer[BUFSIZE];
	ZeroMemory(titleBuffer, BUFSIZE);

	wchar_t* filenameOrUntitled = lstrlen(app.document.filename) ? app.document.filename : L"Untitled";

	wsprintf(titleBuffer, L"%s%s - Typer", filenameOrUntitled,
		app.document.hasUnsavedChanges ? suffixChanged : suffixUnchanged);
	SetWindowText(app.hWnd, titleBuffer);
}

void Typer_FilenameChange(const wchar_t* filename) {
	lstrcpy(app.document.filename, filename);
	Typer_UpdateTitlebar();
}

BOOL Typer_OpenFile(const wchar_t* filename) {
	BOOL success = FALSE;
	BYTE* fileBuffer = (BYTE*)GlobalAlloc(GMEM_FIXED, 1024 * 1024);
	BYTE* filePosition = fileBuffer;

	// Open the file handle
	HANDLE file = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); 
	if (file != INVALID_HANDLE_VALUE) {
		// Read the contents of the file
		enum { BUFSIZE = 4096 };
		BYTE readBuffer[BUFSIZE];
		DWORD bytesRead = 0;

		while (ReadFile(file, readBuffer, BUFSIZE, &bytesRead, NULL) && (bytesRead != 0)) {
			memcpy(filePosition, readBuffer, bytesRead);
			filePosition += bytesRead;
		}

		*filePosition = 0;

		if (filePosition != fileBuffer) {
			app.document.hasUnsavedChanges = FALSE;
			Typer_FilenameChange(filename);
			SetWindowTextA(app.hWndEdit, (char*)fileBuffer);
			success = TRUE;
		}

		CloseHandle(file);
	}
	GlobalFree(fileBuffer);
	return success;
}

BOOL Typer_SaveFile(const wchar_t* filename) {
	BOOL success = FALSE;
	enum { BUFSIZE = 1024 * 1024 };
	char* fileBuffer = (char*)GlobalAlloc(GMEM_FIXED, BUFSIZE);

	int length = GetWindowTextA(app.hWndEdit, fileBuffer, BUFSIZE);
	if (length != 0) {
		HANDLE file = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE) {
			DWORD bytesWritten = 0;
			if (WriteFile(file, fileBuffer, length, &bytesWritten, NULL)) {
				if (bytesWritten == length) {
					app.document.hasUnsavedChanges = FALSE;
					Typer_FilenameChange(filename);
					success = TRUE;
				}
			}
			CloseHandle(file);
		}
	}
	GlobalFree(fileBuffer);
	return success;
}

void Typer_ShowSaveAs() {
	wchar_t filenameBuffer[MAX_PATH];
	ZeroMemory(&filenameBuffer, MAX_PATH * sizeof(wchar_t));

	OPENFILENAME ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hInstance = app.hInstance;
	ofn.hwndOwner = app.hWnd;
	ofn.lpstrTitle = L"Save As";
	ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filenameBuffer;
	ofn.nMaxFile = MAX_PATH;

	if (GetSaveFileName(&ofn)) {
		Typer_SaveFile(filenameBuffer);
	}
}

void Typer_SaveOrPrompt() {
	if (lstrlen(app.document.filename) == 0) {
		Typer_ShowSaveAs();
	} else {
		Typer_SaveFile(app.document.filename);
	}
}

LRESULT CALLBACK Typer_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_CREATE: {
			app.hWndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_MULTILINE,
				0, 0, 0, 0, hWnd, (HMENU)TYPER_EDIT_ID, app.hInstance, NULL);
			app.hWnd = hWnd;
			app.hFont = CreateFontIndirect(&app.document.font);
			SendMessage(app.hWndEdit, WM_SETFONT, (WPARAM)app.hFont, TRUE);

			Typer_UpdateTitlebar();

			ShowWindow(app.hWndEdit, SW_SHOW);
			ShowWindow(hWnd, SW_SHOW);
			InvalidateRect(hWnd, NULL, TRUE);
			break;
		}
		case WM_SIZE: {
			RECT rect;
			int width;
			int height;
			GetClientRect(hWnd, &rect);

			width = rect.right - rect.left;
			height = rect.bottom - rect.top;

			MoveWindow(app.hWndEdit, 0, 0, width, height, FALSE);
			
			break;
		}
		case WM_COMMAND: {
			UINT id = LOWORD(wParam);
			switch (id) {
				case TYPER_EDIT_ID: {
					UINT notification = HIWORD(wParam);
					if (notification == EN_CHANGE) {
						app.document.hasUnsavedChanges = TRUE;
						Typer_UpdateTitlebar();
					}
					break;
				}
				case ID_TYPER_FILE_OPEN: {
					wchar_t filenameBuffer[MAX_PATH];
					ZeroMemory(&filenameBuffer, MAX_PATH * sizeof(wchar_t));

					OPENFILENAME ofn = { 0 };
					ofn.lStructSize = sizeof(ofn);
					ofn.hInstance = app.hInstance;
					ofn.hwndOwner = hWnd;
					ofn.lpstrTitle = L"Open File";
					ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
					ofn.lpstrFile = filenameBuffer;
					ofn.nMaxFile = MAX_PATH;

					if (GetOpenFileName(&ofn)) {
						Typer_OpenFile(filenameBuffer);
					}

					break;
				}
				case ID_TYPER_FILE_SAVE: {
					// Check to see if app.document.filename is valid
					Typer_SaveOrPrompt();
					break;
				}
				case ID_TYPER_FILE_SAVE_AS: {
					Typer_ShowSaveAs();
					break;
				}
				case ID_TYPER_FORMAT_FONT: {
					LOGFONT logfont = { 0 };

					CHOOSEFONT cf = { 0 };
					cf.lStructSize = sizeof(CHOOSEFONT);
					cf.hwndOwner = hWnd;
					cf.lpLogFont = &logfont;
					cf.hInstance = app.hInstance;
					cf.nFontType = SCREEN_FONTTYPE;
					cf.Flags = CF_SCREENFONTS;

					if (ChooseFont(&cf)) {
						if (app.hFont != NULL) {
							DeleteObject(app.hFont);
						}

						app.hFont = CreateFontIndirect(&logfont);
						memcpy(&app.document.font, &logfont, sizeof(LOGFONT));
						SendMessage(app.hWndEdit, WM_SETFONT, (WPARAM)app.hFont, TRUE);
					}
					break;
				}
				case ID_HELP_ABOUT: {
					wchar_t* aboutText = L"Typer Version 0.2\r\nby @rogerclark\r\ngreetz to EFnet #winprog & Twitch chat";
					MessageBox(NULL, aboutText, L"About", MB_OK);
					break;
				}
				case ID_FILE_EXIT: {
					DestroyWindow(hWnd);
					break;
				}
			}
			break;
		}
		case WM_CLOSE: {
			BOOL shouldClose = FALSE;

			if (app.document.hasUnsavedChanges) {
				UINT result = MessageBox(hWnd, L"The file has unsaved changes. Do you want to save?",
					L"Typer", MB_YESNOCANCEL | MB_ICONWARNING);
				if (result == IDYES) {
					Typer_SaveOrPrompt();
					shouldClose = TRUE;
				} else if (result == IDNO) {
					shouldClose = TRUE;
				} else if (result == IDCANCEL) {
					shouldClose = FALSE;
				}
			} else {
				shouldClose = TRUE;
			}

			// if shouldClose is TRUE, then DefWindowProc will call DestroyWindow()
			if (shouldClose == FALSE) {
				return 0;
			}

			break;
		}
		case WM_DESTROY: {
			if (app.hFont != NULL) {
				DeleteObject(app.hFont);
			}

			PostQuitMessage(0);
			break;
		}
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

BOOL Typer_RegisterClass(HINSTANCE instance) {
	WNDCLASSEX wc = { 0 };

	ZeroMemory(&wc, sizeof(WNDCLASSEX));
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = Typer_WndProc;
	wc.hInstance = instance;
	wc.lpszClassName = TYPER_WNDCLASS;
	wc.lpszMenuName = MAKEINTRESOURCE(IDM_TYPER);
	wc.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_TYPER));

	return (RegisterClassEx(&wc) != 0);
}

BOOL WritePrivateProfileInt(const wchar_t* section, const wchar_t* keyname, LONG value, const wchar_t* inifile) {
	wchar_t valueString[32] = { 0 };
	wsprintf(valueString, L"%d", value);
	return WritePrivateProfileString(section, keyname, valueString, inifile);
}

const wchar_t* TYPER_CONFIG_KEY_FONT = L"Font";

void Typer_SaveSettings() {
	WritePrivateProfileString(TYPER_CONFIG_KEY_FONT, L"FaceName", app.document.font.lfFaceName, TYPER_CONFIG_FILENAME);
	WritePrivateProfileInt(TYPER_CONFIG_KEY_FONT, L"Weight", app.document.font.lfWeight, TYPER_CONFIG_FILENAME);
	WritePrivateProfileInt(TYPER_CONFIG_KEY_FONT, L"Height", app.document.font.lfHeight, TYPER_CONFIG_FILENAME);
	WritePrivateProfileInt(TYPER_CONFIG_KEY_FONT, L"Italic", app.document.font.lfItalic, TYPER_CONFIG_FILENAME);
}

void Typer_LoadSettings() {
	GetPrivateProfileString(TYPER_CONFIG_KEY_FONT, L"FaceName", L"Courier New", app.document.font.lfFaceName, LF_FACESIZE, TYPER_CONFIG_FILENAME);
	app.document.font.lfWeight = GetPrivateProfileInt(TYPER_CONFIG_KEY_FONT, L"Weight", 400, TYPER_CONFIG_FILENAME);
	app.document.font.lfHeight = (INT)GetPrivateProfileInt(TYPER_CONFIG_KEY_FONT, L"Height", -27, TYPER_CONFIG_FILENAME);
	app.document.font.lfItalic = GetPrivateProfileInt(TYPER_CONFIG_KEY_FONT, L"Italic", 0, TYPER_CONFIG_FILENAME);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, PSTR cmdLine, int show) {
	if (!Typer_RegisterClass(instance)) {
		MessageBox(NULL, L"Couldn't register window class!", L"Error", MB_OK | MB_ICONERROR);
	}

	ZeroMemory(&app, sizeof(APP_STATE));
	app.hInstance = instance;

	Typer_LoadSettings();

	app.hWnd = CreateWindowEx(0, TYPER_WNDCLASS, L"Typer", WS_OVERLAPPEDWINDOW, 0, 0, 960, 540, NULL, NULL, instance, NULL);

	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Typer_SaveSettings();

	return 0;
}