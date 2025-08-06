#include <DirectXMath.h>
#include <dxgi1_5.h>
#include <d3d12.h>
#include <stdint.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define PADHELPER2(sz,line) private: uint8_t pad##line##[sz]; public:
#define PADHELPER(sz,line) PADHELPER2(sz,line)
#define PAD(sz) PADHELPER(sz, __LINE__)

using namespace DirectX;

export module Northlight;

import Offsets;
import Log;
import std;

export namespace sl
{
    struct DECLSPEC_UUID("D3F0BBFF-3091-4074-9D9E-B99CE2E5CF9A") DXGISwapChain : IDXGISwapChain4
    {
        virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override final;
        ULONG   STDMETHODCALLTYPE AddRef() override final;
        ULONG   STDMETHODCALLTYPE Release() override final;

#pragma region IDXGIObject
        HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void* pData) override final;
        HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown) override final;
        HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData) override final;
        HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent) override final;
#pragma endregion
#pragma region IDXGIDeviceSubObject
        HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) override final;
#pragma endregion
#pragma region IDXGISwapChain
        HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) override final;
        HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) override final;
        HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget) override final;
        HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget) override final;
        HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc) override final;
        HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) override final;
        HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) override final;
        HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** ppOutput) override final;
        HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) override final;
        HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* pLastPresentCount) override final;
#pragma endregion
#pragma region IDXGISwapChain1
        HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc) override final;
        HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc) override final;
        HRESULT STDMETHODCALLTYPE GetHwnd(HWND* pHwnd) override final;
        HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void** ppUnk) override final;
        HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) override final;
        BOOL    STDMETHODCALLTYPE IsTemporaryMonoSupported() override final;
        HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput) override final;
        HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA* pColor) override final;
        HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* pColor) override final;
        HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override final;
        HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* pRotation) override final;
#pragma endregion
#pragma region IDXGISwapChain2
        HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override final;
        HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* pWidth, UINT* pHeight) override final;
        HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override final;
        HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* pMaxLatency) override final;
        HANDLE  STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override final;
        HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* pMatrix) override final;
        HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* pMatrix) override final;
#pragma endregion
#pragma region IDXGISwapChain3
        UINT    STDMETHODCALLTYPE GetCurrentBackBufferIndex() override final;
        HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT* pColorSpaceSupport) override final;
        HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override final;
        HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT* pCreationNodeMask, IUnknown* const* ppPresentQueue) override final;
#pragma endregion
#pragma region IDXGISwapChain4
        HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void* pMetaData) override final;
#pragma endregion

        uint8_t padding[8];
        IDXGISwapChain* m_base; // IMPORTANT: Must be at a fixed offset to support tools, do not move!

        IUnknown* const m_d3dDevice;
        LONG m_refCount = 1;
        uint32_t m_d3dVersion;
        uint32_t m_interfaceVersion;
        uint32_t m_cachedHostSDKVersion;
    };
}

export namespace Northlight
{
    __int64 GetEcsHandle(__int64);

    __int64 ModuleHandle()
    {
        static HMODULE s_moduleHandle = GetModuleHandleA("AlanWake2.exe");
        return (__int64)s_moduleHandle;
    }

