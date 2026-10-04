# Maintainer: KitsuneSemCalda <59454155+KitsuneSemCalda@users.noreply.github.com>
#
# PKGBUILD for OmaMovie, and the single source of truth for the project's dependencies
# (CLAUDE.md §4). scripts/deps.sh reads the arrays below to install them for development and CI.
#
#   depends       what the shipped code needs at runtime
#   makedepends   what building the package needs
#   checkdepends  what `make test` needs inside check()
#   _devdepends   development-only tools (spikes, lint, diagnostics); never part of the package
#
# Move a package from _devdepends to depends/makedepends when shipped code starts using it.

pkgname=omamovie-git
pkgver=r0
pkgrel=1
pkgdesc='Native video editor for Omarchy: simple UI, serious GPU pipeline'
arch=('x86_64' 'aarch64')
url='https://github.com/KitsuneForgering/OmaMovie'
license=('MIT')
provides=('omamovie')
conflicts=('omamovie')

depends=(
    'gcc-libs'                  # libstdc++ (libs/base)
    'glibc'
    'vulkan-icd-loader'         # libvulkan (libs/gpu)
    'ffmpeg'                    # libavformat/libavcodec/libavutil/libavfilter/libswresample/libswscale (libs/media)
    'libpipewire'               # PipeWire client (libs/audio)
)
makedepends=(
    'git'
    'vulkan-headers'            # vulkan.hpp / vulkan_raii.hpp (libs/gpu)
    'shaderc'                   # glslc: GLSL compute shaders to SPIR-V (libs/compositor)
)
checkdepends=(
    'vulkan-swrast'             # lavapipe: GPU tests run on machines without a Vulkan driver
)
# Note: ffmpeg also generates the test fixtures in check() (tests/fixtures/generate.sh).

_devdepends=(
    'clang'                     # second compiler in CI, clang-format, clang-tidy
    'llvm'                      # llvm-ar: LTO-aware archiver for Clang builds
    'vulkan-tools'              # vulkaninfo (S1)
    'vulkan-validation-layers'  # validate synchronization in spikes and libs/gpu
    'libva-utils'               # vainfo (S1)
    'qt6-base'                  # Qt GUI and versioned RHI development headers (S4)
    'qt6-declarative'           # Qt Quick (S4; move to depends when an app ships)
)

source=("${pkgname}::git+${url}.git")
sha256sums=('SKIP')

pkgver() {
    cd "${pkgname}"
    if git describe --long --tags >/dev/null 2>&1; then
        git describe --long --tags | sed 's/^v//; s/\([^-]*-g\)/r\1/; s/-/./g'
    else
        printf 'r%s.%s' "$(git rev-list --count HEAD)" "$(git rev-parse --short=7 HEAD)"
    fi
}

build() {
    cd "${pkgname}"
    # The Makefile appends makepkg's CXXFLAGS/LDFLAGS. Warnings from distribution flags must not
    # fail a user's build, so -Werror stays a developer/CI setting.
    make BUILD=release WERROR=0 libs tests
}

check() {
    cd "${pkgname}"
    make BUILD=release WERROR=0 test
}

package() {
    cd "${pkgname}"
    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
    # The omamovie executable, desktop entry and icon are installed here once the app exists (M6/M7).
}
