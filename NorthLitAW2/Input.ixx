module;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Xinput.h>
#include <DirectXMath.h>
#include "imgui/imgui.h"

#pragma comment(lib, "Xinput9_1_0.lib")

export module Input;

import std;

using namespace DirectX;

namespace NorthLit::Input
{
	struct CameraInputSettings
	{
		bool KeyboardEnabled = true;
		bool ControllerEnabled = true;
		float FastMultiplier = 5.0f;
		float SlowMultiplier = 0.20f;
		float VerySlowMultiplier = 0.05f;
		float ControllerDeadzone = 0.18f;
		float ControllerRotationScale = 1.0f;
		float FovSpeed = 30.0f;
	};

	static CameraInputSettings s_Settings;
	static bool s_PreviousInsertDown = false;
	static bool s_PreviousLogDown = false;
	static bool s_PreviousPauseDown = false;
	static bool s_PreviousHudDown = false;
	static bool s_ToggleCameraRequested = false;
	static bool s_LogBasisRequested = false;
	static BYTE** s_PauseStructLocation = nullptr;
	static bool s_PauseResolveAttempted = false;
	static BYTE* s_HudPatchLocation = nullptr;
	static bool s_HudResolveAttempted = false;

	static bool IsForegroundProcess()
	{
		HWND foreground = GetForegroundWindow();
		if (!foreground) return false;
		DWORD pid = 0;
		GetWindowThreadProcessId(foreground, &pid);
		return pid == GetCurrentProcessId();
	}

	static bool IsKeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

	static BYTE** FindPauseStructLocation()
	{
		HMODULE module = GetModuleHandleW(nullptr);
		if (!module) return nullptr;

		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(module) + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

		BYTE* base = reinterpret_cast<BYTE*>(module);
		const size_t size = static_cast<size_t>(nt->OptionalHeader.SizeOfImage);

		// Frans AW2 camera:
		// 48 8B 2D | ?? ?? ?? ?? 48 8B 3D ?? ?? ?? ?? 48 8B 5B 08 48 8B 83 E0 80 05 00
		static constexpr BYTE pattern[] = {
			0x48,0x8B,0x2D,0,0,0,0,
			0x48,0x8B,0x3D,0,0,0,0,
			0x48,0x8B,0x5B,0x08,
			0x48,0x8B,0x83,0xE0,0x80,0x05,0x00
		};
		static constexpr char mask[] = "xxx????xxx????xxxxxxxxxxxx";

		if (size < sizeof(pattern)) return nullptr;
		for (size_t i = 0; i <= size - sizeof(pattern); ++i)
		{
			bool match = true;
			for (size_t j = 0; j < sizeof(pattern); ++j)
			{
				if (mask[j] == 'x' && base[i + j] != pattern[j])
				{
					match = false;
					break;
				}
			}
			if (!match) continue;

			BYTE* instruction = base + i;
			const int32_t displacement = *reinterpret_cast<const int32_t*>(instruction + 3);
			return reinterpret_cast<BYTE**>(instruction + 7 + displacement);
		}
		return nullptr;
	}

	static void TogglePause()
	{
		if (!s_PauseResolveAttempted)
		{
			s_PauseResolveAttempted = true;
			s_PauseStructLocation = FindPauseStructLocation();
		}

		if (!s_PauseStructLocation) return;
		BYTE* pauseStruct = *s_PauseStructLocation;
		if (!pauseStruct) return;

		// Exact field used by Frans' PauseFeature.
		BYTE* pauseByte = pauseStruct + 0x39;
		const BYTE current = *pauseByte;
		*pauseByte = (current == 1) ? 0 : 1;
	}