    namespace ecs
    {
        unsigned int GetEcsId(const std::string name, const std::string type)
        {
            typedef std::unordered_map<std::string, unsigned int> EcsTypeToIdMap;
            typedef std::unordered_map<std::string, EcsTypeToIdMap> EcsNameToTypeIdMap;
            static EcsNameToTypeIdMap s_EcsNameToTypeIdMap;

            if (s_EcsNameToTypeIdMap.empty())
            {
                struct EcsIdInfo
                {
                    EcsIdInfo* pNext;
                    unsigned int* pId;
                    const char* type;
                    __int64 unk1;
                    const char* name;
                    __int64 unk2;
                };

                EcsIdInfo* pIdInfo = *(EcsIdInfo**)GetOffset(Offset::EcsTypeInfo);

                while (pIdInfo != nullptr)
                {
                    std::string name = std::string(pIdInfo->name);
                    std::string type = std::string(pIdInfo->type);
                    unsigned int id = *pIdInfo->pId;

                    auto itr = s_EcsNameToTypeIdMap.find(name);
                    if (itr == s_EcsNameToTypeIdMap.end())
                    {
                        itr = s_EcsNameToTypeIdMap.emplace(name, EcsTypeToIdMap()).first;
                    }

                    auto&& typeToIdMap = itr->second;
                    const bool inserted = typeToIdMap.emplace(type, id).second;
                    if (!inserted)
                    {
                        Log::Warning("Duplicate ECS id type entry - Name %s - Type %s", name.c_str(), type.c_str());
                    }

                    pIdInfo = pIdInfo->pNext;
                }

                Log::Write("ECS Id map initialized");
            }

            auto itr = s_EcsNameToTypeIdMap.find(name);
            if (itr == s_EcsNameToTypeIdMap.end())
            {
                Log::Error("Couldn't find ECS name %s", name.c_str());
                return 0;
            }

            auto itr2 = itr->second.find(type);
            if (itr2 == itr->second.end())
            {
                Log::Error("Could not find ECS Id for type %s of name %s", type.c_str(), name.c_str());
                return 0;
            }

            return itr2->second;
        }

        enum class EcsId : unsigned int
        {
            AnimationMixer,
            GlobalIDToEntityMap,
            LightComponent,
            MeshBoneMapping,
            MeshResource,
            PreviousWorldTransformComponent,
            SpotlightComponent,
            WorldTransformComponent,
            Count
        };

        void InitializeEcsIds();
        constexpr unsigned int GetEcsId(EcsId id);
    };

    struct StringRef
    {
        const char* str;
        size_t sz;
    };

    class InputSystem
    {
    public:
        PAD(0x70);
        bool m_KeyboardEnabled;
        bool m_MouseEnabled;
        bool m_ControllerEnabled1; // Not sure which these two are for, I assume Xinput and Dinput controllers
        bool m_ControllerEnabled2;

    public:
        static InputSystem* GetInstance()
        {
            //return *(InputSystem**)((__int64)ModuleHandle() + 0x3AC68E8);

            return *(InputSystem**)(GetOffset(Offset::InputSystem));
        }
    };

    namespace coregame
    {
        class GameWindow
        {
        public:
            PAD(0x58);
            bool m_ForceShowMouse;

        public:
            static GameWindow* GetInstance()
            {
                //return *(GameWindow**)((__int64)ModuleHandle() + 0x38038C8);

                return *(GameWindow**)(GetOffset(Offset::GameWindow));
            }
        };

        class ResourceManager
        {
        public:
            PAD(0x10);
            __int64 m_ResourceMap;
            PAD(0x160);
            __int64 m_Resources;

        public:
            static ResourceManager* GetInstance()
            {
                return *(ResourceManager**)GetOffset(Offset::ResourceManager);
            }
        };
    }

    namespace r
    {
        struct GlobalID
        {
            __int64 GID{ 0 };

            GlobalID() : GID(0) {};
            GlobalID(__int64 _GID) : GID(_GID) {};
            GlobalID(unsigned short type, char const* name, __int64 owner) { set(type, name, owner); }

            void set(unsigned short a1, char const* a2, __int64 owner)
            {
                GID = 0;
                typedef __int64(__fastcall* tCityHash64)(const char* str, size_t sz);
                //tCityHash64 CityHash64 = (tCityHash64)((__int64)ModuleHandle() + 0x23B4990);
                tCityHash64 CityHash64 = (tCityHash64)GetOffset(Offset::CityHash);

                GID = CityHash64(a2, strlen(a2)) & 0xFFFFFFFFFFFF;
                GID |= (owner << 48);
                GID = GID << 14;
                GID |= (a1 & 0x3FFF);
            }

            operator __int64() const
            {
                return GID;
            }
        };

