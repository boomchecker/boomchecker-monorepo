---
id: "004"
status: "ready"
priority: "p3"
description: "Finalizace real-time zpracování na STM32"
dependencies: ["003"]
---

# Finalizace real-time zpracování na STM32

## Cíl
Zajistit plynulý běh detekce nad DMA buffery mikrofonu.

## Úkoly
- [ ] Integrovat MFCC + SVM do hlavní smyčky (nebo DMA callbacku).
- [ ] Optimalizovat vytížení CPU.
- [ ] Přidat signalizaci detekce (např. LED nebo UART log).

## Ověření
- Systém běží bez přetečení bufferů a spolehlivě detekuje drony.
