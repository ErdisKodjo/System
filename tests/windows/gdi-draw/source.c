/*
 * gdi-draw/source.c — Test Windows #3 : dessin GDI.
 *
 * Obtient un DC pour l'écran, dessine un rectangle, libère le DC.
 * Valide que afros-dxvk / GDI est fonctionnel via le pipeline
 * Wine + DXVK + Vulkan.
 */
#include <stdio.h>
#include "win32-stubs.h"
#ifdef _WIN32
#include <windows.h>
#endif

int main(void) {
    HDC hdc = GetDC(NULL);
    if (hdc == NULL) {
        printf("FAIL: GetDC(NULL) returned NULL\n");
        ExitProcess(1);
    }

    HBRUSH brush = CreateSolidBrush(RGB(255, 0, 0));
    if (brush == NULL) {
        printf("FAIL: CreateSolidBrush\n");
        ReleaseDC(NULL, hdc);
        ExitProcess(1);
    }

    /* Sélection du brush et dessin d'un rectangle. */
    HBRUSH old = (HBRUSH)SelectObject(hdc, brush);
    if (!Rectangle(hdc, 10, 10, 100, 100)) {
        printf("FAIL: Rectangle\n");
        SelectObject(hdc, old);
        DeleteObject(brush);
        ReleaseDC(NULL, hdc);
        ExitProcess(1);
    }

    SelectObject(hdc, old);
    DeleteObject(brush);
    ReleaseDC(NULL, hdc);

    printf("GDI rectangle drawn\n");
    ExitProcess(0);
    return 0;
}
