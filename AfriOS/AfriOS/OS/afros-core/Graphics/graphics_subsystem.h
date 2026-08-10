/**
 * @file graphics_subsystem.h
 * @brief Sous-système graphique complet pour AfriOS
 * 
 * Fournit une infrastructure graphique native incluant :
 * - Gestion des GPU (Intel, AMD, NVIDIA)
 * - Composition Wayland-compatible
 * - Accélération 2D/3D (OpenGL ES, Vulkan)
 * - Gestion des écrans multiples et HiDPI
 * - Input graphique (souris, clavier, tactile)
 * - Buffer sharing et zero-copy
 */

#ifndef AFROS_GRAPHICS_SUBSYSTEM_H
#define AFROS_GRAPHICS_SUBSYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "afros_types.h"

// ============================================================================
// CONSTANTES ET LIMITES
// ============================================================================

#define MAX_GPUS 4
#define MAX_DISPLAYS 8
#define MAX_SURFACES 1024
#define MAX_PLANES 4
#define MAX_FORMATS 32
#define DRM_FORMAT_ARGB8888 0x34325241
#define DRM_FORMAT_XRGB8888 0x34325258
#define DRM_FORMAT_ABGR8888 0x34324741
#define DRM_FORMAT_XBGR8888 0x34324758

// ============================================================================
// FORMATS DE PIXELS ET BUFFERS
// ============================================================================

typedef enum {
    PIXEL_FORMAT_UNKNOWN = 0,
    PIXEL_FORMAT_RGB565,
    PIXEL_FORMAT_RGB888,
    PIXEL_FORMAT_BGR888,
    PIXEL_FORMAT_ARGB8888,
    PIXEL_FORMAT_ABGR8888,
    PIXEL_FORMAT_XRGB8888,
    PIXEL_FORMAT_XBGR8888,
    PIXEL_FORMAT_ARGB2101010,
    PIXEL_FORMAT_ABGR2101010,
    PIXEL_FORMAT_NV12,      // YUV 4:2:0
    PIXEL_FORMAT_NV21,
    PIXEL_FORMAT_YUYV,
    PIXEL_FORMAT_UYVY
} pixel_format_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    pixel_format_t format;
    uint32_t stride;        // bytes per line
    void* data;             // pointer vers les pixels
    size_t size;            // taille totale en bytes
    bool is_linear;         // layout linéaire vs tiled
    uint64_t modifier;      // format-specific modifier
} framebuffer_t;

typedef struct {
    int fd;                 // DMA-BUF file descriptor
    uint32_t width;
    uint32_t height;
    pixel_format_t format;
    uint64_t modifier;
    uint32_t handles[MAX_PLANES];
    uint32_t strides[MAX_PLANES];
    uint32_t offsets[MAX_PLANES];
    uint64_t size;
    bool imported;          // true si importé d'un autre processus
} dmabuf_t;

// ============================================================================
// GESTION DES GPU
// ============================================================================

typedef enum {
    GPU_VENDOR_INTEL = 0,
    GPU_VENDOR_AMD,
    GPU_VENDOR_NVIDIA,
    GPU_VENDOR_ARM_MALI,
    GPU_VENDOR_QUALCOMM_ADRENO,
    GPU_VENDOR_UNKNOWN
} gpu_vendor_t;

typedef enum {
    GPU_TYPE_INTEGRATED = 0,
    GPU_TYPE_DISCRETE,
    GPU_TYPE_VIRTUAL
} gpu_type_t;

typedef struct {
    char name[64];
    char driver[64];
    char version[32];
    gpu_vendor_t vendor;
    gpu_type_t type;
    uint16_t pci_vendor_id;
    uint16_t pci_device_id;
    uint64_t vram_total;
    uint64_t vram_used;
    uint64_t vram_available;
    bool primary;
    bool modesetting_enabled;
    bool acceleration_3d;
    bool vulkan_supported;
    bool opengl_supported;
    uint32_t opengl_version;
    uint32_t vulkan_version;
    int drm_fd;             // DRM file descriptor
    void* device_context;   // Contexte spécifique au driver
} gpu_info_t;

// ============================================================================
// GESTION DES ÉCRANS (DRM/KMS)
// ============================================================================

