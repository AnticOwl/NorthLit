#include <DirectXMath.h>
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using namespace DirectX;

export module Animation;

import HookUtil;
import Log;
import Northlight;
import Offsets;
import std;

namespace NorthLit::Animation
{
	struct BoneTransform
	{
		XMFLOAT4 qRotation;
		XMFLOAT4 vPosition;
	};

	struct Joint
	{
		std::string Name;
		unsigned int Index;
		bool Override;
	};

	struct CapturedFrame
	{
		std::string Name;
		std::vector<BoneTransform> Transforms;
	};

	typedef std::pair<std::string, unsigned int> BoneNameIndexPair;

	static CapturedFrame s_CapturedFrame;
	static std::vector<CapturedFrame> s_CapturedFrames;

	static std::shared_mutex s_JointMutex;
	static std::vector<Joint> s_Joints;
	static std::unordered_map<unsigned int, unsigned int> s_AnimationIndexToJointIndex;

	static bool s_CaptureFrameRequested = false;
	static bool s_OverrideAnimation = false;
	static bool s_RotateEyes = false;

	static unsigned int s_LeftEyeIndex = 0;
	static unsigned int s_RightEyeIndex = 0;
	static float s_EyeRotationPitch = 0.0f;
	static float s_EyeRotationYaw = 0.0f;

	static __int64 s_CurrentAvatar = -1;

	typedef __int64(__fastcall* tAnimationMixerPreUpdate)(__int64, uint16_t);
	static tAnimationMixerPreUpdate oAnimationMixerPreUpdate = nullptr;

	__int64 __fastcall hAnimationMixerPreUpdate(__int64 a1, uint16_t a2)
	{
		__int64 result = oAnimationMixerPreUpdate(a1, a2);

		if ((!s_CaptureFrameRequested && !s_OverrideAnimation && !s_RotateEyes) || s_CurrentAvatar == -1)
		{
			return result;
		}

		uint16_t avatarEcsHandle = Northlight::GetEcsHandle(s_CurrentAvatar) & 0xFFFF;
		if (avatarEcsHandle != a2)
			return result;

		// coregame::component::AnimationMixer
		static unsigned int AnimationMixerId = Northlight::ecs::GetEcsId("coregame::component::AnimationMixer>(void)", "ecs::core::ComponentContext>(void)");
		__int64* ppAvatarAnimationMixer = (__int64*)Northlight::ecs::GetComponent(s_CurrentAvatar, AnimationMixerId, 0x10);
		if (ppAvatarAnimationMixer == nullptr || *ppAvatarAnimationMixer == 0)
		{
			return result;
		}

		__int64 pAnimationMixer = *ppAvatarAnimationMixer;

		BoneTransform* pBoneTransforms = *(BoneTransform**)(pAnimationMixer + 0x90);
		BoneTransform* pBoneTransforms2 = *(BoneTransform**)(pAnimationMixer + 0xA0);

		unsigned int boneCount = *(unsigned int*)(pAnimationMixer + 0x10);

		if (s_CaptureFrameRequested)
		{
			s_CapturedFrame.Transforms.clear();
			s_CapturedFrame.Transforms.resize(boneCount);
			for (unsigned int i = 0; i < boneCount; ++i)
			{
				//unsigned int boneIndex = s_Bones[i].second;
				s_CapturedFrame.Transforms[i] = pBoneTransforms[i];
			}

			s_CaptureFrameRequested = false;
			s_OverrideAnimation = true;;
		}

		if (s_OverrideAnimation)
		{
			std::shared_lock lock(s_JointMutex);
			for (unsigned int i = 0; i < boneCount; ++i)
			{
				auto jointIndex = s_AnimationIndexToJointIndex.find(i);
				if (jointIndex == s_AnimationIndexToJointIndex.end() || s_Joints[jointIndex->second].Override)
				{
					pBoneTransforms[i] = s_CapturedFrame.Transforms[i];
				}
			}
		}

		if (s_RotateEyes)
		{
			XMVECTOR qEyeRotation = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(s_EyeRotationPitch), XMConvertToRadians(s_EyeRotationYaw), 0);

			XMStoreFloat4(&pBoneTransforms[s_LeftEyeIndex].qRotation, qEyeRotation);
			XMStoreFloat4(&pBoneTransforms[s_RightEyeIndex].qRotation, qEyeRotation);
		}

