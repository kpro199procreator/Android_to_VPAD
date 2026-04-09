# android_to_vpad — Dockerfile de compilación
# Basado en el patrón estándar de wiiu-env/*
#
# Uso:
#   docker build . -t atv-builder
#   docker run --rm -v ${PWD}:/project atv-builder make
#   docker run --rm -v ${PWD}:/project atv-builder make DEBUG=1

FROM ghcr.io/wiiu-env/devkitppc:latest

# Wii U Toolchain (wut)
COPY --from=ghcr.io/wiiu-env/wut:latest /artifacts $DEVKITPRO

# WUPS — WiiU Plugin System
COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:latest /artifacts $DEVKITPRO

# libnotifications (para mostrar mensajes en pantalla)
COPY --from=ghcr.io/wiiu-env/libnotifications:latest /artifacts $DEVKITPRO

WORKDIR /project
