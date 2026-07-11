#include <objbase.h>
#include "resource_loader.h"
#include "resource_ids.h"
#include <gdiplus.h>

using namespace Gdiplus;

void* LoadEmbeddedPng(HINSTANCE hInstance, int resourceId) {
    HRSRC hRes = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (!hRes) return NULL;

    HGLOBAL hGlob = LoadResource(hInstance, hRes);
    if (!hGlob) return NULL;

    const void* pData = LockResource(hGlob);
    if (!pData) return NULL;

    DWORD dwSize = SizeofResource(hInstance, hRes);
    if (dwSize == 0) return NULL;

    IStream* pStream = NULL;
    HGLOBAL hStreamData = GlobalAlloc(GMEM_MOVEABLE, dwSize);
    if (!hStreamData) return NULL;

    void* pStreamWrite = GlobalLock(hStreamData);
    memcpy(pStreamWrite, pData, dwSize);
    GlobalUnlock(hStreamData);

    HRESULT hr = CreateStreamOnHGlobal(hStreamData, TRUE, &pStream);
    if (FAILED(hr) || !pStream) {
        GlobalFree(hStreamData);
        return NULL;
    }

    Image* img = Image::FromStream(pStream);
    pStream->Release();

    return static_cast<void*>(img);
}

HICON LoadEmbeddedIcon(HINSTANCE hInstance, int size) {
    HICON hIcon = static_cast<HICON>(
        LoadImageW(hInstance, MAKEINTRESOURCEW(IDR_MAINICON), IMAGE_ICON, size, size, 0)
    );
    return hIcon;
}