        class ObjectStreamProcessorBase
        {
        public:
            ObjectStreamProcessorBase()
            {
                memset(this, 0, sizeof(ObjectStreamProcessorBase));
                *(int*)((__int64)this + 0xC) = 0;

                *(int*)((__int64)this + 0x328) = 1;
                *(int*)((__int64)this + 0x338) = -1;

                typedef void(__fastcall* tObjectStreamProcessorBaseConstruct)(ObjectStreamProcessorBase*);
                //tObjectStreamProcessorBaseConstruct Construct = (tObjectStreamProcessorBaseConstruct)((__int64)ModuleHandle() + 0x233EAD0);
                tObjectStreamProcessorBaseConstruct Construct = (tObjectStreamProcessorBaseConstruct)GetOffset(Offset::ObjectStreamProcessorCtor);
                Construct(this);
            }

            PAD(0x340);
        };


        template<class T>
        class ClassTypeInfo
        {
        public:
            virtual const char* getName();
            virtual unsigned int getHash();
            virtual unsigned int getSize();
            virtual void Func4();
            virtual ClassTypeInfo* getParentTypeInfo();
            virtual ClassTypeInfo* getTypeInfo();
            virtual void Func7();
            virtual void Func8();
            virtual void Func9();
            virtual void Func10();
            virtual void Func11();
            virtual void Func12();
            virtual void init(T* a1);
            virtual void Func14();
            virtual void Func15();
            virtual void Func16();
            virtual void Func17();
            virtual void Func18();
            virtual void Func19();
            virtual void Func20();
            virtual void Func21();
            virtual void Func22();
            virtual void Func23();
            virtual void Func24();
            virtual void Func25();
            virtual void Func26();
            virtual void LoadDefaultValues(T* a1, r::ObjectStreamProcessorBase* processor, bool a3 = false);
            virtual void Func28();
            virtual void Func29();
            virtual void Func30();
            virtual void Func31();
            virtual void Func32();
            virtual void Func33();
            virtual void Func34();
            virtual void Func35();
            virtual void Func36();
            virtual void Func37();
            virtual void Func38();
            virtual void Func39();
            virtual void Func40();
            virtual void Func41();
            virtual void Func42();
            virtual void Func43();
            virtual void Func44();
            virtual void Func45();
            virtual void Func46();
            virtual void Func47();
            virtual void Func48();
            virtual void Func49();
            virtual void Func50();
            virtual void Func51();
            virtual void Func52();
            virtual void Func53();
            virtual void Func54();
            virtual void Func55();
            virtual void Func56();
            virtual void Func57();
            virtual void Func58();
        };

        class ClassFactory
        {
        public:
            template<class T>
            static T* ConstructType(const char* className, bool loadDefaults = true)
            {
                StringRef strRef{ .str = className, .sz = strlen(className) };
                ClassTypeInfo<T>* typeInfo = GetTypeInfo<T>(strRef);

                typedef T* (__fastcall* tConstructType)(ClassTypeInfo<T>*);
                tConstructType _ConstructType = (tConstructType)GetOffset(Offset::ConstructType);

                T* result = _ConstructType(typeInfo);

                if (loadDefaults)
                {
                    ObjectStreamProcessorBase dummyProcessor = ObjectStreamProcessorBase();
                    typeInfo->LoadDefaultValues(result, &dummyProcessor, false);
                }

                return result;
            };

            template<class T>
            static ClassTypeInfo<T>* GetTypeInfo(StringRef& className)
            {
                typedef ClassTypeInfo<T>* (__fastcall* tGetTypeInfo)(StringRef*);
                tGetTypeInfo _GetTypeInfo = (tGetTypeInfo)GetOffset(Offset::GetTypeInfo);
                return _GetTypeInfo(&className);
            };
        };

        class ContentEntityBase
        {
        public:
            virtual void Func1();
            virtual void Func2();
            virtual void Func3();
            virtual void Func4();
            virtual void Func5();
            virtual void Func6();
            virtual void Func7();
            virtual void Func8();
            virtual void Func9();
            virtual void Func10();
            virtual void Func11();
            virtual void Func12();
            virtual void Func13();
            virtual void Func14();
            virtual void Func15();
            virtual void Func16();
            virtual void Func17();
            virtual void Func18();
            virtual const char* getTypeNameVirtual();
            virtual unsigned int getTypeIDVirtual();
            virtual void* getTypeInfoVirtual();

