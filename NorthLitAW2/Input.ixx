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
	static bool s_ToggleCameraRequested = false;
	static bool s_LogBasisRequested = false;

	static bool IsForegroundProcess()
	{
		HWND foreground = GetForegroundWindow();
		if (!foreground) return false;
		DWORD pid = 0;
		GetWindowThreadProcessId(foreground, &pid);
		return pid == GetCurrentProcessId();
	}

	static bool IsKeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

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
			return;
		}
		const bool insertDown = IsKeyDown(VK_INSERT);
		if (insertDown && !s_PreviousInsertDown) s_ToggleCameraRequested = true;
		s_PreviousInsertDown = insertDown;
		const bool logDown = IsKeyDown(VK_F12);
		if (logDown && !s_PreviousLogDown) s_LogBasisRequested = true;
		s_PreviousLogDown = logDown;
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

		if (s_Settings.KeyboardEnabled)
		{
			if (IsKeyDown(VK_NUMPAD8)) moveForward += 1.0f;
			if (IsKeyDown(VK_NUMPAD5)) moveForward -= 1.0f;
			if (IsKeyDown(VK_NUMPAD6)) moveRight += 1.0f;
			if (IsKeyDown(VK_NUMPAD4)) moveRight -= 1.0f;
			if (IsKeyDown(VK_NUMPAD9)) moveUp += 1.0f;
			if (IsKeyDown(VK_NUMPAD7)) moveUp -= 1.0f;
			if (IsKeyDown(VK_UP)) pitch += 1.0f;
			if (IsKeyDown(VK_DOWN)) pitch -= 1.0f;
			if (IsKeyDown(VK_RIGHT)) yaw += 1.0f;
			if (IsKeyDown(VK_LEFT)) yaw -= 1.0f;
			if (IsKeyDown(VK_NUMPAD3)) roll += 1.0f;
			if (IsKeyDown(VK_NUMPAD1)) roll -= 1.0f;
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
				pitch += ApplyDeadzone(pad.sThumbRY, s_Settings.ControllerDeadzone) * s_Settings.ControllerRotationScale;
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
		RotateAroundCurrentAxes(matrix, pitch * rotationStep, yaw * rotationStep, roll * rotationStep);
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
		ImGui::TextUnformatted("Insert: toggle camera | F12: log camera basis");
	}

	export void GetBasis(const XMFLOAT4X4A& matrix, XMFLOAT3& right, XMFLOAT3& up, XMFLOAT3& forward)
	{
		XMStoreFloat3(&right, LoadAxis(matrix, 0));
		XMStoreFloat3(&up, LoadAxis(matrix, 1));
		XMStoreFloat3(&forward, LoadAxis(matrix, 2));
	}
}
