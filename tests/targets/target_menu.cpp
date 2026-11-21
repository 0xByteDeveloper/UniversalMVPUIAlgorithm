// ============================================================================
// tests/targets/target_menu.cpp — TEST HEDEFİ
// Gerçek Dear ImGui menüsü ("MVP" tarzı oyun overlay'i):
//   * Form1 "XXGOD MVP | Aimbot": checkbox/slider/combo/coledit/inputtext/buttons
//   * Form2 "Visuals": checkbox'lar + slider + büyük buton
//   * Form3 "Misc": tab bar + child pencere
// Backend: DX11 veya DX9 (derleme bayrağıyla seçilir).
// Menü frame 1'den itibaren AÇIK kalır (canlı heap analizinin doğrulanması için).
// ============================================================================
#ifdef UMVP_BACKEND_DX11
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi.h>
#else
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx9.h"
#include <d3d9.h>
#endif
#include <windows.h>
#include <cstdio>

// ---- menü durumu (handler'ların gerçek kod üretmesi için) ------------------
static bool g_aimbot = false;
static bool g_radar = false;
static bool g_esp = false, g_box = false, g_health = false;
static float g_fov = 90.0f, g_smoothing = 4.0f, g_alpha = 0.85f;
static int g_hitbox = 0;  // 0=Head 1=Neck 2=Chest
static float g_color[4] = {1.0f, 0.2f, 0.3f, 1.0f};
static char g_config_name[128] = "default.cfg";
static bool g_running = true;

static FILE* g_log = nullptr;
static void Log(const char* s) {
    if (g_log) { fprintf(g_log, "%s\n", s); fflush(g_log); }
}

// ---- DX ortak ---------------------------------------------------------------
#ifdef UMVP_BACKEND_DX11
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}
static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    D3D_FEATURE_LEVEL fls[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               fls, 2, D3D11_SDK_VERSION, &sd,
                                               &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext);
    if (FAILED(hr))  // Wine/llvmpipe: WARP dene
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                           fls, 2, D3D11_SDK_VERSION, &sd,
                                           &g_pSwapChain, &g_pd3dDevice, &fl, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}
static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}
#else
static IDirect3D9* g_pD3D = nullptr;
static IDirect3DDevice9* g_pd3dDevice = nullptr;
static bool CreateDeviceD3D(HWND hWnd) {
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_pD3D) return false;
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice)))
        if (FAILED(g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice)))
            return false;
    return true;
}
static void CleanupDeviceD3D() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}
#endif

// ---- MENÜ (analyzer'ın çözmesi gereken kod) --------------------------------
static void RenderMenu() {
    // Form 1 — "XXGOD MVP | Aimbot"
    ImGui::SetNextWindowSize(ImVec2(430, 460), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("XXGOD MVP | Aimbot", nullptr, 0)) {
        ImGui::Checkbox("Aimbot Enabled", &g_aimbot);
        if (g_aimbot) {
            ImGui::SliderFloat("FOV", &g_fov, 1.0f, 180.0f, "%.1f");
            ImGui::SliderFloat("Smoothing", &g_smoothing, 0.0f, 10.0f);
            ImGui::Combo("Hitbox", &g_hitbox, "Head\0Neck\0Chest\0\0");
            ImGui::Checkbox("Radar", &g_radar);
        }
        ImGui::ColorEdit4("Player Color", g_color);
        ImGui::InputText("Config Name", g_config_name, IM_ARRAYSIZE(g_config_name));
        if (ImGui::Button("Save Config", ImVec2(120, 28))) {
            Log("SAVE pressed");
            g_fov = 90.0f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Config", ImVec2(120, 28))) {
            Log("LOAD pressed");
            g_hitbox = 0;
        }
        if (ImGui::Button("Panic")) {
            Log("PANIC pressed");
            g_aimbot = g_esp = g_box = g_health = false;
        }
    }
    ImGui::End();

    // Form 2 — "Visuals"
    ImGui::SetNextWindowPos(ImVec2(600, 120), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 330), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Visuals", nullptr, 0)) {
        ImGui::Checkbox("ESP", &g_esp);
        ImGui::Checkbox("Box ESP", &g_box);
        ImGui::Checkbox("Health Bar", &g_health);
        ImGui::SliderFloat("ESP Alpha", &g_alpha, 0.0f, 1.0f, "%.2f");
        if (ImGui::Button("Unload", ImVec2(200, 30))) {
            Log("UNLOAD pressed");
        }
    }
    ImGui::End();

    // Form 3 — "Misc" (tab bar + child)
    ImGui::SetNextWindowPos(ImVec2(950, 120), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Misc", nullptr, 0)) {
        if (ImGui::BeginTabBar("#Tabs")) {
            if (ImGui::BeginTabItem("Info")) {
                ImGui::Text("UMVP test target");
                ImGui::Text("backend: "
#ifdef UMVP_BACKEND_DX11
                            "dx11"
#else
                            "dx9"
#endif
                );
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tools")) {
                ImGui::BeginChild("ChildL", ImVec2(240, 120), true);
                ImGui::Text("child icerik");
                if (ImGui::SmallButton("Refresh")) { Log("REFRESH"); }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        if (ImGui::Button("Apply Settings", ImVec2(150, 26))) {
            Log("APPLY pressed");
        }
    }
    ImGui::End();
}

// ---- Win32 iskeleti ---------------------------------------------------------
static const wchar_t* kClassName = L"UMVP Target Window";

// imgui_impl_win32.h bilinçli olarak bunu #if 0 içinde tutar — kendimiz forward declare ediyoruz
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return 1;
    switch (msg) {
        case WM_SIZE: return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    g_log = fopen("umvp_target.log", "w");
    Log("target started");

    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInst, nullptr, nullptr, nullptr, nullptr, kClassName, nullptr};
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0L, kClassName, L"UMVP Analysis Target", WS_OVERLAPPEDWINDOW,
                                100, 100, 1280, 800, nullptr, nullptr, hInst, nullptr);
    if (!CreateDeviceD3D(hwnd)) {
        Log("FATAL: D3D device olusturulamadi");
        MessageBoxW(nullptr, L"D3D device fail", L"UMVP", MB_OK);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
#ifdef UMVP_BACKEND_DX11
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
#else
    ImGui_ImplDX9_Init(g_pd3dDevice);
#endif

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done || !g_running) break;

#ifdef UMVP_BACKEND_DX11
        ImGui_ImplDX11_NewFrame();
#else
        ImGui_ImplDX9_NewFrame();
#endif
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        RenderMenu();
        ImGui::Render();

#ifdef UMVP_BACKEND_DX11
        const float clear[4] = {0.06f, 0.06f, 0.08f, 1.0f};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
#else
        g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                            D3DCOLOR_RGBA(15, 15, 20, 255), 1.0f, 0);
        if (g_pd3dDevice->BeginScene() >= 0) {
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            g_pd3dDevice->EndScene();
        }
        g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
#endif
    }

#ifdef UMVP_BACKEND_DX11
    ImGui_ImplDX11_Shutdown();
#else
    ImGui_ImplDX9_Shutdown();
#endif
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(kClassName, hInst);
    if (g_log) fclose(g_log);
    return 0;
}