typedef enum {
    CONNECTOR_UNKNOWN = 0,
    CONNECTOR_VGA,
    CONNECTOR_DVI_I,
    CONNECTOR_DVI_D,
    CONNECTOR_HDMI_A,
    CONNECTOR_HDMI_B,
    CONNECTOR_DISPLAYPORT,
    CONNECTOR_LVDS,
    CONNECTOR_eDP,
    CONNECTOR_VIRTUAL
} connector_type_t;

typedef enum {
    DPMS_ON = 0,
    DPMS_STANDBY,
    DPMS_SUSPEND,
    DPMS_OFF
} dpms_state_t;

typedef struct {
    uint32_t id;
    connector_type_t type;
    char name[32];
    bool connected;
    bool enabled;
    dpms_state_t dpms;
    uint32_t mm_width;      // dimensions physiques en mm
    uint32_t mm_height;
    uint32_t current_mode_id;
    uint32_t* mode_ids;
    size_t mode_count;
    gpu_info_t* gpu;
} display_connector_t;

typedef struct {
    uint32_t id;
    uint32_t clock;         // kHz
    uint32_t hdisplay;
    uint32_t hsync_start;
    uint32_t hsync_end;
    uint32_t htotal;
    uint32_t vdisplay;
    uint32_t vsync_start;
    uint32_t vsync_end;
    uint32_t vtotal;
    uint32_t flags;
    bool preferred;         // mode préféré de l'écran
    char name[64];
} display_mode_t;

typedef struct {
    uint32_t crtc_id;
    uint32_t connector_id;
    uint32_t mode_id;
    framebuffer_t* framebuffer;
    int32_t x;              // position dans le desktop
    int32_t y;
    uint32_t gamma_size;
    uint16_t* gamma_r;
    uint16_t* gamma_g;
    uint16_t* gamma_b;
    bool active;
} display_crtc_t;

// ============================================================================
// COMPOSITION ET SURFACES
// ============================================================================

typedef enum {
    SURFACE_TYPE_WINDOW = 0,
    SURFACE_TYPE_OVERLAY,
    SURFACE_TYPE_CURSOR,
    SURFACE_TYPE_SPRITE
} surface_type_t;

typedef enum {
    TRANSFORM_NORMAL = 0,
    TRANSFORM_90,
    TRANSFORM_180,
    TRANSFORM_270,
    TRANSFORM_FLIPPED = 4,
    TRANSFORM_FLIPPED_90,
    TRANSFORM_FLIPPED_180,
    TRANSFORM_FLIPPED_270
} surface_transform_t;

typedef struct {
    uint32_t id;
    surface_type_t type;
    dmabuf_t buffer;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    surface_transform_t transform;
    float alpha;            // 0.0 - 1.0
    uint32_t zorder;        // profondeur
    bool visible;
    bool damaged;           // needs recomposition
    void* client_data;      // données du client (wayland surface, etc.)
} composition_surface_t;

typedef struct {
    uint32_t id;
    char name[64];
    composition_surface_t* surfaces[MAX_SURFACES];
    size_t surface_count;
    display_crtc_t* crtc;
    bool hardware_cursor;
    bool vblank_sync;
    uint32_t last_frame_ms;
    uint32_t fps;
} composition_layer_t;

// ============================================================================
// RENDERING 2D/3D
// ============================================================================

typedef enum {
    API_OPENGL_ES2 = 0,
    API_OPENGL_ES3,
    API_OPENGL_4,
    API_VULKAN_1_0,
    API_VULKAN_1_1,
    API_VULKAN_1_2,
    API_VULKAN_1_3,
    API_SOFTWARE
} render_api_t;

typedef struct {
    render_api_t api;
    uint32_t version;
    gpu_info_t* gpu;
    void* context;          // GL context ou VkDevice
    void* surface;          // Surface de rendu
    bool double_buffered;
    bool vsync_enabled;
    uint32_t samples;       // MSAA samples
} render_context_t;

typedef struct {
    float x, y, z, w;
} vertex_t;

typedef struct {
    float r, g, b, a;
} color_t;

typedef struct {
    vertex_t position;
    color_t color;
    float tex_u, tex_v;
} textured_vertex_t;

// ============================================================================
// INPUT GRAPHIQUE
// ============================================================================

