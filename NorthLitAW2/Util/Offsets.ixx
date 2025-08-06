export module Offsets;

import std;

export enum Offset
{
	AnimationMixerPreUpdate,
	CameraTransform,
	CameraUpdate,
	CityHash,
	ConstructType,
	DestroyEntity,
	DialogueAnimationUpdate,
	DirectCommandQueue,
	EcsTypeInfo,
	//ExposureFixed,
	//ExposureMode,
	FindGidInMap,
	GameServerWorld,
	GameWindow,
	GetDestroyHandle,
	GetTypeInfo,
	GlobalIdMap,
	GlobalParams,
	HotsampleFix,
	InputSystem,
	ObjectStreamProcessorCtor,
	PuppetUpdate,
	ResourceManager,
	RegisterId,
	RendererInterface,
	SpawnArchetype,

	Count
};

export namespace Offsets
{
	bool ScanOffsets();

	long long GetOffset(const std::string& name);
}

export constexpr long long GetOffset(Offset eOffset);