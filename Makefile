# Image with prebuilt Conan deps. `make image-build` compiles gcc/clang and
# Release/Debug. `make image-download` pulls the latest published image
# instead. `make build` pulls it too, when the local image is missing,
# then only compiles this repository.
#
#   make image-build
#   make image-download
#   make build
#   make build COMPILER=clang++ BUILD_TYPE=Debug
#   make shell
#   make image-build COMPILERS=gcc BUILD_TYPES=Release
#   make image-clean
#   make image-upload
#
# image-build without CCI clones 4J-company/conan-center-index at master
# into the image. make build uses that copy. CCI=/path mounts another
# index over /opt/cci; only recipe revisions missing from the cache rebuild.
# Host Conan editables are mounted into make build and make shell.
# The image cache stays; those recipes win for that run.

.DEFAULT_GOAL := help

IMAGE ?= mr-graphics-dev:local
REMOTE_IMAGE ?= ghcr.io/4j-company/mr-graphics-dev
REPO := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
CCI ?=
CCI_URL ?= https://github.com/4J-company/conan-center-index.git
CCI_REF ?= master
COMPILER ?= g++
BUILD_TYPE ?= Release
COMPILERS ?= gcc clang
BUILD_TYPES ?= Release Debug
JOBS ?=
# Host file. Unset CONAN_HOME uses ~/.conan2. Missing file means no editables.
CONAN_EDITABLE ?= $(or $(CONAN_HOME),$(HOME)/.conan2)/editable_packages.json

BUILD_PROXY :=
RUN_PROXY :=
ifneq ($(http_proxy),)
BUILD_PROXY += --build-arg http_proxy=$(http_proxy)
RUN_PROXY += -e http_proxy=$(http_proxy)
endif
ifneq ($(https_proxy),)
BUILD_PROXY += --build-arg https_proxy=$(https_proxy)
RUN_PROXY += -e https_proxy=$(https_proxy)
endif
ifneq ($(HTTP_PROXY),)
BUILD_PROXY += --build-arg HTTP_PROXY=$(HTTP_PROXY)
RUN_PROXY += -e HTTP_PROXY=$(HTTP_PROXY)
endif
ifneq ($(HTTPS_PROXY),)
BUILD_PROXY += --build-arg HTTPS_PROXY=$(HTTPS_PROXY)
RUN_PROXY += -e HTTPS_PROXY=$(HTTPS_PROXY)
endif
ifneq ($(no_proxy),)
BUILD_PROXY += --build-arg no_proxy=$(no_proxy)
RUN_PROXY += -e no_proxy=$(no_proxy)
endif
ifneq ($(NO_PROXY),)
BUILD_PROXY += --build-arg NO_PROXY=$(NO_PROXY)
RUN_PROXY += -e NO_PROXY=$(NO_PROXY)
endif

# Mount at the host path. CMake bakes that path into the binary
# (MR_PROJECT_DIR / bin). /work does not exist on the host, so the
# example cannot open shaders or the pipeline cache.
SRC := $(REPO)
MOUNTS := -v $(SRC):$(SRC) -w $(SRC)
ifneq ($(CCI),)
MOUNTS += -v $(abspath $(CCI)):/opt/cci:ro
endif

# Rootless Docker maps container uid 0 to the host user. -u $(id -u) is a
# different uid in that namespace and cannot write the mounted sources.
ifeq ($(shell docker info 2>/dev/null | grep -c rootless),0)
RUN_USER := -u $(shell id -u):$(shell id -g)
else
RUN_USER :=
endif
RUN_ENV := $(RUN_USER) -e HOME=/tmp \
	-e COMPILER=$(COMPILER) -e BUILD_TYPE=$(BUILD_TYPE)
ifneq ($(JOBS),)
RUN_ENV += -e JOBS=$(JOBS)
endif

.PHONY: help image-build image-upload image-download image-clean ensure-image build shell

