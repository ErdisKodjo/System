# Étape 5 (Vérification) : ce fichier n'existait pas alors que le
# CMakeLists.txt racine appelle add_subdirectory(test) dès que
# AFROS_BUILD_TESTS=ON (vrai par défaut) — la configuration CMake échouait
# donc systématiquement avant ce correctif.
#
# Chaque module possède ses propres tests, au plus près de son code (voir
# afros-core/Kernel/hal/tests/ pour les tests unitaires HAL, branchés via
# Kernel/CMakeLists.txt). Ce répertoire racine reste le point d'ancrage pour
# de futurs tests d'intégration inter-modules ; rien à construire ici tant
# qu'aucun n'existe.