typedef enum {
    INPUT_DEVICE_KEYBOARD = 0,
    INPUT_DEVICE_MOUSE,
    INPUT_DEVICE_TOUCHSCREEN,
    INPUT_DEVICE_TABLET,
    INPUT_DEVICE_TRACKPAD,
    INPUT_DEVICE_JOYSTICK
} input_device_type_t;

typedef struct {
    char name[64];
    char phys[64];
    input_device_type_t type;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t version;
    bool enabled;
    uint64_t capabilities;  // bitmask des capacités
    int fd;                 // event device fd
} input_device_t;

typedef enum {
    KEY_EVENT_PRESS = 0,
    KEY_EVENT_RELEASE,
    KEY_EVENT_REPEAT
} key_event_type_t;

typedef struct {
    uint64_t timestamp;
    key_event_type_t type;
    uint32_t keycode;
    uint32_t modifiers;     // Ctrl, Alt, Shift, etc.
    input_device_t* device;
} keyboard_event_t;

typedef struct {
    uint64_t timestamp;
    int32_t dx;
    int32_t dy;
    int32_t dz;             // wheel
    uint8_t buttons;        // bitmask
    input_device_t* device;
} mouse_event_t;

typedef struct {
    uint64_t timestamp;
    int32_t slot_id;
    int32_t x;
    int32_t y;
    uint32_t pressure;
    uint32_t contact_area;
    bool active;
    input_device_t* device;
} touch_event_t;

// ============================================================================
// CURSEURS ET SPRITES
// ============================================================================

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t hotspot_x;
    uint32_t hotspot_y;
    pixel_format_t format;
    uint8_t* pixels;
    size_t size;
} cursor_image_t;

typedef struct {
    cursor_image_t images[8];   // frames pour animation
    size_t image_count;
    uint32_t delay_ms[8];       // délais entre frames
    uint32_t current_frame;
    int32_t x;
    int32_t y;
    bool visible;
    bool animated;
} hardware_cursor_t;

// ============================================================================
// SOUS-SYSTÈME GRAPHIQUE PRINCIPAL
// ============================================================================

typedef struct {
    // GPUs
    gpu_info_t gpus[MAX_GPUS];
    size_t gpu_count;
    gpu_info_t* primary_gpu;
    
    // Displays
    display_connector_t displays[MAX_DISPLAYS];
    size_t display_count;
    display_mode_t* modes;
    size_t mode_count;
    display_crtc_t crtcs[MAX_DISPLAYS];
    size_t crtc_count;
    
    // Composition
    composition_layer_t layers[MAX_DISPLAYS];
    size_t layer_count;
    
    // Rendu
    render_context_t* render_contexts[MAX_GPUS];
    
    // Input
    input_device_t input_devices[32];
    size_t input_device_count;
    
    // Curseurs
    hardware_cursor_t cursors[MAX_DISPLAYS];
    
    // État global
    bool initialized;
    bool master;            // DRM master
    bool atomic_modesetting;
    bool pageflip_enabled;
    bool vblank_callback_enabled;
    uint32_t dpi_scale;     // pour HiDPI
    
    // Callbacks
    void (*vblank_callback)(uint32_t crtc_id, uint64_t timestamp);
    void (*hotplug_callback)(display_connector_t* connector, bool connected);
} graphics_subsystem_t;

// ============================================================================
// API PUBLIQUE
// ============================================================================

/**
 * @brief Initialise le sous-système graphique
 * @param config Configuration initiale
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_init(const graphics_subsystem_t* config);

/**
 * @brief Détecte et initialise les GPUs disponibles
 * @return Nombre de GPUs détectés
 */
size_t graphics_detect_gpus(void);

/**
 * @brief Définit le GPU primaire
 * @param gpu_index Index du GPU
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_set_primary_gpu(size_t gpu_index);

/**
 * @brief Détecte les écrans connectés
 * @return Nombre d'écrans détectés
 */
size_t graphics_detect_displays(void);

/**
 * @brief Configure un mode d'affichage
 * @param connector_id ID du connecteur
 * @param mode_id ID du mode
 * @param crtc_id ID du CRTC
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_set_mode(uint32_t connector_id, uint32_t mode_id,
                                  uint32_t crtc_id);

/**
 * @brief Crée une surface de composition
 * @param type Type de surface
 * @param width Largeur
 * @param height Hauteur
 * @param format Format de pixel
 * @return ID de la surface ou erreur
 */
