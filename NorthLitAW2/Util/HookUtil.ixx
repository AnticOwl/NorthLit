export module HookUtil;

export void OverrideVTableFunction(void* ppVtable, unsigned int index, void* pHook, void* pOriginal);

export void CreateHook(void* pFunction, void* pHook, void* ppOriginal);
export void RemoveHook(void* pFunction);

export bool WriteMemory(void* dwAddress, const void* cpvPatch, unsigned int dwSize);

export void InitializeMinHook();
export void UninitializeMinHook();