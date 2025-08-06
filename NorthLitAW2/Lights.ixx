#include <DirectXMath.h>
#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"

using namespace DirectX;

export module Lights;

import Log;
import Northlight;
import Offsets;
import std;

namespace NorthLit::Lights
{
	struct Light
	{
		Northlight::content::EntityArchetype* Archetype;
		Northlight::r::GlobalID ArchetypeGID{ 0 };
		Northlight::r::GlobalID EntityGID{ 0 };
		__int64 EntityHandle{ -1 };

		std::string Name;
		bool ShowEditor = true;
		bool Delete = false;
		bool Enabled = true;
	};

	static Northlight::r::GlobalID s_ArchetypeGID{ 0 };
	static Northlight::content::EntityArchetype* s_Archetype = nullptr;
	static Northlight::content::SpotLightComponent* s_ArchetypeSpotlight = nullptr;
	static Northlight::content::TransformComponent* s_ArchetypeTransform = nullptr;

	static std::vector<Light> s_Lights;
	static int s_NextLightId = 1;

	static std::vector<std::pair<Northlight::r::GlobalID, std::string>> s_LightTextures;
	static std::unordered_map<__int64, std::string> s_LightTextureLookup;

	namespace ProjectionMapPopupContext
	{
		static int s_ProjectionMapIndex = 0;
		static Northlight::content::SpotLightComponent* s_LightTexturePopupComponent = nullptr;
	}

	const char* Items_ArrayGetter(void* data, int idx)
	{
		return s_LightTextures[idx].second.c_str();
	}

	void UpdateLightTextureList()
	{
		s_LightTextures.clear();

		__int64 pResourceMap = Northlight::coregame::ResourceManager::GetInstance()->m_ResourceMap;

		struct ResourceEntry
		{
			__int64 Unk;
			__int64 pFileInfoMetadata;
			__int64 pResourceMetadata;
			__int64 pLuaScriptMetadata;
			__int64 pRingBufferMemoryResource;
			const char* FilePath;
			__int64 SzFilePath;
			__int64 Unk2;
		};

		struct MapEntry
		{
			Northlight::r::GlobalID GID;
			ResourceEntry* pEntry;
		};

		MapEntry* entries = *(MapEntry**)(pResourceMap + 0x68);
		const int mapCapacity = *(int*)(pResourceMap + 0x50);

		for (int i = 0; i < mapCapacity; ++i)
		{
			MapEntry& mapEntry = entries[i];
			if (mapEntry.pEntry == nullptr) continue;

			ResourceEntry& entry = *mapEntry.pEntry;
			const std::string filePath = std::string(entry.FilePath, (size_t)entry.SzFilePath);
			const std::string extension = filePath.substr(filePath.find_last_of('.'));

			const size_t findResult = filePath.find("data_pc/textures/lights");

			if (findResult != std::string::npos && extension == ".tex")
			{
				const std::string lightTexturePath = filePath.substr(findResult + 24);
				s_LightTextures.emplace_back(mapEntry.GID, lightTexturePath);

				auto itr = s_LightTextureLookup.find(mapEntry.GID.GID);
				if (itr == s_LightTextureLookup.end())
				{
					s_LightTextureLookup.emplace(mapEntry.GID.GID, lightTexturePath);
				}
			}
		}

		std::sort(s_LightTextures.begin(), s_LightTextures.end(), [](const auto& a, const auto& b)
			{
				return std::lexicographical_compare(
				a.second.begin(), a.second.end(),
				b.second.begin(), b.second.end(),
				[](char c1, char c2) 
				{
					return std::tolower(c1) < std::tolower(c2);
				});
			});
	}

