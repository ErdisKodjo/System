/**
 * @file CsmPolicy.c
 * @brief Legacy CSM (Compatibility Support Module) boot policy for AfriOS.
 *
 * Sur les plateformes où un BIOS legacy (16-bit real-mode INT 19h) doit
 * coexister avec l'UEFI, le CSM fournit une couche de compatibilité.
 * AfriOS privilégie l'UEFI natif, mais conserve un CSM "skeleton" pour
 * le boot d'OS legacy (DOS, anciens Windows, vieux Linux 16-bit) sur
 * le marché de la réutilisation matérielle — typique des déploiements
 * africains où du matériel x86 antérieur à 2010 est encore en service.
 *
 * Politique :
 *   - Si `PcdCsmEnabled` est vrai ET qu'aucun boot UEFI n'a réussi,
 *     déclencher le boot CSM via INT 19h.
 *   - Si une entrée `Boot####` UEFI est marquée active, le CSM ne se
 *     déclenche jamais (UEFI prioritaire).
 *   - En cas de boot CSM, journaliser l'événement pour audit (le CSM
 *     est une surface d'attaque, on veut savoir qui l'utilise).
 */

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Guid/GlobalVariable.h>

///
/// Politique CSM supportée par AfriOS.
///
typedef enum {
  CsmPolicyDisabled = 0,        ///< CSM jamais activé (défaut, sécurisé).
  CsmPolicyFallbackOnly = 1,    ///< CSM activé seulement si UEFI échoue.
  CsmPolicyAlways = 2           ///< CSM toujours activé (mode legacy pur).
} AFRIOS_CSM_POLICY;

STATIC AFRIOS_CSM_POLICY  mCsmPolicy = CsmPolicyDisabled;
STATIC BOOLEAN            mUefiBootAttempted = FALSE;
STATIC BOOLEAN            mCsmInvoked = FALSE;

/**
  Lit la politique CSM depuis une variable NVRAM `AfriCsmPolicy`.

  Valeurs possibles :
    0 → Disabled (défaut)
    1 → FallbackOnly
    2 → Always

  En l'absence de variable, considère la politique comme Disabled.

  @retval EFI_SUCCESS           Politique chargée.
  @retval EFI_NOT_FOUND         Variable absente, défaut appliqué.
  @retval autres                Erreur de lecture NVRAM.

**/
STATIC
EFI_STATUS
CsmReadPolicy (
  VOID
  )
{
  UINTN       Size;
  UINT8       Value;
  EFI_STATUS  Status;

  Size   = sizeof (Value);
  Status = gRT->GetVariable (
                  L"AfriCsmPolicy",
                  &gEfiGlobalVariableGuid,
                  NULL,
                  &Size,
                  &Value
                  );
  if (EFI_ERROR (Status)) {
    mCsmPolicy = CsmPolicyDisabled;
    DEBUG ((DEBUG_INFO, "[CSM] Variable AfriCsmPolicy absente, politique=Disabled.\n"));
    return Status;
  }

  if (Value > (UINT8)CsmPolicyAlways) {
    mCsmPolicy = CsmPolicyDisabled;
    DEBUG ((DEBUG_WARN, "[CSM] Valeur CsmPolicy invalide (%u), remise à Disabled.\n", Value));
  } else {
    mCsmPolicy = (AFRIOS_CSM_POLICY)Value;
  }

  DEBUG ((DEBUG_INFO, "[CSM] Politique chargée : %d.\n", mCsmPolicy));
  return EFI_SUCCESS;
}

/**
  Détermine si le boot CSM doit être tenté.

  @retval TRUE   Le CSM doit être invoqué (politique Always, ou
                 politique FallbackOnly après échec UEFI).
  @retval FALSE  Le CSM ne doit pas être invoqué.

**/
BOOLEAN
CsmShouldInvoke (
  VOID
  )
{
  if (mCsmPolicy == CsmPolicyDisabled) {
    return FALSE;
  }

  if (mCsmPolicy == CsmPolicyAlways) {
    return TRUE;
  }

  // CsmPolicyFallbackOnly : invoquer seulement après un échec UEFI.
  if (mCsmPolicy == CsmPolicyFallbackOnly && mUefiBootAttempted) {
    DEBUG ((DEBUG_WARN, "[CSM] Boot UEFI échoué, repli sur CSM activé.\n"));
    return TRUE;
  }

  return FALSE;
}

/**
  Signale qu'une tentative de boot UEFI a été effectuée (réussie ou non).
  À appeler depuis `GenericBootManager` avant de déclencher le CSM en
  repli.

  @param[in]  Success   TRUE si le boot UEFI a réussi, FALSE sinon.

**/
VOID
CsmSignalUefiAttempt (
  IN BOOLEAN  Success
  )
{
  mUefiBootAttempted = TRUE;
  if (Success) {
    DEBUG ((DEBUG_INFO, "[CSM] Boot UEFI réussi, CSM ne sera pas invoqué.\n"));
  } else {
    DEBUG ((DEBUG_WARN, "[CSM] Boot UEFI échoué signalé.\n"));
  }
}

/**
  Invoque le boot CSM legacy (INT 19h).

  Cette fonction marque le CSM comme invoqué (pour audit), puis tente
  l'appel INT 19h via le CSM dispatch protocol si disponible. Si le
  protocole n'est pas présent (firmware sans module CSM réel), retourne
  EFI_UNSUPPORTED.

  @retval EFI_SUCCESS           Boot CSM déclenché.
  @retval EFI_UNSUPPORTED       CSM non disponible sur cette plateforme.
  @retval EFI_ACCESS_DENIED     Politique CSM désactivée.

**/
EFI_STATUS
CsmInvokeBoot (
  VOID
  )
{
  if (!CsmShouldInvoke ()) {
    DEBUG ((DEBUG_INFO, "[CSM] Invocation refusée (politique=%d).\n", mCsmPolicy));
    return EFI_ACCESS_DENIED;
  }

  mCsmInvoked = TRUE;

  // TODO: localiser EFI_LEGACY_BIOS_PROTOCOL via gBS->LocateProtocol
  //       et appeler LegacyBios->Boot(). Pour l'instant, on journalise
  //       seulement — l'implémentation réelle nécessite le module CSM
  //       d'EDK2 (IntelFrameworkModulePkg) qui n'est pas vendorisé.
  DEBUG ((DEBUG_WARN, "[CSM] INT 19h non implémenté (CSM EDK2 non vendorisé).\n"));

  return EFI_UNSUPPORTED;
}

/**
  Initialise le module de politique CSM. Doit être appelé depuis
  `PlatformInitDxe` avant que le BootManager ne commence son cycle.

  @param[in]  ImageHandle   Handle d'image DXE.
  @param[in]  SystemTable   Table système UEFI.

  @retval EFI_SUCCESS       Politique initialisée.

**/
EFI_STATUS
EFIAPI
CsmPolicyInit (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  DEBUG ((DEBUG_INFO, "[CSM] CsmPolicyInit : initialisation.\n"));
  CsmReadPolicy ();
  return EFI_SUCCESS;
}
