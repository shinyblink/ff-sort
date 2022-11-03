with import <nixpkgs> {};
stdenv.mkDerivation {
  src = ./.;
  name = "ff-sort";
  enableParallelBuilding = true;

  installPhase = ''
      make install PREFIX=$out
    '';
}
