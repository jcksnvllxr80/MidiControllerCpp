# Architecture

Ports & adapters (hexagonal). The domain core is pure C++ and depends only on
the **ports** (interfaces). Adapters implement the ports for a target; the
`Application` wires a concrete set of adapters to the core and runs the loop.

```mermaid
flowchart TB
    EXT["external editor (separate project)"] -- "USB / WiFi editor link" --> APP

    subgraph APP["app/ Application — composition root + event loop"]
      L["poll input → button/encoder handler → mutate state → display → emit MIDI"]
    end

    subgraph PORTS["ports/ (pure virtual HAL)"]
      direction LR
      IMidiOut & IDisplay & ILed
      IInput & IClock & IConfigStore & IConfigTransport
      IWifi & ISystemControl
    end

    subgraph SIM["adapters/sim (now)"]
      direction LR
      LMid["logging midi/led"] & CDisp["console display"]
      SInp["scripted input"] & CClk["chrono clock"] & FsCS["fs config store"]
    end

    subgraph MCU["adapters/mcu (Phase 3)"]
      direction LR
      EXP["MCP23017 footswitches (I²C) + encoder/LED GPIO"] & OLED["SSD1306 (SPI)"]
      DIN["MIDI → PIC bridge (I²C) → 6 jacks"] & STORE["FlashKv store + watchdog"]
      LINK["editor link: USB CDC + WiFi/mDNS :8080"]
    end

    subgraph CORE["domain/ (pure C++, no hardware, no JSON)"]
      direction LR
      MM["MidiMessage · Transform · Value"]
      PC["PedalConfig · MidiPedal"]
      SS["Setlist/Song/Part · ControllerState"]
      BM["ButtonSM · MenuTree"]
    end

    APP --> PORTS
    PORTS --- SIM
    PORTS --- MCU
    APP --> CORE
    CORE --> PORTS

    CFG["config/ConfigLoader\n(only JSON-aware module)"] --> CORE
    DATA[("data/*.json")] --> CFG
```

Data flow on a "load part":

```mermaid
sequenceDiagram
    autonumber
    participant U as Footswitch / Rotary (IInput)
    participant A as Application
    participant M as MenuTree
    participant P as MidiPedal (×N, channel order)
    participant Out as IMidiOut / IDisplay
    U->>A: InputEvent
    alt commit (Select / long-press / menu select)
        A->>P: turnOn/Off → setPreset → setParams
        P->>Out: exact CC/PC bytes
        A->>Out: song-info line
    else preview (Part/Song up/down)
        A->>Out: preview song-info line (no MIDI)
    else rotary
        A->>M: changeMenuPos / pressFor
        M->>Out: menu line
    end
```

> An `architecture.excalidraw` can be exported from the Mermaid above if a
> hand-editable canvas is wanted; the Mermaid here is the canonical source.
