# Makefile til bogen, generer al data automatisk.. 

MANUAL	= $(OUTDIR)/elinks-manual
OUTDIR	= generated
TARGET	= manual.xml

all: html html_tgz html_zip pdf ps txt

outdir:
	mkdir -p $(OUTDIR)

clean:
	rm -rf $(OUTDIR)

html: outdir
	scripts/make-manual html $(TARGET) $(DBPROC)
	cp style.css manual.html
	scripts/installcss manual.html
	rm -rf $(MANUAL).html
	mv manual.html $(MANUAL).html

html_tgz: html
	tar cfz $(MANUAL).html.tgz $(MANUAL).html

html_zip: html
	zip -r $(MANUAL).zip $(MANUAL).html

pdf: outdir
	scripts/make-manual pdf $(TARGET) $(DBPROC)
	mv manual.pdf $(MANUAL).pdf

ps: outdir
	scripts/make-manual ps $(TARGET) $(DBPROC)
	gzip manual.ps
	mv manual.ps.gz $(MANUAL).ps.gz

txt: outdir
	scripts/make-manual txt $(TARGET) $(DBPROC)
	mv manual.txt $(MANUAL).txt

