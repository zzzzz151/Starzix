import random

f = open("sf_d9.plain", "r")
targetFile = open("sf_d9_marlinflow.txt", "w")

lines = []
for line in f:
    if len(lines) < 50000000:
        lines.append(line)
    else:
        break

print("Finished reading sf_d9.plain")

leng = len(lines)
i = 0
numWritten = 0
while i < leng and i+2 < leng and numWritten < 200000:
    lines[i] = lines[i].strip() # lines[i] is "fen <fen>"
    lines[i+2] = lines[i+2].strip() # lines[i+2] is "score -529"

    fen = lines[i][4:] # lines[i] is "fen <fen>"

    score = int(lines[i+2][6:]) # lines[i+2] is "score -529"
    if score > 750:
        score = 750
    elif score < -750:
        score = -750
    if " b " in fen:
        score = -score # scores in the file are stm, mirror black ones to get white perspective

    # <fen0> | <eval0> | <wdl0>
    # <fen> is a FEN string
    # <eval> is a evaluation in centipawns from white's point of view
    # <wdl> is 1.0, 0.5, or 0.0, representing a win for white, a draw, or a win for black, respectively.

    if score > 100:
    	wdl = 1.0
    elif score < -100:
    	wdl = 0.0
    else:
    	wdl = 0.5

    targetFile.write(fen + " | " + str(score) + " | " + str(wdl) + "\n")
    numWritten += 1

    i += random.randint(15, 50) * 6 # jump to next random fen ("fen <fen>" line is every 6 lines)

print("Written:", numWritten)