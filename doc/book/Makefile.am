# Makefile til bogen, generer al data automatisk.. 

MANUAL	= $(OUTDIR)/elinks-manual
OUTDIR	= generated
TARGET	= manual.xml

alldocs: html html_tgz html_zip pdf ps txt

outdir:
	mkdir -p $(OUTDIR)

clean:
	rm -rf $(OUTDIR)

html: outdir
	scripts/make-manual html $(TARGET) $(DOCTOOL)
	rm -rf $(MANUAL).html
	mv manual.html $(MANUAL).html

html_tgz: html
	tar cfz $(MANUAL).html.tgz $(MANUAL).html

html_zip: html
	zip -r $(MANUAL).zip $(MANUAL).html

pdf: outdir
	scripts/make-manual pdf $(TARGET) $(DOCTOOL)
	mv manual.pdf $(MANUAL).pdf

ps: outdir
	scripts/make-manual ps $(TARGET) $(DOCTOOL)
	gzip manual.ps
	mv manual.ps.gz $(MANUAL).ps.gz

txt: outdir
	scripts/make-manual txt $(TARGET) $(DOCTOOL)
	mv manual.txt $(MANUAL).txt

