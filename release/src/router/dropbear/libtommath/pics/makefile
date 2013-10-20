# makes the images... yeah

default:  pses

design_process.ps: design_process.tif
	tiff2ps -s -e design_process.tif > design_process.ps

sliding_window.ps: sliding_window.tif
	tiff2ps -s -e sliding_window.tif > sliding_window.ps
	
expt_state.ps: expt_state.tif
	tiff2ps -s -e expt_state.tif > expt_state.ps

primality.ps: primality.tif
	tiff2ps -s -e primality.tif > primality.ps

design_process.pdf: design_process.ps
	epstopdf design_process.ps

sliding_window.pdf: sliding_window.ps
	epstopdf sliding_window.ps
	
expt_state.pdf: expt_state.ps
	epstopdf expt_state.ps

primality.pdf: primality.ps
	epstopdf primality.ps


pses: sliding_window.ps expt_state.ps primality.ps design_process.ps
pdfes: sliding_window.pdf expt_state.pdf primality.pdf design_process.pdf

clean:
	rm -rf *.ps *.pdf .xvpics
   