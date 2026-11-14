# cross.nix - cross compilers and emulators for `just test-portable-cross`.
#
# Not part of the development shell (shell.nix): they are several hundred
# megabytes and only this one check uses them.  Same nixpkgs revision as
# shell.nix, so everything comes from the binary cache.
let
  pkgs = import (builtins.fetchTarball {
    # Pinned to nixos-24.11 (2024-11-30), as shell.nix
    url = "https://github.com/nixos/nixpkgs/archive/057f63b6dc1a2c67301286152eb5af20747a9cb4.tar.gz";
    sha256 = "17srzd2vif6if6mq6k5prd2kw7zhfnh6bg8ywgz28xj99rvgg4xz";
  }) {};
in {
  s390x = pkgs.pkgsCross.s390x.buildPackages.gcc;                    # 64-bit, big-endian
  ppc64 = pkgs.pkgsCross.ppc64.buildPackages.gcc;                    # 64-bit, big-endian
  arm   = pkgs.pkgsCross.armv7l-hf-multiplatform.buildPackages.gcc;  # 32-bit, little-endian
  i686  = pkgs.pkgsi686Linux.gcc;                                    # 32-bit, little-endian
  qemu  = pkgs.qemu-user;
}
