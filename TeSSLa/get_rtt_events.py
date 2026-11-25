"""
@file get_rtt_events.py
@brief Brücken-Skript zur Erfassung und Konvertierung von Segger SystemView RTT-Events für TeSSLa.
@details Liest SystemView-Events live über den J-Link Telnet-Server (RTT) aus, decodiert die 
         datenvariable Paketstruktur, generiert streng monoton steigende absolute Zeitstempel 
         und leitet das Format direkt an eine Trace-Datei oder einen WSL-FIFO-Pipe für TeSSLa weiter.
@author Ezechiel Tonkeme
@date 2026
"""

import socket
import sys
import time

# ==============================================================================
# KONFIGURATION & GLOBALE VARIABLEN
# ==============================================================================

## @brief Hostname des J-Link Telnet-Servers.
JLINK_HOST = "localhost"

## @brief Standard-Port für Segger SystemView RTT Channels über Telnet.
JLINK_PORT = 19021

## @brief Puffergröße für den Socket-Empfang in Bytes.
BUFFER_SIZE = 4096

## @brief Schalter zum Aktivieren/Deaktivieren ausführlicher Debug-Ausgaben auf stdout.
DEBUG_OUTPUT = False

## @brief Skalierungsfaktor zur Skalierung der empfangenen SystemView-Zeitstempel.
TIMESTAMP_DIVISOR = 1

## @brief Speichert den Zeitstempel des zuletzt an TeSSLa gesendeten Events (zur Wahrung der strikten Monotonie).
last_tessla_timestamp = -1

## @brief Dateipfad zur Ausgabedatei bzw. FIFO-Pipe (wird via Kommandozeilenargument gesetzt).
trace_file_path = None

## @brief Dateihandle oder Subprocess-Handle für die Ausgabedatei/Pipe.
trace_file_handle = None

## @brief Akkumulierter absoluter Zeitstempel über den gesamten Laufzeit-Stream.
absolute_timestamp = 0

## @brief Mapping von SystemView Packet-IDs und Marker-IDs auf eindeutige TeSSLa-Event-Namen.
SYSTEMVIEW_EVENTS = {
    10: "SYSVIEW_MARKER_I2C_WAIT",
    11: "SYSVIEW_MARKER_UART_TX",
    32: "SYSVIEW_EV_MUTEX_TAKE",
    33: "SYSVIEW_EV_MUTEX_GIVE",
    34: "SYSVIEW_EV_SEMAPHORE_WAIT",
    35: "SYSVIEW_EV_SEMAPHORE_POST",
    40: "SYSVIEW_EV_QUEUE_SEND",
    41: "SYSVIEW_EV_QUEUE_RECEIVE"
}


# ==============================================================================
# FUNKTIONSIMPLEMENTIERUNGEN
# ==============================================================================

def open_trace_file() -> bool:
    """
    @brief Öffnet die Trace-Datei oder baut eine WSL-Bridge für FIFO-Pipes auf.
    @details Erkennt anhand des Pfads (`/tmp/` oder `/mnt/`), ob es sich um eine Linux-FIFO-Pipe
             unter WSL handelt. Öffnet im WSL-Fall einen `subprocess` via `wsl sh -c`, um Daten 
             ohne Blockieren direkt einzuspeisen. Öffnet andernfalls eine reguläre Datei.
    
    @return `True` bei erfolgreichem Öffnen, andernfalls `False`.
    """
    global trace_file_handle
    
    if trace_file_path is None:
        print("FEHLER: Trace-Datei-Pfad nicht gesetzt!")
        return False
        
    try:
        # Prüfen ob es sich um einen WSL FIFO-Pfad handelt
        if trace_file_path.startswith('/tmp/') or trace_file_path.startswith('/mnt/'):
            debug_print(f"Erkenne WSL FIFO-Pfad: {trace_file_path}")
            print(f"Öffne WSL FIFO über WSL-Bridge: {trace_file_path}")
            
            import subprocess
            trace_file_handle = subprocess.Popen(
                ["wsl", "sh", "-c", f"cat > {trace_file_path}"],
                stdin=subprocess.PIPE,
                text=True
            )
            debug_print(f"WSL FIFO-Bridge geöffnet: {trace_file_path}")
            print(f"WSL FIFO-Bridge aktiv: {trace_file_path}")
            return True
        else:
            # Normale Windows-Datei
            trace_file_handle = open(trace_file_path, 'w', encoding='utf-8')
            debug_print(f"Trace-Datei geöffnet und wird offen gehalten: {trace_file_path}")
            print(f"Trace-Datei geöffnet: {trace_file_path}")
            return True
    except Exception as e:
        debug_print(f"Fehler beim Öffnen der Trace-Datei: {e}")
        return False


