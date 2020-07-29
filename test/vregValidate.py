import sys
import re

if len(sys.argv) == 2 :
    with open(sys.argv[1], 'r') as f:
        data = f.read()
else:
    data = []
    for line in sys.stdin:
        data.append(line)
    data = ''.join(data)

toplevelSplit = re.compile("^function|global")
data = toplevelSplit.split(data)

for i, toplevel in enumerate(data):
    if toplevel.startswith("global"):
        continue

    percentCount = toplevel.count('%')
    errors = []
    for currentReg in range(percentCount):
        reg = f"%{currentReg}"
        if (f"{reg} " in toplevel or f"{reg}]" in toplevel) and (f"{reg} : " not in toplevel):
            errors.append(reg)

    if len(errors) > 0:
        print(f"Function {i} Bad IR Registers: ")
        for error in errors:
            print(error)
        sys.exit(127)