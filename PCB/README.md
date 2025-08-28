## PCB — Schéma et fabrication

Ce dossier contient les ressources pour fabriquer le circuit imprimé du Timer.

### Fichiers

- `Schematic_Sliders_*.pdf` : schéma électronique détaillé
- `Gerber_Sliders_*.zip` : Gerbers à envoyer au fabricant de PCB

### Commande du PCB

1. Charger l’archive Gerber sur le site du fabricant (JLCPCB, PCBWay, etc.)
2. Paramètres typiques: FR4, 1.6 mm, 1 oz copper, masque de soudure vert (par défaut)
3. Vérifier l’aperçu (sérigraphie, perçages, contours)
4. Valider et commander

### Assemblage

1. Souder d’abord les composants les plus bas (résistances, supports, etc.)
2. Ajouter les circuits (MAX7219, connecteurs, etc.)
3. Câbler l’Arduino Nano et les fils vers les boutons/LED/NeoPixel selon le schéma
4. Inspecter les soudures et réaliser un premier test sous faible tension

