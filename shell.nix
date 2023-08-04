# source https://nixos.wiki/wiki/LLVM
with import <nixpkgs> { };
pkgs.llvmPackages_latest.stdenv.mkDerivation {
  name = "llvm-debug-env";
  nativeBuildInputs = [
    pkgs.bashInteractive
    pkgs.ccache
    pkgs.gdb
    pkgs.cmake
    pkgs.ninja
    pkgs.graphviz
    pkgs.llvmPackages_latest.lld
    pkgs.llvmPackages_latest.lldb
    pkgs.pkg-config
    pkgs.mold
  ];
  buildInputs = [ pkgs.zlib ];
  hardeningDisable = [ "all" ];
  PATH_TO_CLANG = "${pkgs.llvmPackages_latest.stdenv.cc}/bin/clang++";
  # FIXME why is this not included?
  NIX_LDFLAGS = "-rpath ${pkgs.zlib}/lib";
}
