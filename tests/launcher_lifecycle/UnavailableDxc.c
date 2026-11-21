/* Test-only valid DLL without DxcCreateInstance. The normal loader finds it,
 * then exercises Vulkan's unavailable-compiler recovery without changing any
 * installed driver, source compiler, PATH, or build-machine DLL. Never ship. */
__declspec(dllexport) int renderer_test_unavailable_dxc(void) { return 1; }