	static BYTE* FindHudPatchLocation()
	{
		HMODULE module = GetModuleHandleW(nullptr);
		if (!module) return nullptr;

		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const BYTE*>(module) + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

		BYTE* base = reinterpret_cast<BYTE*>(module);
		const size_t size = static_cast<size_t>(nt->OptionalHeader.SizeOfImage);

		// Frans AW2 HUD toggle location. Patch starts at match + 4:
		// 90 40 84 FF | 74 4D 44 8B 0D ?? ?? ?? ?? 0F B6 15 ?? ?? ?? ?? 81 FA 80 00 00 00 73 09
		static constexpr BYTE pattern[] = {
			0x90,0x40,0x84,0xFF,0x74,0x4D,0x44,0x8B,0x0D,0,0,0,0,
			0x0F,0xB6,0x15,0,0,0,0,0x81,0xFA,0x80,0x00,0x00,0x00,0x73,0x09
		};
		static constexpr char mask[] = "xxxxxxxxx????xxx????xxxxxxxx";

		if (size < sizeof(pattern)) return nullptr;
		for (size_t i = 0; i <= size - sizeof(pattern); ++i)
		{
			bool match = true;
			for (size_t j = 0; j < sizeof(pattern); ++j)
			{
				if (mask[j] == 'x' && base[i + j] != pattern[j])
				{
					match = false;
					break;
				}
			}
			if (match) return base + i + 4;
		}
		return nullptr;
	}

