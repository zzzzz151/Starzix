import random

def generateMarlinflowTextDataFromSfD9Plain():
    f = open("sf_d9.plain", "r")
    targetFile = open("sf_d9_marlinflow.txt", "w")

    lines = []
    for line in f:
        if len(lines) < 70000000:
            lines.append(line)
        else:
            break

    print("Finished reading sf_d9.plain")

    leng = len(lines)
    i = 0
    numWritten = 0
    while i < leng and i+2 < leng and numWritten < 500000:
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

        i += random.randint(15, 30) * 6 # jump to next random fen ("fen <fen>" line is every 6 lines)

    print("Positions written from sf_d9.plain:", numWritten)

def generateMarlinflowTextDataFromWhiteCore():
    f = open("WC.txt", "r")
    """ fen - ply - best move - eval - wdl
    5r2/4rpk1/p1p2R2/7P/Pn2P1B1/2N1bR1P/8/7K w - - 0;52;f6d6;106;1;

    7k/8/8/8/8/pb6/8/1K6 w - - 0;154;b1a1;-566;0;

    r2qkb1r/1pp1np2/p1npb2p/4p1p1/P1P1P3/2NP1N2/1P1B1PPP/R2QKB1R w KQkq g6 0;8;f1e2;-36;1;

    r1bq1rk1/1pnpbpp1/1B2pn1p/P7/Q1P5/P3PN1P/5PP1/RN2KB1R w KQ - 1;16;b1c3;146;0;

    8/8/8/8/8/4k3/8/1K6 w - - 10;204;b1c1;0;0;

    5r2/Rpq1bk1p/5np1/1BPp1p2/3PpP2/4P1PP/2QN1K2/8 w - - 3;42;c2a2;91;1;

    1kbr2r1/p1p5/PpR1np2/1P2p3/4P1p1/3PB3/1R2BP1P/5K2 w - - 0;56;c6c4;0;0;

    1r6/5k2/2n2P1p/pp1R2p1/2p1N3/P1K3P1/1P5P/8 w - - 4;74;h2h4;151;1;

    r1bq1b1r/1pp2kp1/2n1pn1p/pN1p1P2/3P1BP1/2PB4/PP2QP1P/R3K1NR b KQ - 3;13;f8d6;-264;1;

    r5k1/pp1q1bpp/2p2p2/5n2/P2P1N2/1P1Q1PP1/7P/3RB1K1 b - - 0;41;g7g5;84;0;
    """
    FEN = 0
    EVAL = 3
    WDL = 4

    output = open("WC_marlinflow.txt", "w")
    positions = 0
    for line in f:
        line = line.strip()
        if line == "":
            continue
        splitted = line.split(";")

        fen = splitted[FEN]
        stmScore = int(splitted[EVAL])
        score = stmScore if " w " in fen else -stmScore
        wdl = float(splitted[WDL])

        output.write(fen + " 1 | " + str(score) + " | " + str(wdl) + "\n")
        positions += 1

    print("Positions written from WC.txt:", positions)

if __name__ == "__main__":
    #generateMarlinflowTextDataFromSfD9Plain()
    generateMarlinflowTextDataFromWhiteCore()
    
