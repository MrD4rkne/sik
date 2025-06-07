.PHONY: all clean

all: solve.pdf

solve.pdf: solve.md
	pandoc --from=gfm --to=pdf -V geometry:"left=1cm, top=1cm, right=1cm, bottom=2cm" -o $@ $^

clean:
	rm -rf solve.pdf 
	rm -rf *.aux *.log *.out *.toc *.nav *.snm *.vrb