	static bool WriteHudBytes(BYTE first, BYTE second)
	{
		if (!s_HudPatchLocation) return false;
		DWORD oldProtect = 0;
		if (!VirtualProtect(s_HudPatchLocation, 2, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
		s_HudPatchLocation[0] = first;
		s_HudPatchLocation[1] = second;
		FlushInstructionCache(GetCurrentProcess(), s_HudPatchLocation, 2);
		DWORD ignored = 0;
		VirtualProtect(s_HudPatchLocation, 2, oldProtect, &ignored);
		return true;
	}

	static void ToggleHud()
	{
		if (!s_HudResolveAttempted)
		{
			s_HudResolveAttempted = true;
			s_HudPatchLocation = FindHudPatchLocation();
		}
		if (!s_HudPatchLocation) return;

		if (s_HudPatchLocation[0] == 0x74 && s_HudPatchLocation[1] == 0x4D)
		{
			WriteHudBytes(0x90, 0x90);
		}
		else if (s_HudPatchLocation[0] == 0x90 && s_HudPatchLocation[1] == 0x90)
		{
			WriteHudBytes(0x74, 0x4D);
		}
	}

	static float ApplyDeadzone(SHORT value, float deadzone)
	{
		const float normalized = std::clamp((float)value / 32767.0f, -1.0f, 1.0f);
		const float magnitude = std::abs(normalized);
		if (magnitude <= deadzone) return 0.0f;
		const float scaled = (magnitude - deadzone) / (1.0f - deadzone);
		return std::copysign(scaled, normalized);
	}

	static float TriggerValue(BYTE value)
	{
		constexpr float deadzone = 30.0f;
		if ((float)value <= deadzone) return 0.0f;
		return ((float)value - deadzone) / (255.0f - deadzone);
	}

	static XMVECTOR LoadAxis(const XMFLOAT4X4A& matrix, int row)
	{
		return XMVector3Normalize(XMVectorSet(matrix.m[row][0], matrix.m[row][1], matrix.m[row][2], 0.0f));
	}

	static void StoreAxis(XMFLOAT4X4A& matrix, int row, FXMVECTOR axis)
	{
		XMFLOAT3 v;
		XMStoreFloat3(&v, XMVector3Normalize(axis));
		matrix.m[row][0] = v.x;
		matrix.m[row][1] = v.y;
		matrix.m[row][2] = v.z;
	}

	static void Orthonormalize(XMFLOAT4X4A& matrix)
	{
		XMVECTOR right = LoadAxis(matrix, 0);
		XMVECTOR up = LoadAxis(matrix, 1);
		XMVECTOR forward = LoadAxis(matrix, 2);
		right = XMVector3Normalize(right);
		up = XMVectorSubtract(up, XMVectorScale(right, XMVectorGetX(XMVector3Dot(up, right))));
		up = XMVector3Normalize(up);
		const float handedness = XMVectorGetX(XMVector3Dot(XMVector3Cross(right, up), forward));
		forward = XMVector3Normalize(XMVector3Cross(right, up));
		if (handedness < 0.0f) forward = XMVectorNegate(forward);
		StoreAxis(matrix, 0, right);
		StoreAxis(matrix, 1, up);
		StoreAxis(matrix, 2, forward);
	}

	// Fully local rotation is kept for IGCS panorama operations.
	static void RotateAroundCurrentAxes(XMFLOAT4X4A& matrix, float pitch, float yaw, float roll)
	{
		XMVECTOR right = LoadAxis(matrix, 0);
		XMVECTOR up = LoadAxis(matrix, 1);
		XMVECTOR forward = LoadAxis(matrix, 2);
		if (yaw != 0.0f)
		{
			const XMMATRIX r = XMMatrixRotationAxis(up, yaw);
			right = XMVector3TransformNormal(right, r);
			forward = XMVector3TransformNormal(forward, r);
		}
		if (pitch != 0.0f)
		{
			const XMMATRIX r = XMMatrixRotationAxis(right, pitch);
			up = XMVector3TransformNormal(up, r);
			forward = XMVector3TransformNormal(forward, r);
		}
		if (roll != 0.0f)
		{
			const XMMATRIX r = XMMatrixRotationAxis(forward, roll);
			right = XMVector3TransformNormal(right, r);
			up = XMVector3TransformNormal(up, r);
		}
		StoreAxis(matrix, 0, right);
		StoreAxis(matrix, 1, up);
		StoreAxis(matrix, 2, forward);
		Orthonormalize(matrix);
	}

	// Interactive camera rotation: yaw is around AW2's stable world-up axis (+Y),
	// so normal yaw/pitch movement cannot accumulate unintended roll.
	static void RotateControlled(XMFLOAT4X4A& matrix, float pitch, float yaw, float roll)
	{
		XMVECTOR right = LoadAxis(matrix, 0);
		XMVECTOR up = LoadAxis(matrix, 1);
		XMVECTOR forward = LoadAxis(matrix, 2);
		const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		if (yaw != 0.0f)
		{
			const XMMATRIX r = XMMatrixRotationAxis(worldUp, yaw);
			right = XMVector3TransformNormal(right, r);
			up = XMVector3TransformNormal(up, r);
			forward = XMVector3TransformNormal(forward, r);
		}
		if (pitch != 0.0f)
		{
			right = XMVector3Normalize(right);
			const XMMATRIX r = XMMatrixRotationAxis(right, pitch);
			up = XMVector3TransformNormal(up, r);
			forward = XMVector3TransformNormal(forward, r);
		}
		if (roll != 0.0f)
		{
			forward = XMVector3Normalize(forward);
			const XMMATRIX r = XMMatrixRotationAxis(forward, roll);
			right = XMVector3TransformNormal(right, r);
			up = XMVector3TransformNormal(up, r);
		}

		StoreAxis(matrix, 0, right);
		StoreAxis(matrix, 1, up);
		StoreAxis(matrix, 2, forward);
		Orthonormalize(matrix);
	}

	static void ResetRoll(XMFLOAT4X4A& matrix)
	{
		const XMVECTOR forward = LoadAxis(matrix, 2);
		const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const float alignment = std::abs(XMVectorGetX(XMVector3Dot(forward, worldUp)));
		if (alignment > 0.9995f) return;

		const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
		const XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));
		StoreAxis(matrix, 0, right);
		StoreAxis(matrix, 1, up);
		StoreAxis(matrix, 2, forward);
	}

