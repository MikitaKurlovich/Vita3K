# Vita3K aarch64 Pi image. Run from the Vita3K checkout.
#   make build
#   make package
#   make deploy PI=192.168.0.204
#   make rollback PI=192.168.0.204 TAG=4074
#   make test

IMAGE ?= vita3k-linux-arm64
PI ?= 192.168.0.204
REMOTE_BIN ?= /userdata/system/vita3k/squashfs-root/usr/bin/Vita3K
SSH ?= ssh -o ControlPath=/tmp/batocera-ssh -o ControlMaster=auto -o ControlPersist=60 -o StrictHostKeyChecking=accept-new
SCP ?= scp -o ControlPath=/tmp/batocera-ssh -o ControlMaster=auto -o StrictHostKeyChecking=accept-new
GIT_TAG := $(shell git rev-parse --short HEAD)
OUT_DIR := dist/linux-arm64
BIN ?= build/linux-ninja-gnu/bin/RelWithDebInfo/Vita3K

.PHONY: build package deploy rollback test

build:
	docker build --platform linux/arm64 -t $(IMAGE) -f Dockerfile .
	docker run --rm --platform linux/arm64 \
		-e VITA3K_JOBS="$(or $(VITA3K_JOBS),4)" \
		-v "$(CURDIR):/src" \
		$(IMAGE)

package:
	@test -x "$(BIN)" || (echo "no Vita3K binary; run make build" >&2; exit 1)
	mkdir -p $(OUT_DIR)
	cp "$(BIN)" $(OUT_DIR)/Vita3K
	patchelf --set-rpath '$$ORIGIN/../lib' $(OUT_DIR)/Vita3K || true
	tar -C $(OUT_DIR) -czf $(OUT_DIR)/vita3k-$(GIT_TAG).tar.gz Vita3K
	@echo "packed $(OUT_DIR)/vita3k-$(GIT_TAG).tar.gz (Qt stays in squashfs usr/lib)"

deploy:
	@test -x "$(BIN)" || (echo "no Vita3K binary; run make build" >&2; exit 1)
	@if [ "$(GIT_TAG)" = "4074" ]; then echo "refusing to overwrite bak-4074" >&2; exit 1; fi
	$(SSH) root@$(PI) "pgrep -x Vita3K >/dev/null && echo 'Vita3K is running; close the game first' >&2 && exit 1; cp -a $(REMOTE_BIN) $(REMOTE_BIN).bak-$(GIT_TAG) && echo backed up bak-$(GIT_TAG)"
	$(SCP) "$(BIN)" root@$(PI):$(REMOTE_BIN)
	$(SSH) root@$(PI) "chmod +x $(REMOTE_BIN) && sync && ls -l $(REMOTE_BIN) $(REMOTE_BIN).bak-$(GIT_TAG)"

rollback:
	@test -n "$(TAG)" || (echo "usage: make rollback PI=host TAG=4074" >&2; exit 1)
	$(SSH) root@$(PI) "test -e $(REMOTE_BIN).bak-$(TAG) && cp -a $(REMOTE_BIN).bak-$(TAG) $(REMOTE_BIN) && sync && ls -l $(REMOTE_BIN)"

test:
	cmake --preset macos-ninja -DBUILD_TESTING=ON
	cmake --build --preset macos-ninja-relwithdebinfo --target kernel-tests util-tests gxm-tests
	ctest --test-dir build/macos-ninja -C RelWithDebInfo -R 'kernel|util|gxm' --output-on-failure