uint32_t graphics_create_surface(surface_type_t type, uint32_t width,
                                  uint32_t height, pixel_format_t format);

/**
 * @brief Attache un buffer à une surface
 * @param surface_id ID de la surface
 * @param dmabuf Buffer DMA à attacher
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_attach_buffer(uint32_t surface_id, 
                                       const dmabuf_t* dmabuf);

/**
 * @brief Positionne une surface
 * @param surface_id ID de la surface
 * @param x Position X
 * @param y Position Y
 * @param zorder Ordre Z
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_position_surface(uint32_t surface_id, int32_t x,
                                          int32_t y, uint32_t zorder);

/**
 * @brief Compose et affiche toutes les surfaces
 * @param crtc_id ID du CRTC cible
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_composite(uint32_t crtc_id);

/**
 * @brief Crée un contexte de rendu
 * @param api API de rendu souhaitée
 * @param gpu_index Index du GPU
 * @param surface_id Surface de rendu
 * @return Pointeur vers le contexte ou NULL
 */
render_context_t* graphics_create_render_context(render_api_t api,
                                                  size_t gpu_index,
                                                  uint32_t surface_id);

/**
 * @brief Détruit un contexte de rendu
 * @param ctx Contexte à détruire
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_destroy_render_context(render_context_t* ctx);

/**
 * @brief Enregistre un périphérique d'input
 * @param device Périphérique à enregistrer
 * @return Index du périphérique ou erreur
 */
int graphics_register_input_device(const input_device_t* device);

/**
 * @brief Lit les événements d'input
 * @param keyboard_event Événement clavier (sortie)
 * @param mouse_event Événement souris (sortie)
 * @param touch_event Événement tactile (sortie)
 * @return true si événement disponible
 */
bool graphics_poll_input(keyboard_event_t* keyboard_event,
                         mouse_event_t* mouse_event,
                         touch_event_t* touch_event);

/**
 * @brief Définit l'image du curseur matériel
 * @param display_index Index de l'écran
 * @param image Image du curseur
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_set_cursor_image(size_t display_index,
                                          const cursor_image_t* image);

/**
 * @brief Positionne le curseur matériel
 * @param display_index Index de l'écran
 * @param x Position X
 * @param y Position Y
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_move_cursor(size_t display_index, int32_t x, int32_t y);

/**
 * @brief Importe un DMA-BUF depuis un autre processus
 * @param fd File descriptor du buffer
 * @param dmabuf Structure à remplir (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_import_dmabuf(int fd, dmabuf_t* dmabuf);

/**
 * @brief Exporte un DMA-BUF vers un autre processus
 * @param surface_id ID de la surface
 * @return FD du buffer exporté ou erreur
 */
int graphics_export_dmabuf(uint32_t surface_id);

/**
 * @brief Attend la synchronisation verticale
 * @param crtc_id ID du CRTC
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_wait_vblank(uint32_t crtc_id);

/**
 * @brief Active/désactive un écran
 * @param connector_id ID du connecteur
 * @param enabled true pour activer
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_set_display_enabled(uint32_t connector_id, bool enabled);

/**
 * @brief Configure le DPMS d'un écran
 * @param connector_id ID du connecteur
 * @param state État DPMS
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_set_dpms(uint32_t connector_id, dpms_state_t state);

/**
 * @brief Récupère les informations d'un GPU
 * @param gpu_index Index du GPU
 * @param info Structure à remplir (sortie)
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_get_gpu_info(size_t gpu_index, gpu_info_t* info);

/**
 * @brief Récupère les statistiques de performance
 * @param fps FPS actuels (sortie)
 * @param vram_used VRAM utilisée (sortie)
 * @param render_time_ms Temps de rendu moyen (sortie)
 */
void graphics_get_performance_stats(uint32_t* fps, uint64_t* vram_used,
                                     uint32_t* render_time_ms);

/**
 * @brief Nettoie le sous-système graphique
 * @return AFROS_SUCCESS ou code d'erreur
 */
afros_status_t graphics_shutdown(void);

#endif // AFROS_GRAPHICS_SUBSYSTEM_H
