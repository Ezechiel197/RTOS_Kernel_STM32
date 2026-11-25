"""
@file run_tessla_live.py
@brief Haupt-Orchestrierungsskript zur Echtzeit-Analyse von SystemView-Events mit TeSSLa.
@details Dieses Skript verbindet das Windows-basierte Event-Capturing (`get_rtt_events.py`) 
         mit dem Linux-basierten TeSSLa-Interpreter unter WSL. Es erstellt eine Named Pipe (FIFO) 
         unter WSL, startet TeSSLa als gewarteten Subprozess und speist die Live-Events parallel ein.
@author Ezechiel Tonkeme
@date 2026
"""

import subprocess
import os
import time
import threading

# ==============================================================================
# KONFIGURATION & PFADE
# ==============================================================================

def windows_to_wsl_path(windows_path: str) -> str:
    """
    @brief Konvertiert einen absoluten Windows-Dateipfad in einen WSL-kompatiblen Pfad.
    @details Wandelt Laufwerksbuchstaben und Backslashes um.
             
    @param windows_path Der Pfad im Windows-Format.
    
    @return Der konvertierte Pfad im POSIX/WSL-Format.
    """
    if os.path.isabs(windows_path) and windows_path[1:3] == ':\\':
        drive = windows_path[0].lower()
        path = windows_path[3:].replace('\\', '/')
        return f"/mnt/{drive}/{path}"
    return windows_path


## @brief Absolute Pfadfeile zur TeSSLa Executable JAR auf dem Windows-Dateisystem.
TESSLA_JAR_WINDOWS = r"C:\Users\ezech\OneDrive\Dokumente\Master 2\Echtzeitssystem\Projekt\TeSSLa\tessla-assembly-2.1.0.jar"

## @brief Pfad zur TeSSLa-Spezifikationsdatei (.tessla) auf dem Windows-Dateisystem.
SPEC_FILE_WINDOWS = r"C:\Users\ezech\OneDrive\Dokumente\Master 2\Echtzeitssystem\Projekt\TeSSLa\generated_rules.tessla"

## @brief WSL-kompatibler Pfad zur TeSSLa Executable JAR (`/mnt/...`).
TESSLA_JAR_WSL = windows_to_wsl_path(TESSLA_JAR_WINDOWS)

## @brief WSL-kompatibler Pfad zur TeSSLa Spezifikationsdatei (`/mnt/...`).
SPEC_FILE_WSL = windows_to_wsl_path(SPEC_FILE_WINDOWS)

## @brief Pfad der Named Pipe (FIFO) innerhalb des WSL-Dateisystems (`/tmp/...`).
WSL_FIFO_PATH = "/tmp/tessla_trace.log"

## @brief Pfad zum Python-Skript für das Capturing der J-Link RTT-Events.
RTT_EVENTS_SCRIPT = os.path.join(os.path.dirname(__file__), "get_rtt_events.py")


# ==============================================================================
# MAIN PROZESS-ORCHESTRIERUNG
# ==============================================================================

