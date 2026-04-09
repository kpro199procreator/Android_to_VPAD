# android_to_vpad — Dockerfile de compilación
# Basado en el patrón estándar de wiiu-env/*
#
# Uso:
#   docker build . -t atv-builder
#   docker run --rm -v ${PWD}:/project atv-builder make
#   docker run --rm -v ${PWD}:/project atv-builder make DEBUG=1

FROM ghcr.io/wiiu-env/devkitppc:20250608

ENV WUPSDIR=/opt/devkitpro/wups

COPY --from=ghcr.io/wiiu-env/wiiupluginsystem:20240505  /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libnotifications:20240426  /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libfunctionpatcher:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiumodulesystem:20250208  /artifacts $DEVKITPRO

WORKDIR /project

WORKDIR /project
