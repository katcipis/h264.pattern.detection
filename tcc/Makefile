LATEX = latex
BIBTEX = bibtex
PS2PDF = ps2pdf
DVIPS = dvips
PDFLATEX = pdflatex
ACENTOS_LATEX = acentosLatex

DVI_VIEWER = xdvi
PS_VIEWER = evince
PDF_VIEWER = evince

WORK_DIR = tmp

PS2PDF_OPTIONS = -sPAPERSIZE=a4
LATEX_OPTIONS = -output-directory=$(WORK_DIR) -halt-on-error

MAIN_TEX_NAME = main

all: dvi pdf bib

dvi: $(MAIN_TEX_NAME)

$(MAIN_TEX_NAME):
	mkdir -p $(WORK_DIR)	
	$(LATEX) $(LATEX_OPTIONS) $(MAIN_TEX_NAME).tex
	$(LATEX) $(LATEX_OPTIONS) $(MAIN_TEX_NAME).tex
	mv $(WORK_DIR)/$(MAIN_TEX_NAME).dvi .
bib:
	$(BIBTEX) $(WORK_DIR)/$(MAIN_TEX_NAME)
   
pdf: ps
	$(PS2PDF) $(PS2PDF_OPTIONS) $(MAIN_TEX_NAME).ps $(MAIN_TEX_NAME).pdf

ps: dvi
	$(DVIPS) $(MAIN_TEX_NAME).dvi

viewdvi: dvi
	$(DVI_VIEWER) $(MAIN_TEX_NAME).dvi

viewps: ps
	$(PS_VIEWER) $(MAIN_TEX_NAME).ps

viewpdf: pdf
	$(PDF_VIEWER) $(MAIN_TEX_NAME).pdf

pdflatex: $(ACENTOS_LATEX) .
	$(PDFLATEX)  $(MAIN_TEX_NAME).tex
	$(BIBTEX) $(MAIN_TEX_NAME)
	$(PDFLATEX)  $(MAIN_TEX_NAME).tex
	$(PDFLATEX)  $(MAIN_TEX_NAME).tex

clean:
	rm -rf $(WORK_DIR)
	rm -rf $(MAIN_TEX_NAME).dvi
	rm -rf $(MAIN_TEX_NAME).ps
	rm -rf $(MAIN_TEX_NAME).pdf