            PAD(0x30);
        }; // Size = 0x38

        class GlobalIDMap
        {
        public:
            static GlobalIDMap* GetInstance()
            {
                //return *(GlobalIDMap**)((__int64)ModuleHandle() + 0x388E170);

                return *(GlobalIDMap**)GetOffset(Offset::GlobalIdMap);
            }

            void RegisterID(GlobalID const& id, void* object)
            {
                typedef void(__fastcall* tRegisterID)(GlobalIDMap* _this, GlobalID const& id, void* object);
                //tRegisterID _RegisterId = (tRegisterID)((__int64)ModuleHandle() + 0x238F200);
                tRegisterID _RegisterId = (tRegisterID)GetOffset(Offset::RegisterId);
                _RegisterId(this, id, object);
            }

            struct Entry
            {
                GlobalID ID;
                void* pObject;
            };
        };

        class RendGlobalParameterBase
        {
        public:
            virtual void Func1();

            PAD(0x4);
            unsigned int m_SzName;
            const char m_Name[0x30];

            PAD(0x50);
            unsigned int m_Type;
            PAD(0x4);
        };

        template<typename T>
        class RendGlobalParameter : RendGlobalParameterBase
        {
        public:
            T m_Unk;
            T m_Value;
        };
    }

	namespace rend
	{
        class RendererInterface
        {
        public:

        public:
            static RendererInterface* GetInstance()
            {
                //return *(RendererInterface**)((__int64)Northlight::ModuleHandle() + 0x3A54628);
                return *(RendererInterface**)(GetOffset(Offset::RendererInterface));
            };
        };

        sl::DXGISwapChain* GetDxgiSwapChain()
        {
            RendererInterface* rendererInterface = RendererInterface::GetInstance();

            __int64 ptr1 = *(__int64*)((__int64)rendererInterface + 0x48);
            __int64 ptr2 = *(__int64*)ptr1;
            __int64 pSwapChainWrapper = *(__int64*)(ptr2 + 0x8);

            return (sl::DXGISwapChain*)(pSwapChainWrapper);
        }

        IDXGISwapChain3* GetSwapChain()
        {
            RendererInterface* rendererInterface = RendererInterface::GetInstance();

            __int64 ptr1 = *(__int64*)((__int64)rendererInterface + 0x48);
            __int64 ptr2 = *(__int64*)ptr1;
            __int64 pSwapChainWrapper = *(__int64*)(ptr2 + 0x8);

            return *(IDXGISwapChain3**)(pSwapChainWrapper + 0x10);
        }

        struct CommandQueue
        {
            enum Type : int
            {
                Direct = 0,
                AsyncCompute = 1,
                AsyncCopy = 2
            };

            Type type;
            PAD(0xC);
            ID3D12CommandQueue* pDxCommandQueue;
            PAD(0x30);

            static CommandQueue* GetDirectCommandQueue()
            {
                //return *(CommandQueue**)((__int64)ModuleHandle() + 0x3805AD8);
                return *(CommandQueue**)GetOffset(Offset::DirectCommandQueue);
            }
        };
	}

    namespace content
    {
        class GenericComponent;

        struct ComponentRef
        {
            r::GlobalID GID;
            GenericComponent* Component;
            __int64 Flags;
        };

        class GenericEntity : public r::ContentEntityBase
        {
        public:
            virtual void Func22();
            virtual void Func23();
            virtual void Func24();
            virtual void Func25();
            virtual void Func26();
            virtual void Func27();
            virtual void Func28();
            virtual void Func29();
            virtual void Func30();
            virtual void Func31();
            virtual void Func32();
            virtual void Func33();
            virtual void Func34();
            virtual void Func35();
            virtual void Func36();
            virtual void Func37();
            virtual void Func38();
            virtual void Func39();

            PAD(0x3C);
            unsigned int m_IdSz;
            char m_Id[0x30];

            ComponentRef* m_Components;
            unsigned int m_ComponentCount;
            unsigned int m_ReservedCount;
            PAD(0x8);

