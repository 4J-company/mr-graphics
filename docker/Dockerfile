# Ubuntu image with both compilers and a Conan cache of mr-graphics dependencies.
# The project and docker/mr-build.sh are not baked in: mount the repo and run that script.
# Legacy docker build (no BuildKit): the context is staged by the Makefile
# and contains conanfile.py, docker/mr-install-deps.sh, and cci/.

FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive \
    CONAN_NON_INTERACTIVE=1 \
    CONAN_HOME=/opt/conan \
    CPM_SOURCE_CACHE=/opt/cpm-cache \
    VULKAN_SDK=/opt/vulkan \
    PATH="/opt/vulkan/bin:${PATH}" \
    LD_LIBRARY_PATH=/opt/vulkan/lib \
    PKG_CONFIG_PATH=/opt/vulkan/lib/pkgconfig \
    CMAKE_PREFIX_PATH=/opt/vulkan

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        clang \
        curl \
        git \
        mold \
        pkg-config \
        python3 \
        python3-pip \
        python3-venv \
        sudo \
        xz-utils \
    && sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources \
    && apt-get update \
    && apt-get build-dep -y glfw3 \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --break-system-packages 'conan==2.34.0'

RUN mkdir -p /tmp/vulkan-sdk \
    && curl -fL --retry 5 -o /tmp/vulkan_sdk.tar.xz \
        https://sdk.lunarg.com/sdk/download/latest/linux/vulkan_sdk.tar.xz \
    && tar -C /tmp/vulkan-sdk -xf /tmp/vulkan_sdk.tar.xz \
    && rm /tmp/vulkan_sdk.tar.xz \
    && sdkroot="$(find /tmp/vulkan-sdk -type d -name x86_64 | head -1)" \
    && test -n "${sdkroot}" \
    && ln -sfn "${sdkroot}" /opt/vulkan \
    && mkdir -p /opt/conan /opt/cpm-cache /work \
    && printf 'core.net.http:timeout=300\n' >> /opt/conan/global.conf \
    && conan profile detect --force

# mr-utils generates Ninja but does not tool_require it. The full CI image
# has ninja on PATH; this one has to install it.
RUN apt-get update \
    && apt-get install -y --no-install-recommends ninja-build \
    && rm -rf /var/lib/apt/lists/*

COPY cci /opt/cci
COPY conanfile.py /tmp/mr-graphics-recipe/conanfile.py
COPY docker/mr-install-deps.sh /usr/local/bin/mr-install-deps.sh
RUN chmod +x /usr/local/bin/mr-install-deps.sh

ARG COMPILERS="gcc clang"
ARG BUILD_TYPES="Release Debug"
ARG JOBS

WORKDIR /tmp/mr-graphics-recipe
RUN set -eu; \
    printf 'COMPILERS="%s"\nBUILD_TYPES="%s"\n' "${COMPILERS}" "${BUILD_TYPES}" > /etc/mr-graphics-image; \
    chmod a+r /etc/mr-graphics-image; \
    export JOBS="${JOBS}"; \
    for compiler in ${COMPILERS}; do \
      for build_type in ${BUILD_TYPES}; do \
        echo "==== prebuild ${compiler} ${build_type} ===="; \
        COMPILER="${compiler}" BUILD_TYPE="${build_type}" mr-install-deps.sh; \
      done; \
    done; \
    conan cache clean "*" -s -b -d; \
    chmod -R a+rX /opt/vulkan /opt/cci; \
    chmod -R a+rwX /opt/conan /opt/cpm-cache

WORKDIR /work
RUN rm -rf /tmp/mr-graphics-recipe

# default: C++23 for the project. build: C++20 so tool_requires such as mold
# match the packages installed above (mold needs at least C++20).
RUN cp /opt/conan/profiles/default /opt/conan/profiles/build \
    && sed -i 's/^compiler.cppstd=.*/compiler.cppstd=23/' /opt/conan/profiles/default \
    && sed -i 's/^compiler.cppstd=.*/compiler.cppstd=20/' /opt/conan/profiles/build \
    && printf 'core:default_build_profile=build\n' >> /opt/conan/global.conf \
    && chmod a+r /opt/conan/profiles/default /opt/conan/profiles/build /opt/conan/global.conf

ENTRYPOINT []
CMD ["bash"]
