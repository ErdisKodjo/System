/*
 * com-basic/source.cpp — Test Windows #5 : instantiation COM.
 *
 * CoInitialize, CoCreateInstance(CLSID_XMLDocument), vérifie qu'on
 * récupère une interface non-NULL, Release, CoUninitialize.
 * Valide le runtime COM de afros-winbridge.
 */
#include <stdio.h>
#include <windows.h>
#include <objbase.h>

/* CLSID_XMLDocument — on utilise CoCreateInstance avec le CLSID connu
 * de MSXML2::DOMDocument ({F6D90F11-9C73-11D3-B32E-00C04F990BB4}). */
static const CLSID CLSID_DOMDocument =
    { 0xF6D90F11, 0x9C73, 0x11D3,
      { 0xB3, 0x2E, 0x00, 0xC0, 0x4F, 0x99, 0x0B, 0xB4 } };

static const IID IID_IXMLDOMDocument =
    { 0x2933BF81, 0x7B36, 0x11D2,
      { 0xB2, 0x0E, 0x00, 0xC0, 0x4F, 0x98, 0x3E, 0x60 } };

int main(void) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        printf("FAIL: CoInitializeEx hr=0x%08lx\n", (unsigned long)hr);
        return 1;
    }

    IUnknown *pUnk = NULL;
    hr = CoCreateInstance(CLSID_DOMDocument, NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_IXMLDOMDocument,
                          (void **)&pUnk);
    if (FAILED(hr) || pUnk == NULL) {
        printf("FAIL: CoCreateInstance hr=0x%08lx\n", (unsigned long)hr);
        CoUninitialize();
        return 1;
    }

    /* Si on arrive ici, COM a instancié un objet — c'est le critère. */
    printf("Hello, AfriOS!\n");

    pUnk->lpVtbl->Release(pUnk);
    CoUninitialize();
    return 0;
}