def close_trace_file() -> None:
    """
    @brief Schließt das Dateihandle bzw. beendet den WSL-Brückenprozess.
    @details Sendet damit das EOF-Signal (End of File) an TeSSLa, um die Analyse 
             sauber zu beenden.
    """
    global trace_file_handle
    if trace_file_handle:
        try:
            if hasattr(trace_file_handle, 'stdin'):
                trace_file_handle.stdin.close()
                trace_file_handle.wait()
                debug_print("WSL FIFO-Bridge geschlossen - EOF gesendet")
                print("WSL FIFO-Bridge geschlossen")
            else:
                trace_file_handle.close()
                debug_print("Trace-Datei geschlossen - EOF gesendet")
                print("Trace-Datei geschlossen")
        except Exception as e:
            debug_print(f"Fehler beim Schließen der Trace-Datei: {e}")
        trace_file_handle = None


def process_events_with_absolute_timestamp(parsed_packets: list) -> None:
    """
    @brief Errechnet absolute Zeitstempel und schreibt TeSSLa-konforme Event-Zeilen.
    @details Akkumuliert die relativen `TimeStampDelta`-Werte der Pakete. Um Fehlentscheidungen
             in TeSSLa zu vermeiden, wird eine strikte Monotonie erzwungen (`scaled_timestamp > last_tessla_timestamp`).
             Formatiert die Ausgabe nach dem Schema: `<timestamp>: events = "<event_name>"`.

    @param parsed_packets Liste von geparsten SystemView-Paket-Dictionaries.
    """
    global absolute_timestamp
    global last_tessla_timestamp
    
    for packet in parsed_packets:
        absolute_timestamp += packet['TimeStampDelta']
        
        if 'EventName' in packet:
            event_name = packet['EventName']
        else:
            event_name = f"Packet_0x{packet['ID']:02X}"
        
        scaled_timestamp = absolute_timestamp // TIMESTAMP_DIVISOR
        
        # Verhindern von doppelten Zeitstempeln (Strict Monotonicity)
        if scaled_timestamp <= last_tessla_timestamp:
            scaled_timestamp = last_tessla_timestamp + 1
            
        last_tessla_timestamp = scaled_timestamp
        
        tessla_output_line = f'{scaled_timestamp}: events = "{event_name}"\n'
        
        if trace_file_handle:
            try:
                if hasattr(trace_file_handle, 'stdin'):
                    trace_file_handle.stdin.write(tessla_output_line)
                    trace_file_handle.stdin.flush()
                    debug_print(f'Geschrieben in WSL FIFO: {tessla_output_line.strip()}')
                else:
                    trace_file_handle.write(tessla_output_line)
                    trace_file_handle.flush()
                    debug_print(f'Geschrieben in Trace-Datei: {tessla_output_line.strip()}')
            except Exception as e:
                debug_print(f"Fehler beim Schreiben in Trace-Datei: {e}")
                print(f"{scaled_timestamp}: {event_name}")
        else:
            print(f"{scaled_timestamp}: {event_name}")


