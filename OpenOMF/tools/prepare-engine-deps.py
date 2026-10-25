"""Pinned libc and declaration-only SDL/ENet headers; no WASI host imports."""
from pathlib import Path
import argparse, hashlib, os, shutil, subprocess, tarfile, urllib.request, zipfile
P=Path(__file__).resolve().parents[1]
D=P/"artifacts/engine-deps"
FILES=[
 ("wasi-sysroot-24.0.tar.gz","https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/wasi-sysroot-24.0.tar.gz","35172f7d2799485b15a46b1d87f50a585d915ec662080f005d99153a50888f08"),
 ("libclang_rt.builtins-wasm32-wasi-24.0.tar.gz","https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/libclang_rt.builtins-wasm32-wasi-24.0.tar.gz","7e33c0df758b90469b1de3ca158e2d0a7f71934d5884525ba6a372de0b3b0ec7"),
 ("SDL2-2.32.10.zip","https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.zip","12b2dc2eb8f2836100a7916b5d394a0c82f1f7e32693f95f98305403af242f08"),
 ("enet-1.3.18.tar.gz","https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz","28603c895f9ed24a846478180ee72c7376b39b4bb1287b73877e5eae7d96b0dd")]
def main():
 a=argparse.ArgumentParser();a.add_argument("--sdk",type=Path,default=P.parents[1]/"aster-sdk");args=a.parse_args();D.mkdir(parents=True,exist_ok=True)
 for name,url,digest in FILES:
  dest=D/name
  if not dest.exists():
   part=dest.with_suffix(dest.suffix+".part")
   with urllib.request.urlopen(url,timeout=120) as src,part.open("wb") as out:shutil.copyfileobj(src,out)
   if hashlib.sha256(part.read_bytes()).hexdigest()!=digest:raise RuntimeError("SHA256 mismatch: "+name)
   part.replace(dest)
  if hashlib.sha256(dest.read_bytes()).hexdigest()!=digest:raise RuntimeError("SHA256 mismatch: "+name)
  print("Verified",name)
 def write(rel,data):
  dest=D/rel
  if not dest.resolve().is_relative_to(D.resolve()):raise ValueError(rel)
  dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
 if not (D/"wasi/lib/wasm32-wasi/libc.a").exists():
  with tarfile.open(D/FILES[0][0]) as t:
   for m in t:
    rel=Path(m.name).relative_to("wasi-sysroot-24.0");s=rel.as_posix()
    if m.isfile() and (s.startswith("include/") or s.startswith("lib/wasm32-wasi/")):write(Path("wasi")/rel,t.extractfile(m).read())
 if not (D/"SDL2/include/SDL.h").exists() or not (D/"SDL2/LICENSE.txt").exists():
  with zipfile.ZipFile(D/FILES[2][0]) as z:
   for name in z.namelist():
    rel=Path(name).relative_to("SDL2-2.32.10")
    if not name.endswith("/") and (rel.as_posix().startswith("include/") or rel.as_posix()=="LICENSE.txt"):write(Path("SDL2")/rel,z.read(name))
 if not (D/"enet/include/enet/enet.h").exists() or not (D/"enet/LICENSE").exists():
  with tarfile.open(D/FILES[3][0]) as t:
   for m in t:
    rel=Path(m.name).relative_to("enet-1.3.18")
    if m.isfile() and (rel.as_posix().startswith("include/") or rel.as_posix()=="LICENSE"):write(Path("enet")/rel,t.extractfile(m).read())
 shutil.copyfile(P/"port/include/SDL_config.h",D/"SDL2/include/SDL_config.h")
 if not (D/"libclang_builtins.a").exists():
  with tarfile.open(D/FILES[1][0]) as t:
   m=next(m for m in t if m.isfile() and m.name.endswith(".a"));write("libclang_builtins.a",t.extractfile(m).read())
 ar=args.sdk/"toolchain/wasi-sdk-24.0-x86_64-windows/bin/llvm-ar.exe"
 if not ar.exists():raise FileNotFoundError(ar)
 shutil.copyfile(D/"wasi/lib/wasm32-wasi/libc.a",D/"libasterc.a")
 subprocess.run([str(ar),"d",str(D/"libasterc.a"),"__wasilibc_real.o"],check=True)
 print("Engine dependencies ready; SDL/ENet implementations are not linked.")
if __name__=="__main__":main()