		return result;
	}

	void UpdateAvatarHandle()
	{
		__int64 avatarHandle = Northlight::GetCurrentAvatarEntityHandle();

		if (avatarHandle != s_CurrentAvatar && avatarHandle != -1)
		{
			std::unique_lock lock(s_JointMutex);

			s_Joints.clear();
			s_AnimationIndexToJointIndex.clear();
			s_OverrideAnimation = false;

			// coregame::component::MeshResource
			static unsigned int MeshResourceComponentId = Northlight::ecs::GetEcsId("coregame::component::MeshResource>(void)", "ecs::core::ComponentContext>(void)");
			__int64* ppMeshResource = (__int64*)Northlight::ecs::GetComponent(avatarHandle, MeshResourceComponentId, 0x8);
			// coregame::component::MeshBoneMapping>(void)
			static unsigned int MeshboneMappingComponentId = Northlight::ecs::GetEcsId("coregame::component::MeshBoneMapping>(void)", "ecs::core::ComponentContext>(void)");
			unsigned int** ppBoneMappings = (unsigned int**)Northlight::ecs::GetComponent(avatarHandle, MeshboneMappingComponentId, 0x8);

			if (ppMeshResource && *ppMeshResource && ppBoneMappings && *ppBoneMappings)
			{
				Log::Write("[Animation] Updating joints due to avatar change (Previous ID %I64X - Current ID %I64X", s_CurrentAvatar, avatarHandle);

				__int64 pMeshResource = *ppMeshResource;
				__int64 pRendMesh = *(__int64*)(pMeshResource + 0xC0);

				struct BoneInfo
				{
					const char Name[0x40];
				};

				const unsigned int boneCount = *(unsigned int*)(pRendMesh + 0x188);
				BoneInfo* boneNames = *(BoneInfo**)(pRendMesh + 0x180);

				unsigned int* pBoneMappings = *ppBoneMappings;

				for (unsigned int i = 0; i < boneCount; ++i)
				{
					std::string boneName = boneNames[i].Name;

					s_Joints.emplace_back(boneName, pBoneMappings[i], true);
					s_AnimationIndexToJointIndex.emplace(pBoneMappings[i], i);

					if (boneName.contains("L_eye_JNT"))
					{
						s_LeftEyeIndex = pBoneMappings[i];
						Log::Write("Left eye index %d", s_LeftEyeIndex);
					}
					else if (boneName.contains("R_eye_JNT"))
					{
						s_RightEyeIndex = pBoneMappings[i];
						Log::Write("Right eye index %d", s_RightEyeIndex);
					}
				}

				s_CurrentAvatar = avatarHandle;
			}
		}
	}

	export void Initialize()
	{
		CreateHook((void*)GetOffset(Offset::AnimationMixerPreUpdate), hAnimationMixerPreUpdate, &oAnimationMixerPreUpdate);
	}

	export void DrawTab()
	{
		if (!ImGui::BeginTabItem("Animation"))
			return;

		ImGui::SeparatorText("Animation capture");

		if (ImGui::Button("Capture animation frame"))
		{
			s_CaptureFrameRequested = true;
		}

		const bool hasCapturedFrame = !s_CapturedFrame.Transforms.empty();
		ImGui::BeginDisabled(!hasCapturedFrame);

		ImGui::Checkbox("Use captured frame", &s_OverrideAnimation);

		ImGui::EndDisabled();

		ImGui::SeparatorText("Eyes");

		ImGui::Checkbox("Override eye rotation", &s_RotateEyes);
		ImGui::DragFloat("Pitch", &s_EyeRotationPitch, 0.1f);
		ImGui::DragFloat("Yaw", &s_EyeRotationYaw, 0.1f);

		ImGui::SeparatorText("Joints");

		static std::string s_BoneListFilter = "";
		ImGui::InputText("Filter", &s_BoneListFilter);

		if (ImGui::Button("Toggle filtered bones"))
		{
			static bool toggleStatus = false;
			toggleStatus = !toggleStatus;
			for (auto& joint : s_Joints)
			{
				if (joint.Name.contains(s_BoneListFilter) == false)
				{
					continue;
				}

				joint.Override = toggleStatus;
			}
		}

		ImGui::BeginChild("Joints", ImVec2(-FLT_MIN, -FLT_MIN));

		std::shared_lock lock(s_JointMutex);

		for (auto& joint : s_Joints)
		{
			if (joint.Name.contains(s_BoneListFilter) == false)
			{
				continue;
			}

			BoneTransform& jointTransform = s_CapturedFrame.Transforms[joint.Index];

			ImGui::PushID(joint.Name.c_str());
			if (ImGui::CollapsingHeader(joint.Name.c_str()))
			{
				ImGui::Checkbox("Override", &joint.Override);

				ImGui::BeginTable("Rotation", 3, 0, ImVec2(-FLT_MIN, 0));
				{
					bool dirty = false;

					XMVECTOR quat = XMLoadFloat4(&jointTransform.qRotation);
					float rotation[3] = { 0,0,0 };

					// Roll
					rotation[2] = atan2f(
						2 * (quat.m128_f32[3] * quat.m128_f32[2] + quat.m128_f32[0] * quat.m128_f32[1]),
						1 - 2 * (quat.m128_f32[1] * quat.m128_f32[1] + quat.m128_f32[2] * quat.m128_f32[2]));

					// Pitch
					rotation[0] = atan2f(
						2 * (quat.m128_f32[3] * quat.m128_f32[0] + quat.m128_f32[1] * quat.m128_f32[2]),
						1 - 2 * (quat.m128_f32[0] * quat.m128_f32[0] + quat.m128_f32[1] * quat.m128_f32[1]));

					// Yaw
					rotation[1] = asinf(2 * (quat.m128_f32[3] * quat.m128_f32[1] - quat.m128_f32[2] * quat.m128_f32[0]));

					rotation[0] = XMConvertToDegrees(rotation[0]);
					rotation[1] = XMConvertToDegrees(rotation[1]);
					rotation[2] = XMConvertToDegrees(rotation[2]);

					//float deltaRotation[3] = { rotation[0],rotation[1],rotation[2] };

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("Pitch");
					ImGui::TableNextColumn();
					ImGui::Text("Yaw");
					ImGui::TableNextColumn();
					ImGui::Text("Roll");

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					dirty |= ImGui::DragFloat("##Pitch", &rotation[0], 0.05f, 0, 0, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					dirty |= ImGui::DragFloat("##Yaw", &rotation[1], 0.05f, 0, 0, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					dirty |= ImGui::DragFloat("##Roll", &rotation[2], 0.05f, 0, 0, "%.1f", ImGuiSliderFlags_NoRoundToFormat);

					if (dirty)
					{
						/*
						deltaRotation[0] = deltaRotation[0] - rotation[0];
						deltaRotation[1] = deltaRotation[1] - rotation[1];
						deltaRotation[2] = deltaRotation[2] - rotation[2];

						XMVECTOR deltaYaw = XMQuaternionRotationRollPitchYaw(0, XMConvertToRadians(deltaRotation[1]), 0);
						quat = XMQuaternionMultiply(quat, deltaYaw);

						XMVECTOR deltaPitchRoll = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(deltaRotation[0]), 0, XMConvertToRadians(deltaRotation[2]));
						quat = XMQuaternionMultiply(deltaPitchRoll, quat);
						*/

						double roll = XMConvertToRadians(rotation[2]);
						double pitch = XMConvertToRadians(rotation[0]);
						double yaw = XMConvertToRadians(rotation[1]);

						double cy = cos(yaw * 0.5);
						double sy = sin(yaw * 0.5);
						double cp = cos(pitch * 0.5);
						double sp = sin(pitch * 0.5);
						double cr = cos(roll * 0.5);
						double sr = sin(roll * 0.5);

						quat.m128_f32[0] = float(cy * sp * cr - sy * cp * sr); // qx
						quat.m128_f32[1] = float(sy * cp * cr + cy * sp * sr); // qy
						quat.m128_f32[2] = float(cy * cp * sr - sy * sp * cr); // qz
						quat.m128_f32[3] = float(cy * cp * cr + sy * sp * sr); // qw

						XMStoreFloat4(&jointTransform.qRotation, quat);
					}

					ImGui::EndTable();
				}

				ImGui::BeginTable("Position", 3, 0, ImVec2(-FLT_MIN, 0));
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("X");
					ImGui::TableNextColumn();
					ImGui::Text("Y");
					ImGui::TableNextColumn();
					ImGui::Text("Z");

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::DragFloat("##X", &jointTransform.vPosition.x, 0.005f, 0, 0, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::DragFloat("##Y", &jointTransform.vPosition.y, 0.005f, 0, 0, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
					ImGui::TableNextColumn();
					ImGui::SetNextItemWidth(-FLT_MIN);
					ImGui::DragFloat("##Z", &jointTransform.vPosition.z, 0.005f, 0, 0, "%.1f", ImGuiSliderFlags_NoRoundToFormat);

					ImGui::EndTable();
				}
			}
			ImGui::PopID();
		}

		ImGui::EndChild();

		ImGui::EndTabItem();
	}

	export void Update()
	{
		UpdateAvatarHandle();

		if (GetAsyncKeyState(VK_F6) & 0x8000)
		{
			s_CaptureFrameRequested = true;

			while (GetAsyncKeyState(VK_F6) & 0x8000)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}
}