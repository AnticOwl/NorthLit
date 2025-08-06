export module UI;

import std;

export class UI
{
public:
	static UI& GetInstance();

	bool Init();
	void Shutdown();

	void Draw();

	void Toggle() { SetVisible(!m_Visible); }
	void SetVisible(bool visible);

	bool IsVisible() { return m_Visible; }
	bool HasKbFocus() { return m_Visible && m_HasKeyboardFocus; }

	void RegisterDrawCb(std::function<void()> cb) { m_DrawCbs.emplace_back(cb); }

	void StartHotkeyCapture(int oldKey);
	bool GetCapturedHotkey(int& outHotkey);

private:
	UI();
	~UI();

	bool m_HasKeyboardFocus;
	bool m_Visible;

	std::vector<std::function<void()>> m_DrawCbs;
};