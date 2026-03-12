import sys
try:
    with open('c:/Users/cfh12/Desktop/rocky_qt/vsgQt-(rubulid)/vsgQt-master/examples/vsgqtviewer/main.cpp', 'r', encoding='utf-8') as f:
        text = f.read()
    with open('c:/Users/cfh12/Desktop/rocky_qt/vsgQt-(rubulid)/vsgQt-master/examples/vsgqtviewer/main.cpp', 'w', encoding='utf-8-sig') as f:
        f.write(text)
    print('done')
except Exception as e:
    print(e)