def decode_varlen_int(data: bytes, offset: int = 0) -> tuple:
    """
    @brief Decodiert ein Ganzzahl-Feld variabler Länge (Variable-Length Integer / LEB128).
    @details Segger SystemView verwendet zur Bandbreitenoptimierung 7-Bit-Eingaben mit MSB-Continuation-Bit.

    @param data Byte-Array mit den Rohdaten.
    @param offset Startindex für das Decodieren innerhalb des Byte-Arrays.
    
    @return Ein Tupel bestehend aus `(decodierter_wert, anzahl_verbrauchte_bytes)`.
    """
    if offset >= len(data):
        return 0, 0
        
    result = 0
    pos = offset
    bytes_consumed = 0
    shift = 0
    
    while pos < len(data):
        byte = data[pos]
        data_bits = byte & 0x7F
        result |= (data_bits << shift)
        shift += 7
        pos += 1
        bytes_consumed += 1
        
        if (byte & 0x80) == 0:
            break
            
    return result, bytes_consumed


def debug_print(message: str) -> None:
    """
    @brief Gibt Debug-Meldungen auf der Konsole aus, sofern `DEBUG_OUTPUT` auf `True` steht.

    @param message Die auszugebende Nachricht.
    """
    if DEBUG_OUTPUT:
        print(message)


def parse_systemview_packets(data: bytes) -> list:
    """
    @brief Parst rohe SystemView-Byte-Streams in strukturierte Pakete.
    @details Extrahiert Packet-ID, Nutzdatenlänge, Payload und Zeitstempel-Deltas.
             Unterstützt reguläre OS-Events, Marker-Events (ID 114) sowie benutzerdefinierte IDs.

    @param data Der vom RTT-Socket empfangene Byte-Puffer.
    
    @return Eine Liste von Dictionaries, welche die geparsten Daten und zugeordneten Eventnamen enthalten.
    """
    packets = []
    i = 0
    
    allowed_ids = [129, 130, 114] + list(SYSTEMVIEW_EVENTS.keys())
    
    while i < len(data):
        start_pos = i
        
        if i >= len(data):
            break
            
        pkt_id, id_bytes = decode_varlen_int(data, i)
        if id_bytes == 0:
            break
        i += id_bytes
        
        debug_print(f"Debug: Found packet ID 0x{pkt_id:02X} at position {start_pos}, consumed {id_bytes} bytes")
        
        if pkt_id not in allowed_ids:
            continue
            
        try:
            length = 0
            packet_data = b''
            timestamp_delta = 0
            
            # --- Paketstruktur parsen ---
            if pkt_id >= 32 and pkt_id != 114:
                if i >= len(data): break
                length, length_bytes = decode_varlen_int(data, i)
                i += length_bytes
                
                if i + length > len(data): break
                packet_data = data[i:i+length] if length > 0 else b''
                i += length
                
                if i >= len(data): break
                timestamp_delta, ts_bytes_consumed = decode_varlen_int(data, i)
                i += ts_bytes_consumed
                
            elif pkt_id == 114:
                if i >= len(data): break
                marker_id, m_bytes = decode_varlen_int(data, i)
                i += m_bytes
                packet_data = bytes([marker_id])
                
                if i >= len(data): break
                timestamp_delta, ts_bytes_consumed = decode_varlen_int(data, i)
                i += ts_bytes_consumed
                
            else:
                if pkt_id == 11:
                    if i >= len(data): break
                    isr_id, isr_bytes = decode_varlen_int(data, i)
                    i += isr_bytes
                
                if i >= len(data): break
                timestamp_delta, ts_bytes_consumed = decode_varlen_int(data, i)
                i += ts_bytes_consumed

            # Grundpaket schnüren
            packet = {
                'ID': pkt_id,
                'Length': length,
                'Data': packet_data,
                'TimeStampDelta': timestamp_delta,
                'RawBytes': data[start_pos:i]
            }
            
            # --- Event-Namen zuordnen ---
            if pkt_id in SYSTEMVIEW_EVENTS and pkt_id not in [10, 11]:
                packet['EventName'] = SYSTEMVIEW_EVENTS[pkt_id]
                
            elif pkt_id in [10, 11]:
                packet['EventName'] = SYSTEMVIEW_EVENTS[pkt_id]
                
            elif pkt_id == 114 and len(packet_data) > 0:
                marker_id = packet_data[0]
                if marker_id in SYSTEMVIEW_EVENTS:
                    packet['EventName'] = SYSTEMVIEW_EVENTS[marker_id]
                else:
                    packet['EventName'] = f'Marker_{marker_id}'
                    
            elif len(packet_data) > 0:
                params = list(packet_data)
                packet['Params'] = params
                
                if len(params) > 0:
                    last_param = params[-1]
                    if pkt_id == 129:
                        if last_param == 0: packet['EventName'] = 'ButtonNotPressed'
                        elif last_param == 1: packet['EventName'] = 'ButtonShortPressed'
                        elif last_param == 2: packet['EventName'] = 'ButtonLongPressed'
                        else: packet['EventName'] = f'Button_Unknown_{last_param}'
                    elif pkt_id == 130:
                        if last_param == 0: packet['EventName'] = 'LightOff'
                        elif last_param == 1: packet['EventName'] = 'LightBlink'
                        elif last_param == 2: packet['EventName'] = 'LightOn'
                        else: packet['EventName'] = f'Light_Unknown_{last_param}'

            if 'EventName' not in packet:
                packet['EventName'] = f'Packet_0x{pkt_id:02X}'

            packets.append(packet)
            debug_print(f"Debug: Successfully parsed packet: {packet}")
            
        except Exception as e:
            debug_print(f"Debug: Error parsing packet at position {start_pos}: {e}")
            i = start_pos + 1
            continue

    return packets


