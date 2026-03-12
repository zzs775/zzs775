import subprocess, os
os.chdir(r'C:\Users\cfh12\Desktop\rocky_qt\vsgQt-(rubulid)\vsgQt-master')
result = subprocess.run(
    ['cmake', '--build', 'build/111222333444-Release', '--target', 'vsgqtviewer'],
    capture_output=True, encoding='utf-8', errors='replace'
)
print('=== STDOUT ===')
print(result.stdout[-3000:])
print('=== STDERR ===')
print(result.stderr[-4000:])
print('Exit code:', result.returncode)