            void SetName(const char* name)
            {
                const size_t szName = strlen(name);
                m_IdSz = static_cast<unsigned int>(szName);
                memcpy(m_Id, name, szName);
            }
        }; // Size = 0xC0

        class GenericComponent
        {
        public:
            virtual void Func1();
            virtual void Func2();
            virtual void Func3();
            virtual void Func4();
            virtual void Func5();
            virtual void Func6();
            virtual void Func7();
            virtual void Func8();
            virtual void Func9();
            virtual void Func10();
            virtual void Func11();
            virtual void Func12();
            virtual void Func13();
            virtual void Func14();
            virtual void Func15();
            virtual void Func16();
            virtual void Func17();
            virtual void Func18();
            virtual void Func19();
            virtual void Func20();
            virtual void Func21();
            virtual void Func22();
            virtual void Func23();
            virtual void Func24();
            virtual void Func25();
            virtual void Func26();
            virtual void Func27();
            virtual void Func28();
            virtual void Func29();
            virtual void LoadDefaultValues(r::ObjectStreamProcessorBase* processor, bool a3 = false);
            virtual void Func31();
            virtual void Func32();
            virtual void Func33();
            virtual void Func34();
            virtual void Func35();
            virtual void Func36();
            virtual void Func37();
            virtual void Func38();
            virtual void Func39();
            virtual const char* getTypeNameVirtual();
            virtual unsigned int getTypeIDVirtual();
            virtual void* getTypeInfoVirtual();
        }; // Size 0x8

        class TransformComponent : public GenericComponent
        {
        public:
            PAD(0x8);
            XMFLOAT4 m_qRotation;
            XMFLOAT4 m_vPosition;
            PAD(0x20);
        }; // Size: 0x50

        class SpotLightComponent : public GenericComponent
        {
        public:
            enum class DynamicShadowType
            {
                Off,
                PCF,
                VSM
            };

            enum class StaticShadowType
            {
                Off,
                VSM
            };

            enum class ProjectionType
            {
                Perspective,
                Stereographic
            };

            enum class DynamicShadowMapSize
            {
                s128,
                s256,
                s512,
                s1024,
            };

            enum class StaticShadowMapSize
            {
                s32,
                s64,
                s128,
                s256,
                s512
            };

            enum class VolumetricQuality
            {
                Low,
                Medium,
                High
            };

            enum class VolumetricBoundingShape
            {
                Cone,
                SquareFrustum
            };

            __int64                 m_ResourceProjectionMap;

            DynamicShadowType       m_eDynamicShadowType;		// 0x10 Type of dynamic shadows
            StaticShadowType        m_eStaticShadowType;		// 0x14 Type of static shadows
            StaticShadowMapSize     m_eStaticShadowMapSize;		// 0x18 Size of the static shadow map
            DynamicShadowMapSize    m_eShadowMapSize;			// 0x1C Size of the dynamic shadow map
            VolumetricQuality       m_eVolumetricQuality;		// 0x20 Quality of the volumetric effect
            VolumetricBoundingShape m_eVolumetricBoundingShape;	// 0x24 Base shape of volumetric effect
            ProjectionType          m_eProjectionType;			// 0x28 Type of mapping used on projection map

