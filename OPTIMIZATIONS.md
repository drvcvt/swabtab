# Vorschläge zur Optimierung von TabSwitcher

Dieses Dokument listet eine Reihe von potenziellen Optimierungen und Verbesserungen für die TabSwitcher-Anwendung auf. Die Vorschläge sind in Kategorien unterteilt, um einen besseren Überblick zu geben.

---

### 1. Konfiguration

Die Anwendung verwendet derzeit hartcodierte Werte für viele Einstellungen. Eine externe Konfigurationsdatei (z.B. `config.ini` oder `config.json`) würde die Anpassbarkeit erheblich verbessern.

- **Hotkey Konfiguration**: Der Hotkey zum Aktivieren des Switchers (`RSHIFT`) ist in `src/main.cpp` hartcodiert. Dies sollte in eine Konfigurationsdatei ausgelagert werden, damit Benutzer ihren bevorzugten Hotkey festlegen können.

- **UI-Anpassung**: Farben, Schriftarten, Fenstergröße und Abstände sind in `src/Config.h` fest definiert. Diese sollten ebenfalls konfigurierbar sein, um Benutzern das Anpassen des Erscheinungsbildes zu ermöglichen.

- **Fenster-Filterung**: Die Logik, welche Fenster angezeigt werden, befindet sich in `src/Utils.cpp` (`IsValidWindow`). Es wäre nützlich, wenn Benutzer über die Konfigurationsdatei eigene Regeln definieren könnten, um bestimmte Anwendungen (basierend auf Prozessnamen oder Fenstertiteln) von der Liste auszuschließen.

---

### 2. Leistung

Obwohl die Anwendung bereits leichtgewichtig ist, gibt es Bereiche, in denen die Leistung weiter optimiert werden kann.

- **Effizientere Suche**: Die aktuelle Implementierung verwendet `std::wregex` für die Suche, was bei jeder Tastenbetätigung ausgeführt wird. Reguläre Ausdrücke können langsam sein. Ein Wechsel zu einem leichtgewichtigeren Fuzzy-Such-Algorithmus (z.B. Substring-Matching oder ein Score-basiertes System) würde die Reaktionsfähigkeit bei der Eingabe verbessern.

- **Caching der Fensterliste**: Die Liste aller Fenster wird jedes Mal neu erstellt, wenn der Switcher mit `m_windowManager->GetAllWindows()` geöffnet wird. Bei vielen offenen Fenstern kann dies zu einer merklichen Verzögerung führen. Die Fensterliste könnte im Hintergrund zwischengespeichert und periodisch aktualisiert oder nur bei Bedarf (z.B. wenn ein Fenster geöffnet/geschlossen wird) neu geladen werden.

- **Optimiertes Rendering**: Die Anwendung zeichnet bei jeder Auswahländerung oder Texteingabe die gesamte Liste neu (`InvalidateRect(m_hwnd, nullptr, TRUE)`). Dies kann zu Flackern führen. Eine gezieltere Neuzeichnung (nur die geänderten Elemente, z.B. die alte und die neue Auswahl) würde das Rendering glatter machen. Die Verwendung von `Direct2D` anstelle von GDI könnte hier ebenfalls erhebliche Vorteile bringen.

---

### 3. Funktionen & Benutzerfreundlichkeit

Neue Funktionen könnten die Nützlichkeit und das Benutzererlebnis erheblich steigern.

- **Fenster-Vorschau (Thumbnails)**: Eine der wirkungsvollsten Verbesserungen wäre die Anzeige einer Live-Vorschau des ausgewählten Fensters. Dies kann mit der DWM-API (`DwmRegisterThumbnail`) realisiert werden und würde dem Benutzer eine viel bessere Orientierung geben.

- **Verbesserte Multi-Monitor-Unterstützung**: Derzeit zentriert sich das Switcher-Fenster auf dem primären Monitor (`Utils::CenterWindow`). Es wäre benutzerfreundlicher, wenn das Fenster auf dem Monitor erscheinen würde, auf dem sich der Mauszeiger gerade befindet.

- **Bessere Fuzzy-Suche**: Anstatt nur Übereinstimmungen zu finden, könnte ein fortschrittlicherer Fuzzy-Finder die Ergebnisse nach Relevanz sortieren (z.B. wie gut der Suchbegriff passt).

- **Mausunterstützung**: Das Hinzufügen von Mausunterstützung (Klicken auf ein Element, um es auszuwählen und zu aktivieren) würde die Anwendung für Benutzer zugänglicher machen, die nicht ausschließlich die Tastatur verwenden.

---

### 4. Code-Qualität und Wartbarkeit

Obwohl der Code gut strukturiert ist, gibt es einige Möglichkeiten für Verbesserungen.

- **Modernere C++-Praktiken**: An einigen Stellen könnten modernere C++-Features (z.B. Smart Pointer für GDI-Objekte mittels benutzerdefinierter Deleter) die Ressourcensicherheit weiter erhöhen.

- **Abstraktion der WinAPI**: Die direkte Verwendung von WinAPI-Aufrufen ist über den Code verteilt. Eine stärkere Kapselung in wiederverwendbaren Klassen (z.B. eine `Window`-Klasse, die `HWND` umschließt, oder eine `GdiObject`-Wrapper-Klasse) könnte die Lesbarkeit und Wartbarkeit verbessern.

- **Fehlerbehandlung**: Die Fehlerbehandlung beschränkt sich oft auf eine `MessageBox`. Ein einfaches Logging-System (z.B. in eine Datei schreiben) könnte die Fehlersuche bei Problemen erleichtern.