	std::pair<Northlight::r::GlobalID, Northlight::content::EntityArchetype*> CopyAndRegisterArchetype(const std::string& lightName)
	{
		const std::string archetypeName = lightName + "Archetype";
		auto GID = Northlight::r::GlobalID(77, archetypeName.c_str(), 0);

		auto newArchetype = Northlight::r::ClassFactory::ConstructType<Northlight::content::EntityArchetype>("content::EntityArchetype");

		newArchetype->SetName(archetypeName.c_str());
		newArchetype->m_Entity.SetName(lightName.c_str());

		const std::string spotlightComponentName = lightName + "Spotlight";
		const std::string transformComponentName = lightName + "Transform";

		newArchetype->m_Components = new Northlight::content::GenericComponent * [2] {nullptr, nullptr};
		newArchetype->m_ComponentCount = 2;
		newArchetype->m_ReservedCount = 2;

		newArchetype->m_Components[0] = Northlight::r::ClassFactory::ConstructType<Northlight::content::SpotLightComponent>("content::SpotLightComponent");
		newArchetype->m_Components[1] = Northlight::r::ClassFactory::ConstructType<Northlight::content::TransformComponent>("content::TransformComponent");

		memcpy(newArchetype->m_Components[0], s_ArchetypeSpotlight, sizeof(Northlight::content::SpotLightComponent));
		memcpy(newArchetype->m_Components[1], s_ArchetypeTransform, sizeof(Northlight::content::TransformComponent));

		newArchetype->m_Entity.m_Components = new Northlight::content::ComponentRef[2]();
		newArchetype->m_Entity.m_Components[0].GID.set(0xE3, spotlightComponentName.c_str(), 0);
		newArchetype->m_Entity.m_Components[0].Component = nullptr;
		newArchetype->m_Entity.m_Components[0].Flags = 0;
		newArchetype->m_Entity.m_Components[1].GID.set(0xE3, transformComponentName.c_str(), 0);
		newArchetype->m_Entity.m_Components[1].Component = nullptr;
		newArchetype->m_Entity.m_Components[1].Flags = 0;

		auto globalIdMap = Northlight::r::GlobalIDMap::GetInstance();
		globalIdMap->RegisterID(GID, newArchetype);

		return { GID, newArchetype };
	}

	std::pair<float, float> GetLightPitchYaw(const XMVECTOR& q)
	{
		XMMATRIX m = XMMatrixRotationQuaternion(q);

		float pitch = 0, yaw = 0;

		pitch = atanf(m.r[2].m128_f32[1] / sqrtf(pow(m.r[2].m128_f32[0], 2) + pow(m.r[2].m128_f32[2], 2)));
		if (fabsf(m.r[2].m128_f32[1]) != 1.0f)
		{
			yaw = atan2f(m.r[2].m128_f32[0], m.r[2].m128_f32[1]);
		}

		//return { pitch, yaw, roll }; // Return a tuple of pitch, yaw, and roll

		return { pitch, yaw };
	}

