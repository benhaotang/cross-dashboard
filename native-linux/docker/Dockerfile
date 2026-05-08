# Debian 13–style image: full Meson build inside the image (no host bind mount).
# From native-linux/: docker build --progress=plain -f docker/Dockerfile .

FROM debian:trixie-slim

RUN apt-get update -qq \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
        meson ninja-build pkg-config g++ \
        libsqlite3-dev libglib2.0-dev libgtkmm-3.0-dev libhandy-1-dev libsoup-3.0-dev \
        nlohmann-json3-dev libsecret-1-dev libcmark-gfm-dev libcmark-gfm-extensions-dev \
        libnotify-dev \
        libwebkit2gtk-4.1-dev libcairo2-dev libpango1.0-dev libatk1.0-dev \
        libgdk-pixbuf-2.0-dev libgtk-3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN rm -rf build \
    && meson setup build --buildtype=release \
    && meson compile -C build -j 1 \
    && meson test -C build

CMD ["/src/build/cross-dashboard"]
