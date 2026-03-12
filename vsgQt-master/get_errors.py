import subprocess, os
os.chdir(r'C:\Users\cfh12\Desktop\rocky_qt\vsgQt-(rubulid)\vsgQt-master')
result = subprocess.run(
    ['cmake', '--build', 'build/111222333444-Release', '--target', 'vsgqtviewer'],
    capture_output=True, encoding='utf-8', errors='replace'
)
with open(r'C:\Users\cfh12\Desktop\rocky_qt\vsgQt-(rubulid)\vsgQt-master\errors_utf8.txt', 'w', encoding='utf-8') as f:
    f.write(result.stdout)
    f.write(result.stderr)
print('exit:', result.returncode)