# WSL FIFO für Live-Kommunikation erstellen
print("Erstelle WSL FIFO für Live-Kommunikation...")
try:
    # Eventuell vorhandene FIFO löschen
    subprocess.run(["wsl", "rm", "-f", WSL_FIFO_PATH], check=False)
    
    # Neue FIFO erstellen
    result = subprocess.run(["wsl", "mkfifo", WSL_FIFO_PATH], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Fehler beim Erstellen der FIFO: {result.stderr}")
        exit(1)
    
    print(f"WSL FIFO erstellt: {WSL_FIFO_PATH}")
except Exception as e:
    print(f"Fehler bei WSL FIFO-Erstellung: {e}")
    exit(1)

# TeSSLa zuerst starten (wartet an der FIFO auf Daten)
print("Starte TeSSLa Interpreter über WSL...")
print(f"TeSSLa liest aus WSL FIFO: {WSL_FIFO_PATH}")

## @brief Subprozess-Handle für den unter WSL laufenden TeSSLa-Interpreter.
tessla_proc = subprocess.Popen(
    ["wsl", "java", "-jar", TESSLA_JAR_WSL, "interpreter", "-t", "1ns", SPEC_FILE_WSL, WSL_FIFO_PATH],
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True
)

# Kurze Pause, damit TeSSLa die FIFO zum Lesen öffnen kann
time.sleep(1)

# RTT Events Script starten (schreibt in WSL FIFO)
print("Starte RTT Events Script...")

## @brief Subprozess-Handle für das auf Windows laufende RTT-Event-Capturing-Skript.
rtt_events_proc = subprocess.Popen(
    ["python", RTT_EVENTS_SCRIPT, WSL_FIFO_PATH],
    cwd=os.path.dirname(__file__)
)
print(f"RTT Events Script gestartet (PID: {rtt_events_proc.pid})")

# Sofort prüfen ob TeSSLa noch läuft
tessla_return_code = tessla_proc.poll()
if tessla_return_code is not None:
    print(f"FEHLER: TeSSLa wurde sofort beendet mit Code {tessla_return_code}")
    stdout, _ = tessla_proc.communicate()
    print("TeSSLa Ausgabe:")
    print(stdout)
    exit(1)

print("TeSSLa und RTT Events laufen.")


def print_tessla_output() -> None:
    """
    @brief Liest und formatiert die Konsolenausgabe des TeSSLa-Interpreters in einem separaten Thread.
    @details Erfasst `stdout` fortlaufend Zeile für Zeile und druckt verifizierte TeSSLa-Signale 
             mit dem Präfix `TeSSLa:` aus. Verhindert das Blockieren des Haupt-Threads.
    """
    while True:
        try:
            output = tessla_proc.stdout.readline()
            if output:
                print(f"TeSSLa: {output.strip()}")
            elif tessla_proc.poll() is not None:
                # TeSSLa wurde beendet
                remaining_output = tessla_proc.stdout.read()
                if remaining_output:
                    print(f"TeSSLa (final): {remaining_output.strip()}")
                print(f"TeSSLa wurde beendet mit Code: {tessla_proc.returncode}")
                break
        except Exception as e:
            print(f"Fehler beim Lesen der TeSSLa-Ausgabe: {e}")
            break

## @brief Hintergrund-Thread zum fortlaufenden Lesen und Ausgeben des TeSSLa-Interpreters.
output_thread = threading.Thread(target=print_tessla_output, daemon=True)
output_thread.start()

print("Drücke Ctrl+C zum Beenden.")

# Main Loop & Prozess-Überwachung
try:
    while True:
        time.sleep(1)
        
        # Prüfen ob TeSSLa noch läuft
        if tessla_proc.poll() is not None:
            print(f"TeSSLa wurde beendet mit Return Code: {tessla_proc.returncode}")
            break
            
        # Prüfen ob RTT Events Script noch läuft
        if rtt_events_proc.poll() is not None:
            print(f"RTT Events Script wurde beendet mit Return Code: {rtt_events_proc.returncode}")
            break
            
except KeyboardInterrupt:
    print("\nBeende Prozesse...")
finally:
    print("Aufräumen...")
    
    # RTT Events Script beenden
    if 'rtt_events_proc' in locals() and rtt_events_proc:
        print("Beende RTT Events Script...")
        rtt_events_proc.terminate()
        rtt_events_proc.wait()
    
    # TeSSLa beenden falls noch aktiv
    if tessla_proc and tessla_proc.poll() is None:
        print("Beende TeSSLa...")
        tessla_proc.terminate()
        tessla_proc.wait()
    
    # WSL FIFO entfernen
    try:
        subprocess.run(["wsl", "rm", "-f", WSL_FIFO_PATH], check=False)
        print(f"WSL FIFO entfernt: {WSL_FIFO_PATH}")
    except Exception as e:
        print(f"Fehler beim Entfernen der WSL FIFO: {e}")
    
    print("Cleanup abgeschlossen.")

print("Alle Prozesse beendet.")