            int m_LightColor;                       // 0x2C Color of the light
            float m_fIntensity;                     // 0x30 Intensity of the light
            float m_fFov;                           // 0x34 Field of view in degrees (0.1-179.0)
            float m_fNear;                          // 0x38 Distance of the near plane (0.1-99.99)
            float m_fDepthBias;                     // 0x3C Bias for preventing shadow artifacts (0.0-0.01)
            float m_fDepthSlopeBias;                // 0x40 Bias for preventing shadow artifacts (0.1-16.0)
            float m_fRadius;                        // 0x44 Source radius of area light
            float m_fConeShape;                     // 0x48 Aspect ratio of light volume
            float m_fFar;							// 0x4C The distance of the far plane (0-32768)
            float m_fDirectionality;				// 0x50 Directionality blends light falloff between masked and directed spot
            float m_fScatterIntensityMultiplier;	// 0x54 Multiplier for intensity of scattered light
            float m_fVolumetricIntensityMultiplier;	// 0x58 Multiplier for intensity of volumetric effect
            float m_fVolumetricLightPhase1;			// 0x5C First scattering lobe of the volumetric light
            float m_fVolumetricLightPhase2;			// 0x60 Second scattering lobe of the volumetric light
            float m_fVolumetricLightPhaseBlend;		// 0x64 Blend between scattering lobes of the volumetric light
            float m_fIndirectIntensityMultiplier;	// 0x68 Multiplier for intensity of indirect light
            float m_fDirectIntensityMultiplier;     // 0x6C Probe Direct Intensity Multiplier
            float m_fDrawDistance;					// 0x70 Max distance from camera that the light is drawn
            float m_fMaxDynamicShadowDistance;		// 0x74 Max distance between camera and light to use dynamic shadows
            float m_fShadowFilterSize;				// 0x78 Size of shadow filter
            float m_fProjectionMapMipmapLevel;      // 0x7C Projection Map Mipmap Level
            float m_vProjectionMapScale[2];         // 0x80 Using negative number will flip the projection map in corresponding axis
            float m_fSpecularIntensityMultiplier;   // 0x88 Can be used to reduce and remove specular highlights
            bool m_bEnableFarClip;					// 0x8C Should the light be cliped to the far plane
            bool m_bAutostart;						// 0x8D Should the light be active when it loads in game
            bool m_bBentNormals;					// 0x8E Should the normals be bent
            bool m_bEnableVolumetric;				// 0x8F Should this be used as a runtime volumetric light
            bool m_bEnableDirect;					// 0x90 Should this be used as a direct runtime light
            bool m_bPrioritize;                     // 0x91 If this light should be given atlas priority (no resize and full update)
            PAD(0x6);
            __int64 m_ResourceShadowMap;			// 0x88 Shadow map

        }; // Size 0xA0

        class EntityArchetype
        {
        public:
            PAD(0x4);
            unsigned int m_NameSz;
            char m_Name[0x30];
            PAD(0x58);
            GenericEntity m_Entity;
            GenericComponent** m_Components;
            unsigned int m_ComponentCount;
            unsigned int m_ReservedCount;
            PAD(0x10);

            void SetName(const char* name)
            {
                const size_t szName = strlen(name);
                m_NameSz = static_cast<unsigned int>(szName);
                memcpy(m_Name, name, szName);
            }
        }; // Size 0x170
    }   

    namespace ecs
    {
        void* GetComponent(__int64 entityHandle, uint16_t id, size_t componentSize)
        {
            __int64 ecsHandle = GetEcsHandle(entityHandle);
            __int64 a1 = *(__int64*)GetOffset(Offset::GameServerWorld);
            a1 = *(__int64*)(a1 + 0x8);

            const uint16_t ecsIndex = static_cast<uint16_t>(ecsHandle & 0xFFFF);
            const uint16_t ecsOffset = static_cast<uint16_t>(ecsHandle >> 32);

            __int64 pEcsEntity = *(__int64*)(a1 + 0x88 + 0x8 * ecsIndex);
            if (!pEcsEntity)
            {
                return nullptr;
            }

            uint16_t* components = *(uint16_t**)(a1 + 0x18098 + 0x20 * ecsIndex);
            unsigned int componentCount = *(unsigned int*)(a1 + 0x180AC + 0x20 * ecsIndex);
            unsigned int* componentOffsets = *(unsigned int**)(a1 + 0x40 + (ecsIndex + 0xC03) * 0x20);

            for (unsigned int i = 0; i < componentCount; ++i)
            {
                if (components[i] == id)
                {
                    return (void*)(pEcsEntity + componentOffsets[i] + ecsOffset * componentSize);
                }
            }

            return nullptr;
        }

        template<typename T>
        T* GetComponent(__int64 entityHandle, uint16_t id)
        {
            return (T*)GetComponent(entityHandle, id, sizeof(T));
        }

        template<typename T, unsigned int SizeValue, unsigned int IdValue>
        struct Component
        {
            static const unsigned int Size = SizeValue;
            static const unsigned int Id = IdValue;

