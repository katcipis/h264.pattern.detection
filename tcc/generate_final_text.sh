make clean; make all; make all;
cd artigo
make clean; make all; make all;
cd ..
gs -sDEVICE=pdfwrite -sPAPERSIZE=a4 -dNOPAUSE -sOUTPUTFILE=texto_final.pdf -dBATCH main.pdf ./artigo/artigo.pdf
