import os
import shutil
import subprocess

os.makedirs("/usr/local/share/rtav", exist_ok=True)

shutil.copy("shader/vert.vs", "/usr/local/share/rtav/vert.vs")
shutil.copy("shader/frag.fs", "/usr/local/share/rtav/frag.fs")

result = subprocess.run(["cmake -B build && cmake --build build"], shell=True)

if result.returncode == 0:
    shutil.copy("build/rtav", "/usr/local/bin/rtav")