help:
	@printf '%s\n' \
	'mr-graphics' \
	'' \
	'Targets' \
	'  make build            Compile this repository in the dev image' \
	'  make shell            Open a shell in the image' \
	'  make image-build      Build the image and preinstall Conan deps' \
	'  make image-download   Pull the published image and tag it locally' \
	'  make image-upload     Push the local image to ghcr.io' \
	'  make image-clean      Delete local mr-graphics images (asks first)' \
	'  make help             Show this help' \
	'' \
	'build' \
	'  The usual command. If the local image is missing, it pulls' \
	'  ghcr.io/4j-company/mr-graphics-dev:latest first, then compiles' \
	'  only this repository. docker/mr-build.sh is taken from the' \
	'  working tree, not from inside the image.' \
	'' \
	'  make build' \
	'  make build COMPILER=clang++ BUILD_TYPE=Debug' \
	'  make build CCI=../cci-4j JOBS=8' \
	'' \
	'  COMPILER     g++ (default) or clang++' \
	'               The image must already contain that compiler.' \
	'  BUILD_TYPE   Release (default) or Debug' \
	'               The image must already contain that build type.' \
	'  CCI          Conan recipe index, mounted read-only on /opt/cci.' \
	'               Unset: use the index baked into the image.' \
	'               Set: rebuild only packages whose recipe revision is' \
	'               not in the image cache. That rebuild is discarded' \
	'               when the container exits.' \
	'  JOBS         Parallel compile jobs. Unset: all CPUs.' \
	'  IMAGE        Local image tag (default mr-graphics-dev:local).' \
	'  Editables    Host Conan editables are mounted automatically' \
	'               from ~/.conan2/editable_packages.json, or from' \
	'               $$CONAN_HOME if that variable is set. Each package' \
	'               directory is mounted at the same path, and the json' \
	'               replaces /opt/conan/editable_packages.json. The image' \
	'               cache stays; an editable recipe is used instead of' \
	'               the cached package with the same reference.' \
	'               A dependency rebuilt because of that is discarded' \
	'               when the container exits.' \
	'' \
	'shell' \
	'  Same COMPILER, BUILD_TYPE, CCI, JOBS, IMAGE, and Conan editables' \
	'  as build. Pulls the image the same way, then starts bash. A plain' \
	'  conan build uses the image profile: host C++23, build-profile C++20.' \
	'' \
	'image-build' \
	'  COMPILERS    Which compilers to prebuild (default: gcc clang).' \
	'  BUILD_TYPES  Which build types to prebuild (default: Release Debug).' \
	'  CCI          Recipe tree to copy into the image.' \
	'               Unset: clone conan-center-index branch master.' \
	'  JOBS         Parallel jobs while prebuilding deps.' \
	'  IMAGE        Tag to write (default mr-graphics-dev:local).' \
	'' \
	'image-download / image-upload' \
	'  IMAGE         Local tag (default mr-graphics-dev:local).' \
	'  REMOTE_IMAGE  Registry repo (default ghcr.io/4j-company/mr-graphics-dev).' \
	'  download pulls :latest. upload pushes :latest and an image-id tag.' \
	''

image-build:
	rm -rf /tmp/mr-graphics-docker-context
	mkdir -p /tmp/mr-graphics-docker-context/docker
	cp $(REPO)/conanfile.py /tmp/mr-graphics-docker-context/
	cp $(REPO)/docker/mr-install-deps.sh /tmp/mr-graphics-docker-context/docker/mr-install-deps.sh
	if [ -n "$(CCI)" ]; then \
		test -d "$(abspath $(CCI))" || { echo "CCI not found: $(CCI)" >&2; exit 1; }; \
		cp -a "$(abspath $(CCI))" /tmp/mr-graphics-docker-context/cci; \
	else \
		git clone --depth 1 --branch $(CCI_REF) $(CCI_URL) /tmp/mr-graphics-docker-context/cci; \
	fi
	rm -rf /tmp/mr-graphics-docker-context/cci/.git
	docker build -f $(REPO)/docker/Dockerfile \
		--network=host \
		--build-arg COMPILERS="$(COMPILERS)" \
		--build-arg BUILD_TYPES="$(BUILD_TYPES)" \
		--build-arg JOBS="$(JOBS)" \
		$(BUILD_PROXY) \
		-t $(IMAGE) \
		/tmp/mr-graphics-docker-context
	rm -rf /tmp/mr-graphics-docker-context

# :latest is what image-download pulls. The image id tag does not move.
image-upload:
	docker tag $(IMAGE) $(REMOTE_IMAGE):latest
	docker push $(REMOTE_IMAGE):latest
	id=$$(docker image inspect --format '{{.Id}}' $(IMAGE) | cut -c8-19); \
	docker tag $(IMAGE) $(REMOTE_IMAGE):$$id; \
	docker push $(REMOTE_IMAGE):$$id

image-download:
	docker pull $(REMOTE_IMAGE):latest
	docker tag $(REMOTE_IMAGE):latest $(IMAGE)

# Only mr-graphics-dev / ghcr.io/4j-company/mr-graphics-dev images,
# their untagged build layers, and containers started from those images.
image-clean:
	@printf 'Delete local mr-graphics images and containers? [y/N] ' >/dev/tty; \
	read ans </dev/tty; \
	case "$$ans" in y|Y) ;; *) echo "cancelled"; exit 1 ;; esac
	rm -rf /tmp/mr-graphics-docker-context
	IMAGE=$(IMAGE) REMOTE_IMAGE=$(REMOTE_IMAGE) bash $(REPO)/docker/image-clean.sh

ensure-image:
	docker image inspect $(IMAGE) >/dev/null 2>&1 || $(MAKE) image-download

build: ensure-image
	set -e; \
	editable=$$(bash $(REPO)/docker/editable-mounts.sh '$(CONAN_EDITABLE)'); \
	docker run --rm --network=host $(RUN_ENV) $(RUN_PROXY) $(MOUNTS) $$editable \
		--entrypoint $(SRC)/docker/mr-build.sh $(IMAGE)

shell: ensure-image
	set -e; \
	editable=$$(bash $(REPO)/docker/editable-mounts.sh '$(CONAN_EDITABLE)'); \
	docker run --rm -it --network=host $(RUN_ENV) $(RUN_PROXY) $(MOUNTS) $$editable $(IMAGE) bash
