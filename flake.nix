{
  description = "SSCMA Example for SG200X - Development Environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";

    # Sophgo host-tools containing the RISC-V toolchain
    host-tools = {
      url = "github:sophgo/host-tools";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, flake-utils, host-tools }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # Local SDK path (relative to project root)
        sdkPath = "./temp/sg2002_recamera_emmc";

        # Toolchain path inside host-tools repo
        toolchainSubdir = "gcc/riscv64-linux-musl-x86_64";

        # FHS environment for running the toolchain
        fhsEnv = pkgs.buildFHSEnv {
          name = "sscma-sg200x-fhs";

          targetPkgs = pkgs: with pkgs; [
            # Build tools
            cmake
            ninja
            gnumake
            pkg-config

            # Version control
            git

            # Deploy tools
            openssh
            rsync

            # Debugging/analysis
            file
            binutils

            # Required for toolchain
            stdenv.cc.cc.lib
            zlib
            ncurses5
            python3
            glibc
          ];

          profile = ''
            # Project root
            export PROJECT_ROOT="$(pwd)"

            # Toolchain from Nix store (fetched from sophgo/host-tools)
            export TOOLCHAIN_PATH="${host-tools}/${toolchainSubdir}"
            export PATH="$TOOLCHAIN_PATH/bin:$PATH"
            echo "✓ Toolchain: ${host-tools}/${toolchainSubdir}"

            # Set SDK path
            if [ -d "${sdkPath}" ]; then
              export SG200X_SDK_PATH="$PROJECT_ROOT/temp/sg2002_recamera_emmc"
              echo "✓ SDK: $SG200X_SDK_PATH"
            else
              echo "⚠ SDK not found at ${sdkPath}"
              echo "  Download from: https://github.com/Seeed-Studio/reCamera-OS/releases"
            fi

            # Verify toolchain
            if command -v riscv64-unknown-linux-musl-gcc &> /dev/null; then
              echo "✓ Compiler: $(riscv64-unknown-linux-musl-gcc --version | head -1)"
            else
              echo "⚠ Toolchain compiler not found"
            fi

            echo ""
            echo "╔════════════════════════════════════════════════════════════╗"
            echo "║      SSCMA SG200X Development Environment                  ║"
            echo "╚════════════════════════════════════════════════════════════╝"
            echo ""
            echo "Build a solution:"
            echo "  cd solutions/helloworld"
            echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release ."
            echo "  cmake --build build"
            echo ""
          '';

          runScript = "bash";
        };

      in
      {
        devShells.default = pkgs.mkShell {
          name = "sscma-sg200x";

          buildInputs = [ fhsEnv ];

          shellHook = ''
            echo "Starting FHS environment for toolchain compatibility..."
            exec ${fhsEnv}/bin/sscma-sg200x-fhs
          '';
        };
      });
}
