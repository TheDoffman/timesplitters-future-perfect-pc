#include <d3d11.h>
#include <dxgi1_2.h>
#include <iostream>
#include <wincodec.h>
#include <windows.h>
#include <winrt/base.h>
using namespace winrt;
// Opt-in test helper, never shipped. GDI redirected surfaces can be stale for
// independently flipped Vulkan windows. Capture only the verified child's rect.
struct Search {
  DWORD pid;
  HWND hwnd = nullptr;
  int count = 0;
};
BOOL CALLBACK Find(HWND h, LPARAM p) {
  auto &s = *reinterpret_cast<Search *>(p);
  DWORD id = 0;
  GetWindowThreadProcessId(h, &id);
  if (id == s.pid && IsWindowVisible(h) && !GetWindow(h, GW_OWNER)) {
    s.hwnd = h;
    s.count++;
  }
  return TRUE;
}
int wmain(int argc, wchar_t **argv) {
  try {
    if (argc != 4)
      throw std::runtime_error("PID expected-exe output.png required");
    init_apartment(apartment_type::multi_threaded);
    DWORD pid = std::stoul(argv[1]);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    wchar_t path[32768];
    DWORD n = 32768;
    if (!process)
      throw std::runtime_error("Process open failed");
    bool queried = QueryFullProcessImageNameW(process, 0, path, &n) != 0;
    CloseHandle(process);
    if (!queried)
      throw std::runtime_error("Process query failed");
    if (_wcsicmp(path, argv[2]))
      throw std::runtime_error("Owned game path mismatch");
    Search search{pid};
    EnumWindows(Find, reinterpret_cast<LPARAM>(&search));
    if (search.count != 1)
      throw std::runtime_error("Game window is not unique");
    if (GetForegroundWindow()!=search.hwnd)
      throw std::runtime_error("Game is not foreground; desktop capture may be occluded");

    com_ptr<ID3D11Device> device;
    com_ptr<ID3D11DeviceContext> context;
    check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                    D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
                                    0, D3D11_SDK_VERSION, device.put(), nullptr,
                                    context.put()));
    auto dxgi = device.as<IDXGIDevice>();
    com_ptr<IDXGIAdapter> adapter;
    check_hresult(dxgi->GetAdapter(adapter.put()));
    RECT window;
    if (!GetWindowRect(search.hwnd, &window) || window.right <= window.left ||
        window.bottom <= window.top || window.right - window.left > 8192 ||
        window.bottom - window.top > 8192)
      throw std::runtime_error("Invalid game rectangle");
    com_ptr<IDXGIOutput1> output;
    DXGI_OUTPUT_DESC outputDesc{};
    for (UINT index = 0;; index++) {
      com_ptr<IDXGIOutput> candidate;
      auto result = adapter->EnumOutputs(index, candidate.put());
      if (result == DXGI_ERROR_NOT_FOUND)
        break;
      check_hresult(result);
      DXGI_OUTPUT_DESC d;
      check_hresult(candidate->GetDesc(&d));
      auto r = d.DesktopCoordinates;
      if (window.left >= r.left && window.top >= r.top &&
          window.right <= r.right && window.bottom <= r.bottom) {
        if (output)
          throw std::runtime_error("Ambiguous output");
        output = candidate.as<IDXGIOutput1>();
        outputDesc = d;
      }
    }
    if (!output)
      throw std::runtime_error("Game window not contained by one GPU output");
    if (outputDesc.Rotation != DXGI_MODE_ROTATION_IDENTITY)
      throw std::runtime_error("Rotated capture outputs are unsupported");
    com_ptr<IDXGIOutputDuplication> duplicate;
    check_hresult(output->DuplicateOutput(device.get(), duplicate.put()));
    DXGI_OUTDUPL_FRAME_INFO info{};
    com_ptr<IDXGIResource> resource;
    // The initial duplicated image can precede Vulkan's current scanout.
    // Drain two arrivals before copying; keep the whole helper under 12s.
    for (int frame = 0; frame < 3; ++frame) {
      resource = nullptr;
      check_hresult(duplicate->AcquireNextFrame(3000, &info, resource.put()));
      if (frame < 2)
        check_hresult(duplicate->ReleaseFrame());
    }
    if (!info.LastPresentTime.QuadPart || info.ProtectedContentMaskedOut)
      throw std::runtime_error("No unmasked desktop image arrived");
    auto tex = resource.as<ID3D11Texture2D>();
    D3D11_TEXTURE2D_DESC copyDesc;
    tex->GetDesc(&copyDesc);
    copyDesc.Width = window.right - window.left;
    copyDesc.Height = window.bottom - window.top;
    copyDesc.Usage = D3D11_USAGE_STAGING;
    copyDesc.BindFlags = 0;
    copyDesc.MiscFlags = 0;
    copyDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    com_ptr<ID3D11Texture2D> staging;
    check_hresult(device->CreateTexture2D(&copyDesc, nullptr, staging.put()));
    UINT x = window.left - outputDesc.DesktopCoordinates.left,
         y = window.top - outputDesc.DesktopCoordinates.top;
    D3D11_BOX box{x, y, 0, x + copyDesc.Width, y + copyDesc.Height, 1};
    context->CopySubresourceRegion(staging.get(), 0, 0, 0, 0, tex.get(), 0,
                                   &box);
    check_hresult(duplicate->ReleaseFrame());
    if (GetForegroundWindow()!=search.hwnd)
      throw std::runtime_error("Game lost foreground during desktop capture");
    D3D11_TEXTURE2D_DESC desc;
    staging->GetDesc(&desc);
    D3D11_MAPPED_SUBRESOURCE data;
    check_hresult(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &data));
    com_ptr<IWICImagingFactory> wic;
    check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                   CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(wic.put())));
    com_ptr<IWICStream> stream;
    check_hresult(wic->CreateStream(stream.put()));
    check_hresult(stream->InitializeFromFilename(argv[3], GENERIC_WRITE));
    com_ptr<IWICBitmapEncoder> encoder;
    check_hresult(
        wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()));
    check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));
    com_ptr<IWICBitmapFrameEncode> bitmap;
    check_hresult(encoder->CreateNewFrame(bitmap.put(), nullptr));
    check_hresult(bitmap->Initialize(nullptr));
    check_hresult(bitmap->SetSize(desc.Width, desc.Height));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    check_hresult(bitmap->SetPixelFormat(&format));
    check_hresult(bitmap->WritePixels(desc.Height, data.RowPitch,
                                      data.RowPitch * desc.Height,
                                      (BYTE *)data.pData));
    check_hresult(bitmap->Commit());
    check_hresult(encoder->Commit());
    context->Unmap(staging.get(), 0);
    std::cout << "Captured " << desc.Width << "x" << desc.Height
              << " through DXGI desktop duplication\n";
    return 0;
  } catch (hresult_error const &e) {
    std::wcerr << e.message().c_str() << L" HRESULT " << std::hex << e.code()
               << L"\n";
    return 1;
  } catch (std::exception const &e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
