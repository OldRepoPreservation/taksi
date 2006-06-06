//
// HotKeys.h
//
#ifndef _INC_Input
#define _INC_Input
#if _MSC_VER > 1000
#pragma once
#endif

#ifdef USE_DX
struct CTaksiDI : public CDllFile
{
	// Direct input keys.
	// NOTE: Process/thread specific interface.
public:
	CTaksiDI();

	HRESULT SetupDirectInput();
	void ProcessDirectInput();
	void CloseDirectInput();

public:
	bool m_bSetup;
private:
	// key states (for DirectInput) 
	BYTE m_bScanExt[3];	// VK_SHIFT, VK_CONTROL, VK_MENU scan codes.
	bool m_abHotKey[TAKSI_HOTKEY_QTY];
};
extern CTaksiDI g_UserDI;
#endif // USE_DX

struct CTaksiKeyboard
{
	// keyboard hook
	// (Use ONLY If the DI interface doesnt work)
	// NOTE: Process and Thread-specific keyboard hook.
public:
	CTaksiKeyboard()
		: m_hHookKeys(NULL)
		, m_bHotMask(0)
	{
	}
	bool InstallHookKeys(bool bDummy);
	void UninstallHookKeys(void);

protected:
	static LRESULT CALLBACK DummyKeyboardProc(int code, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);

public:
	// keyboard hook handle
	HHOOK m_hHookKeys;
	BYTE m_bHotMask; // the state of HOTKEYF_CONTROL and HOTKEYF_SHIFT
};
extern CTaksiKeyboard g_UserKeyboard;

struct CTaksiHotKeys
{
	// Taksi DLL HotKey state information
	// Changed by user pressing specific keys.
public:
	CTaksiHotKeys()
		: m_bAttachedHotKeys(false)
		, m_dwHotKeyMask(0)
	{
	}

	void DoHotKey( TAKSI_HOTKEY_TYPE eHotKey );
	bool IsHotKey( TAKSI_HOTKEY_TYPE eHotKey ) const
	{
		return( m_dwHotKeyMask & (1<<eHotKey));
	}
	void SetHotKey( TAKSI_HOTKEY_TYPE eHotKey )
	{
		m_dwHotKeyMask |= (1<<eHotKey);
	}
	void ClearHotKey( TAKSI_HOTKEY_TYPE eHotKey )
	{
		m_dwHotKeyMask &= ~(1<<eHotKey);
	}

	HRESULT AttachHotKeysToApp();	// to current app/process.
	void DetachHotKeys();

public:
	bool m_bAttachedHotKeys;	// hooked the keyboard for this process.
	DWORD m_dwHotKeyMask;	// TAKSI_HOTKEY_TYPE mask
};
extern CTaksiHotKeys g_HotKeys;

#endif
