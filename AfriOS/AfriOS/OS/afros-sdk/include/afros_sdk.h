#ifndef AFROS_SDK_H
#define AFROS_SDK_H

#include "../../afros-core/Kernel/hal/include/afros_types.h"

/**
 * @file afros_sdk.h
 * @brief SDK officiel pour le développement d'applications AfriOS.
 */

// Fonctions d'interface utilisateur (UI)
void afros_ui_create_window(const char *title, uint32_t width, uint32_t height);
void afros_ui_draw_text(const char *text, int x, int y);

// Fonctions système
afros_status_t afros_sys_get_power_info(afros_power_source_t *source);

#endif
