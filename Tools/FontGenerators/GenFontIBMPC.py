# Generate font from image
from PIL import Image

with open ("../src/systemfont.c", "w+") as outFile:
    outFile.write("// IBM PC Font\n")
    outFile.write("// Autogenerated file - see GenFontIBMPC.py\n")
    outFile.write("unsigned char __pSystemFontBin[] = {\n")

    pngFile = Image.open("ibmfont.png")
    print(pngFile.size)
    pngData = pngFile.load()
    print(pngData[7,7])

    borderWidth = 0
    pixWidth = 1
    pixHeight = 1
    cellWidthPix = 8
    cellHeightPix = 16
    bytesAcross = 1
    bytesPerChar = 16
    charCellWidth = 8
    charCellHeight = 16

    for rowChar in range(8):
        for colChar in range(32):
            outFile.write("// Char code 0x{:02x}\n".format(rowChar*32+colChar)) 
            for rowPix in range(cellHeightPix):
                outFile.write("0b")
                for colPix in range(cellWidthPix):
                    pixX = colChar*charCellWidth + colPix*pixWidth + borderWidth
                    pixY = rowChar*charCellHeight + rowPix*pixHeight + borderWidth
                    #print(rowChar, colChar, rowPix, colPix, pixX, pixY)
                    outFile.write("0" if pngData[pixX, pixY] != 0 else "1")
                outFile.write(",\n")
            outFile.write("\n")        
    outFile.write("};\n")

    outFile.write("\n#include \"wgfxfont.h\"\n")
    outFile.write("\nWgfxFont __systemFont = {\n")
    outFile.write("    .cellX = " + str(cellWidthPix) + ",\n")
    outFile.write("    .cellY = " + str(cellHeightPix) + ",\n")
    outFile.write("    .bytesAcross = " + str(bytesAcross) + ",\n")
    outFile.write("    .bytesPerChar = " + str(bytesPerChar) + ",\n")
    outFile.write("    .pFontData = __pSystemFontBin\n")
    outFile.write("};\n");

    