	void DrawArchetypeEditor(Northlight::content::SpotLightComponent* spotlightComponent, bool StaticPropertiesOnly = false)
	{
		if (!StaticPropertiesOnly)
		{
			float currentColor[3] = { ((spotlightComponent->m_LightColor & 0xFF0000) >> 16) / 255.0f,
					((spotlightComponent->m_LightColor & 0x00FF00) >> 8) / 255.0f,
					((spotlightComponent->m_LightColor & 0x0000FF) >> 0) / 255.0f };

			if (ImGui::ColorEdit3("Color", currentColor))
			{
				spotlightComponent->m_LightColor = (0xFF << 24)
					| ((unsigned int)(currentColor[0] * 255.0f) << 16)
					| ((unsigned int)(currentColor[1] * 255.0f) << 8)
					| ((unsigned int)(currentColor[2] * 255.0f) << 0);
			}
		}

		if (!StaticPropertiesOnly)
		{
			ImGui::DragFloat("Intensity", &spotlightComponent->m_fIntensity, 5.0f, 0.0f, FLT_MAX, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Field of view", &spotlightComponent->m_fFov, 0.1f, 0.0f, 179.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Area light radius", &spotlightComponent->m_fRadius, 0.005f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		}
		ImGui::SliderFloat("Aspect ratio", &spotlightComponent->m_fConeShape, 0, 2);
		ImGui::SliderFloat("Directionality", &spotlightComponent->m_fDirectionality, 0, 1);
		if (!StaticPropertiesOnly)
		{
			ImGui::DragFloat("Near plane", &spotlightComponent->m_fNear, 0.1f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloat("Far plane", &spotlightComponent->m_fFar, 0.1f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		}

		static const char* sMappingType[2] = { "Perspective", "Stereographic" };
		ImGui::Combo("Projection type", (int*)&spotlightComponent->m_eProjectionType, sMappingType, 2);

		ImGui::Checkbox("Bent Normals", &spotlightComponent->m_bBentNormals);
		ImGui::Checkbox("Enable direct lighting", &spotlightComponent->m_bEnableDirect);
		ImGui::Checkbox("Enable far clipping", &spotlightComponent->m_bEnableFarClip);

		ImGui::NewLine();

		ImGui::Checkbox("Enable volumetric lighting", &spotlightComponent->m_bEnableVolumetric);
		ImGui::DragFloat("Scattered light multiplier", &spotlightComponent->m_fScatterIntensityMultiplier, 0.005f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Volumetric effect multiplier", &spotlightComponent->m_fVolumetricIntensityMultiplier, 0.005f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Scattering lobe #1", &spotlightComponent->m_fVolumetricLightPhase1, 0.005f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Scattering lobe #2", &spotlightComponent->m_fVolumetricLightPhase2, 0.005f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Scattering lobe blend", &spotlightComponent->m_fVolumetricLightPhaseBlend, 0.005f, 0, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp);

		static const char* sVolumetricQuality[3] = { "Low", "Medium", "High" };
		ImGui::Combo("Volumetric quality", (int*)&spotlightComponent->m_eVolumetricQuality, sVolumetricQuality, 3);

		static const char* sVolumetricBoundingShape[2] = { "Cone", "Square Frustum" };
		ImGui::Combo("Volumetric shape", (int*)&spotlightComponent->m_eVolumetricBoundingShape, sVolumetricBoundingShape, 2);

		ImGui::NewLine();

		ImGui::DragFloat("Indirect intensity multiplier", &spotlightComponent->m_fIndirectIntensityMultiplier, 0.005f, 0, FLT_MAX);
		ImGui::DragFloat("Direct intensity multiplier", &spotlightComponent->m_fDirectIntensityMultiplier, 0.005f, 0, FLT_MAX);
		ImGui::DragFloat("Specular intensity multiplier", &spotlightComponent->m_fSpecularIntensityMultiplier, 0.005f, 0, FLT_MAX);

		ImGui::Checkbox("Enable direct", &spotlightComponent->m_bEnableDirect);

		ImGui::NewLine();

		ImGui::SliderFloat("Depth bias", &spotlightComponent->m_fDepthBias, 0, 0.01f);
		ImGui::SliderFloat("Depth slope bias", &spotlightComponent->m_fDepthSlopeBias, 0.1f, 16.0f);
		ImGui::SliderFloat("Shadow filter size", &spotlightComponent->m_fShadowFilterSize, 0, 10);
		ImGui::DragFloat("Max dynamic shadow distance", &spotlightComponent->m_fMaxDynamicShadowDistance, 0.1f, 0, FLT_MAX);

		static const char* sDynamicShadowType[3] = { "Off", "PCF", "VSM" };
		ImGui::Combo("Shadowmap type", (int*)&spotlightComponent->m_eDynamicShadowType, sDynamicShadowType, 3);

		static const char* sShadowMapSize[4] = { "128", "256", "512", "1024" };
		ImGui::Combo("Shadowmap size", (int*)&spotlightComponent->m_eShadowMapSize, sShadowMapSize, 4);

		ImGui::NewLine();

		if (ImGui::Button("Select projection map"))
		{
			UpdateLightTextureList();
			ProjectionMapPopupContext::s_LightTexturePopupComponent = spotlightComponent;
			ImGui::OpenPopup("Select projection map");
		}

		auto findResult = s_LightTextureLookup.find(spotlightComponent->m_ResourceProjectionMap);

		ImGui::Text("Projection map: %s", findResult != s_LightTextureLookup.end() ? findResult->second.c_str() : "None");
		ImGui::DragFloat("Projection map Mipmap level", &spotlightComponent->m_fProjectionMapMipmapLevel, 0.01f);
		ImGui::DragFloat2("Projection map scale", spotlightComponent->m_vProjectionMapScale, 0.01f);

		if (ImGui::BeginPopupModal("Select projection map", 0, ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::SetWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::ListBox("##Textures", &ProjectionMapPopupContext::s_ProjectionMapIndex, Items_ArrayGetter, &s_LightTextures, (int)s_LightTextures.size(), 20);

			if (ImGui::Button("Select"))
			{
				ProjectionMapPopupContext::s_LightTexturePopupComponent->m_ResourceProjectionMap = s_LightTextures[ProjectionMapPopupContext::s_ProjectionMapIndex].first.GID;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	export void Initialize()
	{
		s_ArchetypeSpotlight = Northlight::r::ClassFactory::ConstructType<Northlight::content::SpotLightComponent>("content::SpotLightComponent");
		s_ArchetypeTransform = Northlight::r::ClassFactory::ConstructType<Northlight::content::TransformComponent>("content::TransformComponent");

		s_ArchetypeSpotlight->m_fIntensity = 1000.0f;
		s_ArchetypeSpotlight->m_bEnableVolumetric = true;
		s_ArchetypeSpotlight->m_eDynamicShadowType = Northlight::content::SpotLightComponent::DynamicShadowType::PCF;
		s_ArchetypeSpotlight->m_eShadowMapSize = Northlight::content::SpotLightComponent::DynamicShadowMapSize::s1024;
		s_ArchetypeSpotlight->m_eVolumetricQuality = Northlight::content::SpotLightComponent::VolumetricQuality::High;

		// lights/default.tex
		s_ArchetypeSpotlight->m_ResourceProjectionMap = 0x9389DAB71C850837;

		UpdateLightTextureList();
	}

	export void DrawLightEditors()
	{
		for (auto& light : s_Lights)
		{
			if (!light.ShowEditor)
			{
				continue;
			}

			ImGui::PushID((const void*)light.ArchetypeGID.GID);

			ImGui::SetNextWindowSize(ImVec2(250, 980), ImGuiCond_FirstUseEver);

			const std::string windowName = light.Name + "###" + std::to_string(light.ArchetypeGID.GID);
			if (ImGui::Begin(windowName.c_str(), &light.ShowEditor, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize))
			{
				if (light.EntityHandle == -1)
				{
					light.EntityHandle = Northlight::GetEntityHandle(light.EntityGID);
					if (light.EntityHandle == -1)
					{
						ImGui::Text("Waiting for entity handle...");
						continue;
					}
				}

				static const unsigned int lightComponentId = Northlight::ecs::GetEcsId("coregame::component::Light>(void)", "ecs::core::ComponentContext>(void)");
				static const unsigned int spotlightComponentId = Northlight::ecs::GetEcsId("coregame::component::SpotLight>(void)", "ecs::core::ComponentContext>(void)");
				static const unsigned int worldTransformComponentId = Northlight::ecs::GetEcsId("coregame::component::WorldTransformReadOnly>(void)", "ecs::core::ComponentContext>(void)");
				static const unsigned int previousWorldTransformComponentId = Northlight::ecs::GetEcsId("coregame::component::PreviousWorldTransformReadOnly>(void)", "ecs::core::ComponentContext>(void)");


				auto lightComponent = Northlight::ecs::GetComponent<Northlight::ecs::LightComponent>(light.EntityHandle, lightComponentId);
				auto spotlightComponent = Northlight::ecs::GetComponent<Northlight::ecs::SpotlightComponent>(light.EntityHandle, spotlightComponentId);
				auto transformComponent1 = Northlight::ecs::GetComponent<Northlight::ecs::TransformComponent>(light.EntityHandle, previousWorldTransformComponentId);
				auto transformComponent2 = Northlight::ecs::GetComponent<Northlight::ecs::TransformComponent>(light.EntityHandle, worldTransformComponentId);

				if (!lightComponent || !spotlightComponent || !transformComponent1 || !transformComponent2)
				{
					ImGui::Text("Waiting for ECS components...");
					continue;
				}

				ImGui::PushItemWidth(200.0f);

				ImGui::InputText("Name", &light.Name);

				ImGui::ColorEdit3("Color", &lightComponent->Color.x);

				ImGui::DragFloat("Intensity", &lightComponent->Intensity, 5.0f, 0.0f, FLT_MAX, "%.1f", ImGuiSliderFlags_AlwaysClamp);

				float fovDegrees = XMConvertToDegrees(spotlightComponent->FieldOfView);
				if (ImGui::DragFloat("Field of view", &fovDegrees, 0.1f, 0.0f, 179.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp))
				{
					spotlightComponent->FieldOfView = XMConvertToRadians(fovDegrees);
				}

				ImGui::DragFloat("Area light radius", &spotlightComponent->Radius, 0.005f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat("Near plane", &spotlightComponent->NearPlane, 0.1f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
				ImGui::DragFloat("Far plane", &spotlightComponent->FarPlane, 0.1f, 0, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

				ImGui::NewLine();

				ImGui::SeparatorText("Transform");

				XMVECTOR qVec = XMLoadFloat4(&transformComponent1->Rotation);

				float angles[2] = { 0, 0 };

				if (ImGui::DragFloat2("Rotate", angles, 0.01f))
				{
					XMVECTOR qYaw = XMQuaternionRotationRollPitchYaw(0, angles[0], 0);
					XMVECTOR qPitch = XMQuaternionRotationRollPitchYaw(angles[1], 0, 0);

					qVec = XMQuaternionMultiply(qVec, qYaw);
					qVec = XMQuaternionMultiply(qPitch, qVec);
					XMStoreFloat4(&transformComponent1->Rotation, qVec);
					XMStoreFloat4(&transformComponent2->Rotation, qVec);
				}

				if (ImGui::DragFloat3("Position", &transformComponent1->Position.x, 0.01f))
				{
					transformComponent2->Position = transformComponent1->Position;
				}

				if (ImGui::Button("Move to camera"))
				{
					XMFLOAT4X4A* pCameraTransform = (XMFLOAT4X4A*)GetOffset(Offset::CameraTransform);
					XMVECTOR qRotation = XMQuaternionRotationMatrix(XMLoadFloat4x4A(pCameraTransform));
					XMVECTOR vPosition = XMLoadFloat3((XMFLOAT3*)pCameraTransform->m[3]);

					XMStoreFloat4(&transformComponent1->Rotation, qRotation);
					XMStoreFloat4(&transformComponent2->Rotation, qRotation);
					XMStoreFloat4(&transformComponent1->Position, vPosition);
					XMStoreFloat4(&transformComponent2->Position, vPosition);
				}

				ImGui::NewLine();

				ImGui::Separator();

				if (ImGui::CollapsingHeader("Static Properties", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto archetypeSpotlight = (Northlight::content::SpotLightComponent*)light.Archetype->m_Components[0];
					DrawArchetypeEditor(archetypeSpotlight, true);

					if (ImGui::Button("Update light"))
					{
						// Copy ECS component properties to archetype
						archetypeSpotlight->m_LightColor = (0xFF << 24)
							| ((unsigned int)(lightComponent->Color.x * 255.0f) << 16)
							| ((unsigned int)(lightComponent->Color.y * 255.0f) << 8)
							| ((unsigned int)(lightComponent->Color.z * 255.0f) << 0);
						archetypeSpotlight->m_fIntensity = lightComponent->Intensity;
						archetypeSpotlight->m_fFov = XMConvertToDegrees(spotlightComponent->FieldOfView);
						archetypeSpotlight->m_fNear = spotlightComponent->NearPlane;
						archetypeSpotlight->m_fFar = spotlightComponent->FarPlane;
						archetypeSpotlight->m_fRadius = spotlightComponent->Radius;

						auto archetypeTransform = (Northlight::content::TransformComponent*)light.Archetype->m_Components[1];
						archetypeTransform->m_qRotation = transformComponent1->Rotation;
						archetypeTransform->m_vPosition = transformComponent1->Position;

						Northlight::DestroyEntity(light.EntityHandle);
						light.EntityHandle = -1;
						light.EntityGID = Northlight::SpawnArchetype(light.ArchetypeGID);
					}
				}

				ImGui::PopItemWidth();
			}
			ImGui::End();
			ImGui::PopID();
		}
	}

	export void DrawLightsTab()
	{
		// Update entity handles and delete lights
		for (auto itr = s_Lights.begin(); itr != s_Lights.end();)
		{
			Light& light = *itr;

			if (light.EntityHandle == -1)
			{
				light.EntityHandle = Northlight::GetEntityHandle(light.EntityGID);
			}

			if (light.Delete && light.EntityHandle != -1)
			{
				Northlight::DestroyEntity(light.EntityHandle);
				itr = s_Lights.erase(itr);
			}
			else
			{
				itr++;
			}
		}

		if (ImGui::Button("Create spotlight"))
		{
			XMFLOAT4X4A* pCameraTransform = (XMFLOAT4X4A*)GetOffset(Offset::CameraTransform);
			XMVECTOR qRotation = XMQuaternionRotationMatrix(XMLoadFloat4x4A(pCameraTransform));

			XMStoreFloat4(&s_ArchetypeTransform->m_qRotation, qRotation);

			s_ArchetypeTransform->m_vPosition.x = pCameraTransform->m[3][0];
			s_ArchetypeTransform->m_vPosition.y = pCameraTransform->m[3][1];
			s_ArchetypeTransform->m_vPosition.z = pCameraTransform->m[3][2];

			ImGui::OpenPopup("New spotlight");
		}

		ImGui::BeginChild("##Lights", ImVec2(0, 0), true);

		for (auto& light : s_Lights)
		{
			if (light.EntityHandle == -1) continue;

			ImGui::PushID((const void*)light.EntityHandle);

			if (ImGui::Checkbox("##IsEnabled", &light.Enabled))
			{
				static const unsigned int HideReasonId = Northlight::ecs::GetEcsId("coregame::component::HideReason>(void)", "ecs::core::ComponentContext>(void)");
				unsigned int* pHideReason = Northlight::ecs::GetComponent<unsigned int>(light.EntityHandle, HideReasonId); // coregame::component::HideReason
				*pHideReason = (*pHideReason & ~(1 << 2)) | (!light.Enabled << 2);
			}

			ImGui::SameLine();

			ImGui::Text(light.Name.c_str());

			const float width = ImGui::GetWindowWidth();

			ImGui::SameLine(width - 100);
			if (ImGui::Button("Edit"))
			{
				light.ShowEditor = true;
			}


			ImGui::SameLine();
			if (ImGui::Button("Remove"))
			{
				light.Delete = true;
			}
		}

		ImGui::EndChild();

		if (ImGui::BeginPopupModal("New spotlight", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			DrawArchetypeEditor(s_ArchetypeSpotlight, false);

			ImGui::NewLine();

			if (ImGui::Button("Create"))
			{
				Light newLight = {};
				newLight.Name = "Light #" + std::to_string(s_NextLightId++);

				auto [archetypeGID, pArchetype] = CopyAndRegisterArchetype(newLight.Name);

				newLight.Archetype = pArchetype;
				newLight.ArchetypeGID = archetypeGID;
				newLight.EntityGID = Northlight::SpawnArchetype(archetypeGID);

				s_Lights.emplace_back(newLight);

				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}
	};
}