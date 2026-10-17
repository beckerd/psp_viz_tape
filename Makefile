.PHONY: all icon host-check profile-check icon-check preview package release clean
# Your own stock 6.61 resource; the cassette icon patch is applied to it.
ICON_SOURCE ?= captures/run_000/visualizer_plugin.rco

all:
	mkdir -p build
	$(MAKE) -f Makefile.render
	$(MAKE) -f Makefile.plugin
	python3 tools/audit_imports.py
	python3 tools/checksums.py

# Development only: the plugin creates this on the PSP itself. Building it here
# needs a private copy of the stock resource (ICON_SOURCE) and is used by icon-check.
visualizer.rco: assets/visualizer-icon.patch.json
	rm -f visualizer.rco
	python3 tools/apply_icon_patch.py "$(ICON_SOURCE)" --output visualizer.rco

icon:
	mkdir -p build
	python3 tools/build_icon.py
	python3 tools/build_icon_patch.py
	cp build/visualizer.rco visualizer.rco

# Checks the install copy of visualizer.rco without regenerating anything.
icon-check: visualizer.rco
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs icon.c paths.c safety.c tests/icon_test.c -o build/icon_test
	build/icon_test captures/run_000/music_player_module.seg0.bin visualizer.rco
	python3 tests/icon_resource_test.py

host-check:
	mkdir -p build
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined tape.c animation.c tests/render_test.c -o build/render_test
	build/render_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined animation.c tests/animation_test.c -o build/animation_test
	build/animation_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined tape.c animation.c mesh.c tests/mesh_test.c -o build/mesh_test
	build/mesh_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined tape.c animation.c mesh.c frame.c tests/frame_test.c -o build/frame_test
	build/frame_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined safety.c tests/safety_test.c -o build/safety_test
	build/safety_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined paths.c tests/paths_test.c -o build/paths_test
	build/paths_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs profile.c icon.c safety.c paths.c tests/compat_test.c -o build/compat_test
	build/compat_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs guard.c paths.c tests/guard_test.c -o build/guard_test
	build/guard_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs module_exports.c tests/module_exports_test.c -o build/module_exports_test
	build/module_exports_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs safety.c paths.c module_exports.c tests/plugin_test.c -o build/plugin_test
	build/plugin_test
	python3 tests/renderer_buffer_test.py
	python3 tests/icon_patch_test.py
	python3 tools/build_icon_header.py --check
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs icon.c icon_build.c safety.c paths.c tests/icon_build_test.c -o build/icon_build_test
	build/icon_build_test $(wildcard visualizer.rco)

profile-check:
	mkdir -p build
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined profile.c safety.c tests/profile_test.c -o build/profile_test
	build/profile_test captures/run_000/visualizer_module.seg0.bin

preview:
	mkdir -p build
	cc -std=c99 -Wall -Wextra -Werror -O2 tape.c animation.c tools/render_preview.c -o build/render_preview
	build/render_preview 440 > build/cassette.ppm

package: all
	python3 tools/package_release.py

release: package

clean:
	$(MAKE) -f Makefile.render clean
	$(MAKE) -f Makefile.plugin clean
	rm -f SHA256SUMS
