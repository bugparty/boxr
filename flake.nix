{
  description = "BOXR development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-21.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      {
        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            # Build tools
            cmake
            pkg-config
            git
            
            # Libraries from README
            glew
            libGLU
            sqlite
            xorg.libX11
            libGL
            eigen
            spdlog
            fmt
            boost
            gflags
            glfw
            glslang
            libjpeg
            libusb1
            libuvc
            opencv
            openxr-loader
            monado
            libpng
            SDL2
            libtiff
            udev
            vulkan-headers
            vulkan-loader
            vulkan-validation-layers
            wayland
            wayland-protocols
            libxkbcommon
            
            # Shell
            bashInteractive
            
            # X11 extensions
            xorg.libxcb
            xorg.libXext
            xorg.libXrandr
          ];

          shellHook = ''
            echo "Welcome to the BOXR Nix environment!"
            export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${pkgs.lib.makeLibraryPath [
              pkgs.mesa
              pkgs.vulkan-loader
              pkgs.libGL
            ]}
          '';
        };
      }
    );
}