	static void TranslateCamera(XMFLOAT4X4A& matrix, float rightAmount, float upAmount, float forwardAmount)
	{
		const XMVECTOR right = LoadAxis(matrix, 0);
		const XMVECTOR up = LoadAxis(matrix, 1);
		const XMVECTOR forward = LoadAxis(matrix, 2);
		XMVECTOR delta = XMVectorZero();
		delta = XMVectorAdd(delta, XMVectorScale(right, rightAmount));
		delta = XMVectorAdd(delta, XMVectorScale(up, upAmount));
		delta = XMVectorAdd(delta, XMVectorScale(forward, forwardAmount));
		XMFLOAT3 d;
		XMStoreFloat3(&d, delta);
		matrix.m[3][0] += d.x;
		matrix.m[3][1] += d.y;
		matrix.m[3][2] += d.z;
	}

	export void MoveCameraRelative(XMFLOAT4X4A& matrix, float right, float up, float forward)
	{
		TranslateCamera(matrix, right, up, forward);
	}

	export void RotateCameraLocal(XMFLOAT4X4A& matrix, float pitch, float yaw, float roll)
	{
		RotateAroundCurrentAxes(matrix, pitch, yaw, roll);
	}

	export void UpdateHotkeys()
	{
		if (!IsForegroundProcess())
		{
			s_PreviousInsertDown = false;
			s_PreviousLogDown = false;
			s_PreviousPauseDown = false;
			s_PreviousHudDown = false;
			return;
		}
		const bool insertDown = IsKeyDown(VK_INSERT);
		if (insertDown && !s_PreviousInsertDown) s_ToggleCameraRequested = true;
		s_PreviousInsertDown = insertDown;
		const bool logDown = IsKeyDown(VK_F12);
		if (logDown && !s_PreviousLogDown) s_LogBasisRequested = true;
		s_PreviousLogDown = logDown;
		const bool pauseDown = IsKeyDown(VK_NUMPAD0);
		if (pauseDown && !s_PreviousPauseDown) TogglePause();
		s_PreviousPauseDown = pauseDown;
		const bool hudDown = IsKeyDown(VK_DELETE);
		if (hudDown && !s_PreviousHudDown) ToggleHud();
		s_PreviousHudDown = hudDown;
	}

	export bool ConsumeToggleCameraRequest()
	{
		const bool requested = s_ToggleCameraRequested;
		s_ToggleCameraRequested = false;
		return requested;
	}

	export bool ConsumeLogBasisRequest()
	{
		const bool requested = s_LogBasisRequested;
		s_LogBasisRequested = false;
		return requested;
	}

