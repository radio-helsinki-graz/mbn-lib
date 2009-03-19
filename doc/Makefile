DOCFILE = doc

doc.pdf:
	pdflatex ${DOCFILE}
	pdflatex ${DOCFILE} 2>&1 >/dev/null

clean:
	rm -f *.aux *.toc *.log

