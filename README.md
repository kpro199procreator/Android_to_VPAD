# android_to_vpad

Plugin WUPS para Wii U que reemplaza `hid_to_vpad` con soporte nativo de Android.
Recibe input desde un cliente Android/Termux vía TCP e inyecta los botones en `VPADRead`.

---

## Fases del proyecto

| Fase | Estado | Descripción |
|------|--------|-------------|
| **1** | ✅ En desarrollo | Servidor TCP en Wii U + cliente Python/Bash para pruebas iniciales |
| **2** | 🔜 Planificada | App Android completa con mando virtual táctil + Android Input API |

---

## Fase 1 — Arquitectura

```
 Android / Termux                    Wii U (Aroma)
 ┌────────────────────┐              ┌──────────────────────────────┐
 │  atv_client.py     │  TCP :4322   │  android_to_vpad.wps         │
 │  (o atv_send.sh)   │ ──────────► │  serverThread: accept/recv    │
 │                    │  12 bytes   │  → g_virt_hold/trigger/rel    │
 │  Teclado / Script  │  por pkt    │  ↓                            │
 └────────────────────┘              │  DECL_FUNCTION(VPADRead)      │
                                     │  → inyecta en buffers[0]      │
                                     └──────────────────────────────┘
```

### Protocolo de paquetes

Cada paquete es exactamente **12 bytes**, big-endian (network byte order):

```
Offset  Tamaño  Campo    Descripción
0       4       hold     Botones mantenidos actualmente
4       4       trigger  Botones recién presionados (este frame)
8       4       release  Botones recién soltados (este frame)
```

Los valores son bitmasks compatibles con `VPADButtonBitfield` de `vpad/input.h`.

### Bitmask de botones

```
A        = 0x00008000    ZL    = 0x00000080
B        = 0x00004000    ZR    = 0x00000040
X        = 0x00002000    L     = 0x00000020
Y        = 0x00001000    R     = 0x00000010
UP       = 0x00000800    PLUS  = 0x00000008
DOWN     = 0x00000400    MINUS = 0x00000004
LEFT     = 0x00000200    HOME  = 0x00000002
RIGHT    = 0x00000100    LS    = 0x00080000
                         RS    = 0x00040000
```

---

## Compilación

### Con Docker (recomendado)

```bash
docker build . -t atv-builder
docker run --rm -v ${PWD}:/project atv-builder make

# Con logs de debug
docker run --rm -v ${PWD}:/project atv-builder make DEBUG=1
```

### Nativa (con devkitPro instalado)

```bash
make
```

El binario generado es `android_to_vpad.wps`.

---

## Instalación en Wii U

1. Copiar `android_to_vpad.wps` a `SD:/wiiu/environments/aroma/plugins/`
2. Reiniciar la consola (o recargar plugins)
3. El plugin escucha en el puerto TCP **4322** al arrancar cualquier app/juego

**Configuración** → `L + D-Pad Abajo + Minus` en el GamePad:
- Enable/Disable plugin
- Pass real GamePad input (passthrough del mando real)
- TCP Port (cambiar puerto si es necesario)

---

## Uso del cliente

### Python (recomendado para Termux)

```bash
# Instalar en Termux:
pkg install python

# Modo interactivo (escribe botones y Enter):
python3 client/atv_client.py 192.168.1.50

# Modo teclado en tiempo real:
python3 client/atv_client.py 192.168.1.50 --keyboard

# Modo pipe (script de macros):
cat << 'EOF' | python3 client/atv_client.py 192.168.1.50 --pipe
PRESS A
WAIT 0.5
PRESS B
PRESS UP DOWN 0.3
PRESS PLUS
EOF

# Demo automático:
python3 client/atv_client.py 192.168.1.50 --demo

# Ver botones disponibles:
python3 client/atv_client.py --list-buttons
```

### Bash (mínimo, cualquier shell)

```bash
chmod +x client/atv_send.sh

# Presionar un botón:
./client/atv_send.sh 192.168.1.50 A

# Combo de botones:
./client/atv_send.sh 192.168.1.50 ZL ZR

# Puerto personalizado:
./client/atv_send.sh 192.168.1.50 4322 UP
```

### Macro de ejemplo (script Bash)

```bash
#!/bin/bash
IP="192.168.1.50"
send() { ./client/atv_send.sh "$IP" "$@"; }

# Navegar al juego y presionar Start
send HOME
sleep 1
send A
sleep 0.5
send PLUS
```

---

## Logging / Debug

El plugin envía logs por UDP al puerto 4405. Capturar en PC:

```bash
# Linux / macOS
nc -u -l 4405

# O con udplogger
```

---

## Estructura del proyecto

```
android_to_vpad/
├── Dockerfile              # Entorno de compilación reproducible
├── Makefile                # Build del plugin
├── .github/
│   └── workflows/
│       └── build.yml       # GitHub Actions CI
├── src/
│   ├── main.cpp            # Plugin principal (servidor TCP + VPADRead patch)
│   └── logger.h            # Utilidades de logging
└── client/
    ├── atv_client.py       # Cliente Python completo (Termux / PC)
    └── atv_send.sh         # Cliente Bash mínimo (cualquier shell)
```

---

## Fase 2 (próximamente)

- App Android nativa con mando virtual táctil
- Soporte de `android.view.InputDevice` / `MotionEvent`
- Sticks analógicos mapeados a `VPADVec2D`
- Múltiples perfiles de mando
- QR code para conectar escaneando la IP del Wii U