	export void UpdateCamera(XMFLOAT4X4A& matrix, float& fov, double dt, float movementSpeed, float rotationSpeed)
	{
		if (!IsForegroundProcess()) return;
		const float frameScale = (float)std::clamp(dt * 60.0, 0.0, 4.0);
		float speedMultiplier = 1.0f;
		if (IsKeyDown(VK_SHIFT)) speedMultiplier *= s_Settings.FastMultiplier;
		if (IsKeyDown(VK_CONTROL)) speedMultiplier *= s_Settings.SlowMultiplier;
		if (IsKeyDown(VK_MENU)) speedMultiplier *= s_Settings.VerySlowMultiplier;
		float moveRight = 0.0f, moveUp = 0.0f, moveForward = 0.0f;
		float pitch = 0.0f, yaw = 0.0f, roll = 0.0f, fovDelta = 0.0f;
		bool resetRoll = false;

		if (s_Settings.KeyboardEnabled)
		{
			if (IsKeyDown(VK_NUMPAD8)) moveForward += 1.0f;
			if (IsKeyDown(VK_NUMPAD5)) moveForward -= 1.0f;
			if (IsKeyDown(VK_NUMPAD6)) moveRight += 1.0f;
			if (IsKeyDown(VK_NUMPAD4)) moveRight -= 1.0f;
			if (IsKeyDown(VK_NUMPAD9)) moveUp += 1.0f;
			if (IsKeyDown(VK_NUMPAD7)) moveUp -= 1.0f;
			if (IsKeyDown(VK_UP)) pitch -= 1.0f;
			if (IsKeyDown(VK_DOWN)) pitch += 1.0f;
			if (IsKeyDown(VK_RIGHT)) yaw += 1.0f;
			if (IsKeyDown(VK_LEFT)) yaw -= 1.0f;
			if (IsKeyDown(VK_NUMPAD3)) roll += 1.0f;
			if (IsKeyDown(VK_NUMPAD1)) roll -= 1.0f;
			if (IsKeyDown(VK_NUMPAD2)) resetRoll = true;
			if (IsKeyDown(VK_ADD)) fovDelta += 1.0f;
			if (IsKeyDown(VK_SUBTRACT)) fovDelta -= 1.0f;
		}

		if (s_Settings.ControllerEnabled)
		{
			XINPUT_STATE state{};
			for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index)
			{
				if (XInputGetState(index, &state) != ERROR_SUCCESS) continue;
				const auto& pad = state.Gamepad;
				moveRight += ApplyDeadzone(pad.sThumbLX, s_Settings.ControllerDeadzone);
				moveForward += ApplyDeadzone(pad.sThumbLY, s_Settings.ControllerDeadzone);
				yaw += ApplyDeadzone(pad.sThumbRX, s_Settings.ControllerDeadzone) * s_Settings.ControllerRotationScale;
				pitch -= ApplyDeadzone(pad.sThumbRY, s_Settings.ControllerDeadzone) * s_Settings.ControllerRotationScale;
				moveUp += TriggerValue(pad.bRightTrigger) - TriggerValue(pad.bLeftTrigger);
				if (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) roll += 1.0f;
				if (pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) roll -= 1.0f;
				if (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) fovDelta += 1.0f;
				if (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) fovDelta -= 1.0f;
				if (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) speedMultiplier *= s_Settings.SlowMultiplier;
				if (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) speedMultiplier *= s_Settings.FastMultiplier;
				break;
			}
		}

		const float translationStep = movementSpeed * frameScale * speedMultiplier;
		TranslateCamera(matrix, moveRight * translationStep, moveUp * translationStep, moveForward * translationStep);
		const float rotationStep = rotationSpeed * frameScale;
		RotateControlled(matrix, pitch * rotationStep, yaw * rotationStep, roll * rotationStep);
		if (resetRoll) ResetRoll(matrix);
		fov = std::clamp(fov + fovDelta * s_Settings.FovSpeed * (float)dt, 1.0f, 179.0f);
	}

	export void DrawCameraInputUI()
	{
		ImGui::Checkbox("Keyboard camera control", &s_Settings.KeyboardEnabled);
		ImGui::Checkbox("Controller camera control", &s_Settings.ControllerEnabled);
		ImGui::DragFloat("Fast multiplier", &s_Settings.FastMultiplier, 0.05f, 1.0f, 20.0f);
		ImGui::DragFloat("Slow multiplier", &s_Settings.SlowMultiplier, 0.01f, 0.01f, 1.0f);
		ImGui::DragFloat("Very slow multiplier", &s_Settings.VerySlowMultiplier, 0.005f, 0.005f, 0.5f);
		ImGui::DragFloat("Controller deadzone", &s_Settings.ControllerDeadzone, 0.01f, 0.0f, 0.95f);
		ImGui::DragFloat("Controller rotation scale", &s_Settings.ControllerRotationScale, 0.05f, 0.1f, 5.0f);
		ImGui::DragFloat("FOV speed", &s_Settings.FovSpeed, 0.5f, 1.0f, 180.0f);
		ImGui::TextUnformatted("Insert: camera | NP0: pause | Delete: HUD | NP2: reset roll | F12: basis log");
	}

	export void GetBasis(const XMFLOAT4X4A& matrix, XMFLOAT3& right, XMFLOAT3& up, XMFLOAT3& forward)
	{
		XMStoreFloat3(&right, LoadAxis(matrix, 0));
		XMStoreFloat3(&up, LoadAxis(matrix, 1));
		XMStoreFloat3(&forward, LoadAxis(matrix, 2));
	}
}
