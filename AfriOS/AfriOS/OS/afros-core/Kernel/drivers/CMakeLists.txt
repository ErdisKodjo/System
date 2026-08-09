# CMakeLists.txt for afros-core drivers layer
#
# Pilotes matériels concrets. Chaque sous-dossier correspond à une famille de
# périphérique (pci/, uart/, ...). Un pilote ne dépend que de hal/include/ —
# jamais directement d'un registre ou d'une adresse physique en dur.

add_library(afros-drivers STATIC
    pci/afros_pci.c
)

target_include_directories(afros-drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(afros-drivers PUBLIC afros-hal)