def main() -> None:
    """
    @brief Hauptfunktionsknoten.
    @details Liest Argumente ein, verbindet sich per Socket mit J-Link RTT (Telnet),
             konfiguriert Channel 1 und startet die permanente Empfangsschleife.
    """
    global trace_file_path
    
    if len(sys.argv) < 2:
        print("Fehler: Trace-Datei-Pfad als Argument erforderlich")
        print("Verwendung: python get_rtt_events.py <trace_file_path>")
        sys.exit(1)
    
    trace_file_path = sys.argv[1]
    print(f"Verwende Trace-Datei: {trace_file_path}")
    
    trace_opened = open_trace_file()
    if not trace_opened:
        print("Warnung: Trace-Datei konnte nicht geöffnet werden. Ausgabe erfolgt auf Konsole.")
    
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            print(f"Verbinde zu J-Link RTT Telnet {JLINK_HOST}:{JLINK_PORT} ...")
            s.connect((JLINK_HOST, JLINK_PORT))
            s.sendall(b"$$SEGGER_TELNET_ConfigStr=RTTCh;1$$\n")
            print("Verbunden!")
            s.sendall(bytes([1]))
            print("Start-Befehl gesendet. Empfang läuft ...")

            try:
                while True:
                    try:
                        data = s.recv(BUFFER_SIZE)
                        if not data:
                            break

                        debug_print(f"Empfangene Rohdaten: {data}")
                        
                        parsed_packets = parse_systemview_packets(data)
                        for packet in parsed_packets:
                            if 'EventName' in packet:
                                debug_print(f"Event: {packet['EventName']}, Timestamp: {packet['TimeStampDelta']}, Params: {packet.get('Params', [])}")
                            else:
                                debug_print(f"Packet ID: 0x{packet['ID']:02X}, Data: {packet['Data'].hex()}, Timestamp: {packet['TimeStampDelta']}")
                        
                        process_events_with_absolute_timestamp(parsed_packets)

                    except socket.timeout:
                        continue 
            except Exception as e:
                print("Fehler:", e)
    finally:
        close_trace_file()

if __name__ == "__main__":
    main()