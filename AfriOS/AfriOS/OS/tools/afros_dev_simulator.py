import time
import json

class AfrosHAL:
    def __init__(self):
        self.power_source = "SOLAR"
        self.battery_level = 95
        self.cpu_count = 8
        self.total_memory_mb = 8192
        self.used_memory_mb = 2048
        print(f"[SIM-HAL] Initialis : {self.total_memory_mb}MB RAM, {self.cpu_count} Cores.")

    def init(self):
        return True

    def get_power_info(self):
        return {"source": self.power_source, "level": self.battery_level}

class AfrosOrchestrator:
    def __init__(self):
        self.runtimes = ["Native", "Android", "WinBridge", "Apple", "Harmony"]
        print(f"[SIM-ORCH] Orchestrateur prêt. Runtimes supports : {', '.join(self.runtimes)}")

    def load_package(self, pkg_path):
        print(f"[SIM-ORCH] Analyse du paquet : {pkg_path}")
        # Simulation de détection de type
        if pkg_path.endswith(".exe"):
            return "WinBridge"
        elif pkg_path.endswith(".apk"):
            return "Android"
        elif pkg_path.endswith(".macho"):
            return "Apple"
        else:
            return "Native"

    def run_app(self, pkg_path):
        runtime = self.load_package(pkg_path)
        print(f"[SIM-ORCH] Lancement de {pkg_path} via le runtime {runtime}...")
        time.sleep(0.5)
        print(f"[SIM-{runtime.upper()}] Application dmarre avec succs.")

class AfrosNetworkManager:
    def __init__(self):
        self.interfaces = ["WiFi", "Mobile", "Satellite"]

    def send_packet(self, data_size, interface="WiFi"):
        print(f"[SIM-NET] Envoi de {data_size} octets via {interface}...")
        if interface == "Satellite":
            print("[SIM-NET] Optimisation haute latence active.")

def simulate_dev_workflow():
    print("=== AfriOS Development Workflow Simulation ===")
    hal = AfrosHAL()
    orch = AfrosOrchestrator()
    net = AfrosNetworkManager()

    # 1. Simulation d'un paquet Windows
    print("\n--- Test 1 : Application Windows ---")
    orch.run_app("Notepad.exe")

    # 2. Simulation d'un paquet Android
    print("\n--- Test 2 : Application Android ---")
    orch.run_app("Calculator.apk")

    # 3. Simulation Réseau
    print("\n--- Test 3 : Networking ---")
    net.send_packet(1024, "Satellite")

    print("\n=== Fin de la simulation ===")

if __name__ == "__main__":
    simulate_dev_workflow()
