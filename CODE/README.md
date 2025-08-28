## Firmware Arduino — Sliders Timer

Ce répertoire contient le firmware principal et les ressources audio.

### Structure

- `sliders_timer_main/sliders_timer_main.ino` : sketch principal
- `LIB/DFPlayer_Mini_Mp3.zip` : librairie DFPlayer Mini fournie
- `SOUNDS/0001.mp3`, `SOUNDS/0002.mp3` : effets sonores (à copier sur microSD)

### Cartes et bibliothèques

- Carte cible: Arduino Nano (ATmega328P). Nécessite A6/A7.
- Bibliothèques à installer dans l’IDE Arduino:
  - LedControl (MAX7219)
  - OneButton
  - Adafruit NeoPixel
  - DFPlayer Mini (`DFPlayer_Mini_Mp3.h`) — archive fournie dans `LIB/`

### Brochage (pins)

- MAX7219 (afficheurs) via `LedControl(12, 11, 10, 2)` :
  - DIN: D12
  - CLK: D11
  - CS: D10
  - Nombre de modules: 2
- Bandeau NeoPixel: `ADAFRUITPIN` = D4
- DFPlayer Mini: utilise `Serial` (9600 bauds). Connecter TX/RX selon le câblage standard Arduino Nano (via un convertisseur de niveau si nécessaire)
- Haut‑parleur (buzzer): D5
- LED statut Arduino: D13
- Boutons: `BP_START`=A0, `BP_UP`=A1, `BP_DOWN`=A2, `BP_VORTEX`=A5 (pull‑up interne activée)
- Potentiomètre: A6 (`pot1`)
- Mesure batterie: A7 (`BATTERYPIN`)
- Interruption: `attachInterrupt(1, buttonStart, FALLING)` → INT1 (D3 sur Nano)

### Sons (DFPlayer)

- Copier `0001.mp3` et `0002.mp3` sur une microSD formatée FAT32
- Nommer les fichiers exactement comme dans `SOUNDS/`
- Insérer la carte dans le DFPlayer Mini

### Compilation et téléversement

1. Ouvrir `sliders_timer_main/sliders_timer_main.ino` dans l’IDE Arduino
2. Sélectionner la carte “Arduino Nano” (ATmega328P, Old Bootloader si nécessaire)
3. Installer les bibliothèques listées ci‑dessus
4. Compiler et téléverser

Exemple via arduino‑cli:

```bash
arduino-cli compile --fqbn arduino:avr:nano
arduino-cli upload -p /dev/ttyUSB0 --fqbn arduino:avr:nano
```

### Notes de fonctionnement

- Le bouton Start bascule l’état (via interruption INT1)
- Les animations LED (NeoPixel) et le bargraphe s’alignent avec les modes (normal, vortex, fin)
- Le buzzer est piloté avec `tone()` si activé (`buzzerState`)

