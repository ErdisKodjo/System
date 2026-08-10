import time

class AfrosHAL:
    def __init__(self):
        self.power_source = "SOLAR"
        self.cpu_count = 8
        self.total_memory_mb = 8192 # 8 GB
        self.used_memory_mb = 2048  # Start with 2GB used
        print(f"[SIM] HAL initialis avec {self.total_memory_mb}MB de mmoire totale.")

    def init(self):
        print("[HAL] Initialisation de la couche d'abstraction...")
        print("[HAL] Systeme pret pour le noyau.")
        return True

    def get_power_source(self):
        return self.power_source

    def get_cpu_info(self, cpu_id):
        cluster = 0 if cpu_id < 4 else 1
        is_big = cpu_id >= 4
        freq = 2800 if is_big else 1400
        return {"id": cpu_id, "cluster": cluster, "is_big": is_big, "freq": freq}

    def get_memory_usage_percent(self):
        """Retourne le pourcentage de mmoire utilise."""
        return (self.used_memory_mb / self.total_memory_mb) * 100

    def compress_memory_pages(self, page_count):
        """Simule la compression de pages mmoire pour en librer."""
        # Simulation trs simplifie : chaque page fait 4KB
        mem_freed_mb = (page_count * 4) / 1024
        self.used_memory_mb -= mem_freed_mb
        print(f"[HAL] Compression de {page_count} pages. Mmoire libre: {mem_freed_mb:.2f}MB.")
        return True

class AfrosCFS:
    def __init__(self, hal):
        self.hal = hal

    def init(self):
        print("[KERNEL] Ordonnanceur CFS : Initialisation...")

    def run(self):
        print("[KERNEL] Planificateur en cours d'execution...")
        # Simulation d'une tache
        task_id = 101
        current_cpu = 2
        load = 85
        print(f"[CFS] Analyse Tache {task_id} sur Core {current_cpu} (Charge: {load}%)")

        # Logique de migration big.LITTLE
        info = self.hal.get_cpu_info(current_cpu)
        if not info["is_big"] and load > 80:
            print(f"[CPU] Migration : Tache {task_id} deplacee du Core {current_cpu} (LITTLE) vers Core 4 (big).")

class SolarAware:
    def __init__(self, hal):
        self.hal = hal

    def check_status(self):
        source = self.hal.get_power_source()
        if source == "SOLAR":
            print("[POWER] Energie solaire detectee. Mode Haute Performance ACTIVE.")
        else:
            print("[POWER] Batterie detectee. Mode Basse Consommation.")

class AdaptiveMemoryManager:
    """
    Simulation du gestionnaire de mmoire adaptatif (adaptive_reclaim.c).
    """
    def __init__(self, hal):
        self.hal = hal
        self.threshold_percent = 85

    def check_memory_pressure(self):
        """Vrifie la pression mmoire et dclenche la rcupration si ncessaire."""
        current_usage = self.hal.get_memory_usage_percent()
        print(f"[MEM] Monitorage de la charge mmoire ({current_usage:.1f}%)...")

        if current_usage > self.threshold_percent:
            print(f"[MEM] Alerte : Mmoire faible dtecte (seuil > {self.threshold_percent}%).")
            print("[MEM] Action : Rcupration des pages les moins utilises (LRU).")
            # Simule l'appel  la HAL pour compresser 4096 pages
            self.hal.compress_memory_pages(4096)
        else:
            print("[MEM] tat : Mmoire suffisante. Aucune action requise.")

def kernel_main():
    print("--- Simulation AfriOS Core (Python Test Runner) ---")
    hal = AfrosHAL()
    cfs = AfrosCFS(hal)
    power = SolarAware(hal)
    mem_manager = AdaptiveMemoryManager(hal)

    if hal.init():
        power.check_status()
        cfs.init()
        print("\n[KERNEL] Noyau operationnel.\n")

        # 1. Dmonstration de l'ordonnanceur
        cfs.run()
        # 2. Dmonstration de la gestion mmoire
        print("\n--- Test de gestion mmoire ---")
        hal.used_memory_mb = 7500 # Augmente l'utilisation pour dclencher l'alerte
        mem_manager.check_memory_pressure()

    print("\n--- Test termine avec SUCCES ---")

if __name__ == "__main__":
    kernel_main()
