"""Prepare original vanilla resources and full PSM music. Existing saves are retained."""
from pathlib import Path
import argparse, os, shutil, subprocess
P=Path(__file__).resolve().parents[1]
def linux(p):
 s=str(p.resolve()).replace(chr(92),"/");return "/mnt/"+s[0].lower()+s[2:]
def main():
 a=argparse.ArgumentParser();a.add_argument("--app-id",default="aster.openomf");args=a.parse_args()
 if not args.app_id.replace(".","").replace("-","").isalnum():raise ValueError("Invalid APP id")
 src=P/"artifacts/reference-0.8.6/openomf/resources"
 if not (src/"SOUNDS.DAT").exists():subprocess.run(["powershell","-NoProfile","-ExecutionPolicy","Bypass","-File",str(P/"tools/prepare-reference.ps1")],check=True)
 dst=P/"sdcard/aster/apps"/args.app_id/"data"
 for name in ["resources","config","state"]:(dst/name).mkdir(parents=True,exist_ok=True)
 for f in src.iterdir():
  if f.is_file():shutil.copy2(f,dst/"resources"/f.name)
 ffmpeg=os.environ.get("FFMPEG") or shutil.which("ffmpeg")
 for stem in ["MENU","END"]+[f"ARENA{i}" for i in range(5)]:
  source=src/(stem+".PSM");target=dst/"resources"/(stem+".mp3")
  if target.exists():continue
  part=target.with_suffix(".part.mp3")
  cmd=["-hide_banner","-loglevel","error","-y","-i",str(source),"-c:a","libmp3lame","-b:a","128k","-ar","44100","-ac","2",str(part)]
  if ffmpeg:cmd=[ffmpeg]+cmd
  elif os.name=="nt":
   cmd[cmd.index("-i")+1]=linux(source);cmd[-1]=linux(part);cmd=["wsl","-d",os.environ.get("OPENOMF_WSL_DISTRO","Ubuntu-22.04"),"--exec","ffmpeg"]+cmd
  else:raise RuntimeError("ffmpeg with libopenmpt/libmp3lame is required")
  subprocess.run(cmd,check=True);part.replace(target);print("Converted full track",stem)
 print("Original resources ready:",dst)
if __name__=="__main__":main()