            static T* Get(__int64 entityHandle)
            {
                return GetComponent<T>(entityHandle, Id);
            }
        };

        struct LightComponent// : public Component<LightComponent, 0x20, 0x2E9>
        {
            XMFLOAT3 Color;
            float Unknown;

            float IntensityMultiplier;
            float Intensity;
            float Unknown1;
            unsigned int Flags;
        };// Size 0x20

        struct SpotlightComponent// : public Component<SpotlightComponent, 0x18, 0x374>
        {
            float FieldOfView;
            float NearPlane;
            float FarPlane;
            float Radius;

            float Unknown1;
            float Unknown2;
        };// Size 0x18

        struct TransformComponent
        {
            XMFLOAT4 Rotation;
            XMFLOAT4 Position;
        };

        void GetComponentsForEntity(__int64 entityHandle, std::vector<std::pair<unsigned int, __int64>>& outComponents)
        {
            __int64 ecsHandle = GetEcsHandle(entityHandle);
            __int64 a1 = *(__int64*)GetOffset(Offset::GameServerWorld);
            a1 = *(__int64*)(a1 + 0x8);

            const uint16_t ecsIndex = static_cast<uint16_t>(ecsHandle & 0xFFFF);
            const uint16_t ecsOffset = static_cast<uint16_t>(ecsHandle >> 32);

            __int64 pEcsEntity = *(__int64*)(a1 + 0x88 + 0x8 * ecsIndex);
            if (!pEcsEntity)
            {
                return;
            }

            uint16_t* components = *(uint16_t**)(a1 + 0x18098 + 0x20 * ecsIndex);
            unsigned int componentCount = *(unsigned int*)(a1 + 0x180AC + 0x20 * ecsIndex);
            unsigned int* componentOffsets = *(unsigned int**)(a1 + 0x40 + (ecsIndex + 0xC03) * 0x20);

            for (unsigned int i = 0; i < componentCount; ++i)
            {
                outComponents.emplace_back(components[i], pEcsEntity + componentOffsets[i]);
            }
        }

        const char* GetComponentName(unsigned int idx)
        {
            static std::unordered_map<unsigned int, const char*> s_EcsComponentNameMap;

            if (s_EcsComponentNameMap.empty())
            {
                struct EcsTypeInfo
                {
                    EcsTypeInfo* pNext;
                    unsigned int* pId;
                    const char* sTypeName;
                    __int64 unk1;
                    const char* sTypeName2;
                };

                EcsTypeInfo* pType = *(EcsTypeInfo**)GetOffset(Offset::EcsTypeInfo);

                while (pType != nullptr)
                {
                    if (strcmp("ecs::core::ComponentContext>(void)", pType->sTypeName) == 0)
                    {
                        s_EcsComponentNameMap.emplace(*pType->pId, pType->sTypeName2);
                    }

                    pType = pType->pNext;
                }
            }

            auto findResult = s_EcsComponentNameMap.find(idx);
            return findResult != s_EcsComponentNameMap.end() ? findResult->second : "Unknown";
        }

    }

    __int64 GetEntityHandle(r::GlobalID entityGID)
    {
        if ((entityGID.GID & 0x3FFF) == 0)
            return -1;

        static unsigned int GlobalIDToEntityMapID = ecs::GetEcsId("coregame::global::GlobalIDToEntityMap>(void)", "ecs::core::EnvironmentId>(void)");

        __int64 a1 = *(__int64*)GetOffset(Offset::GameServerWorld);
        a1 = *(__int64*)(a1 + 0x8);
        a1 = *(__int64*)(a1 + 0x580E0);
        a1 = *(__int64*)(a1 + GlobalIDToEntityMapID * 0x8); // coregame::global::GlobalIDToEntityMap>(void)

        typedef void(__fastcall* tFindGIDInMap)(__int64 pMap, __int64* outIndex, __int64* pGID);
        tFindGIDInMap FindGIDInMap = (tFindGIDInMap)GetOffset(Offset::FindGidInMap);
        __int64 result[2] = {};

        FindGIDInMap(a1, &result[0], &entityGID.GID);
        if (result[0] == -1)
        {
            return -1;
        }

        struct GIDMapEntry
        {
            __int64 GID;
            __int64 Handle;
        };

        GIDMapEntry* mapEntries = *(GIDMapEntry**)(a1 + 0x58);
        return mapEntries[result[0]].Handle;
    }

