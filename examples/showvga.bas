IF LEN(COMMAND$) = 0 THEN
    PRINT "Usage: SHOWVGA filename.bsv"
    SYSTEM
END IF

SCREEN 13
DEF SEG = &HA000
BLOAD COMMAND$, 0

palcount% = PEEK(64000) + PEEK(64001) * 256

FOR i% = 0 TO palcount%
    offset& = 64000 + (i% * 3) + 2
    red% = PEEK(offset&)
    green% = PEEK(offset& + 1)
    blue% = PEEK(offset& + 2)
    OUT &H3C8, i%
    OUT &H3C9, red%
    OUT &H3C9, green%
    OUT &H3C9, blue%
NEXT i%

WHILE INKEY$ = "": WEND

SCREEN 0
WIDTH 80




