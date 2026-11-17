# fx68k's structures, made packed, which is how Yosys reads a structure;
# it changes nothing they are.  FROM is fx68k.sv as it was fetched, and TO
# where the same with its structures packed is written (the target `gates`
# runs this).
file(READ "${FROM}" TEXT)
string(REPLACE "typedef struct {" "typedef struct packed {" TEXT "${TEXT}")
file(WRITE "${TO}" "${TEXT}")