    __int64 GetEcsHandle(__int64 entityHandle)
    {
        __int64 a1 = *(__int64*)GetOffset(Offset::GameServerWorld);
        a1 = *(__int64*)(a1 + 0x8);
        a1 = *(__int64*)(a1 + 0x30);

        return *(__int64*)(a1 + 8 * (entityHandle & 0xFFFFFFFF));
    }

    r::GlobalID GetCurrentAvatarID()
    {
        static unsigned int CurrentAvatarId = ecs::GetEcsId("bigfish::global::CurrentAvatar>(void)", "ecs::core::EnvironmentId>(void)");

        __int64 a1 = *(__int64*)GetOffset(Offset::GameServerWorld);
        a1 = *(__int64*)(a1 + 0x8);
        a1 = *(__int64*)(a1 + 0x580E0);
        a1 = *(__int64*)(a1 + 0x8 * CurrentAvatarId); // bigfish::global::CurrentAvatar>(void)

        return *(r::GlobalID*)(a1 + 0x50);
    }

    __int64 GetCurrentAvatarEntityHandle()
    {
        static unsigned int CurrentAvatarId = ecs::GetEcsId("bigfish::global::CurrentAvatar>(void)", "ecs::core::EnvironmentId>(void)");

        __int64 a1 = *(__int64*)GetOffset(Offset::GameServerWorld);
        a1 = *(__int64*)(a1 + 0x8);
        a1 = *(__int64*)(a1 + 0x580E0);
        a1 = *(__int64*)(a1 + 0x8 * CurrentAvatarId); // bigfish::global::CurrentAvatar>(void)

        r::GlobalID avatarID = *(r::GlobalID*)(a1 + 0x50);
        if ((avatarID.GID & 0x3FFF) == 0)
            return -1;

        return GetEntityHandle(avatarID);
    }

    void DestroyEntity(__int64 entityHandle)
    {
        typedef __int64(__fastcall* tGetDestroyHandle)();
        tGetDestroyHandle GetDestroyHandle = (tGetDestroyHandle)GetOffset(Offset::GetDestroyHandle);

        __int64 destroyHandle = GetDestroyHandle();

        typedef void(__fastcall* tDestroy)(__int64, __int64);
        tDestroy Destroy = (tDestroy)GetOffset(Offset::DestroyEntity);

        __int64 a1 = *(__int64*)GetOffset(Offset::GameServerWorld);
        a1 = *(__int64*)(a1 + 0x8);
        a1 = (a1 + 0x580F0 + 0x188 * destroyHandle);

        Destroy(a1, entityHandle);
    }

    r::GlobalID SpawnArchetype(r::GlobalID archetypeGID)
    {
        typedef void(__fastcall* tSpawnArchetype)(void*, __int64, void*, void*);
        tSpawnArchetype SpawnArchetype = (tSpawnArchetype)GetOffset(Offset::SpawnArchetype);

        __int64 a4[2] = { 0,0 };
        __int64 a3[4] = { archetypeGID.GID,0,0,2 };
        static __int64 result[4]{ 0,0,0,0 };

        static unsigned int SpawningPoolRegistryId = ecs::GetEcsId("coregame::env::SpawningPoolRegistry>(void)", "ecs::core::EnvironmentId>(void)");

        __int64 a1 = *(__int64*)GetOffset(Offset::GameServerWorld);
        a1 = *(__int64*)(a1 + 0x8);
        a1 = *(__int64*)(a1 + 0x580E0);
        a1 = *(__int64*)(a1 + SpawningPoolRegistryId * 0x8); // coregame::env::SpawningPoolRegistry

        SpawnArchetype(result, a1, a3, a4);
        return result[0];
    }
}