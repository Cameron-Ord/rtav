import os
import shutil
import subprocess

os.makedirs("/usr/local/share/3dmv", exist_ok=True)

shutil.copy("shader/vert.vs", "/usr/local/share/3dmv/vert.vs")
shutil.copy("shader/frag.fs", "/usr/local/share/3dmv/frag.fs")

result = subprocess.run(["cmake -B build && cmake --build build"], shell=True)

if result.returncode == 0:
    shutil.copy("build/3dmv", "/usr/local/bin/3dmv")
