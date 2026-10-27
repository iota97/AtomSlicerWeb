import math
import os

ids = []
for f in os.listdir('../image'):
    if f[-4:] == '.png':
        ids.append(f[:-4])
ids.sort()

f = open("latex.tex", "w")

f.write(\
r"""
\documentclass{article}
\usepackage[margin=0mm, tmargin=9mm]{geometry}
\usepackage{graphicx}
\usepackage{tikz}
\usepackage[colorlinks=true, urlcolor=black]{hyperref}

\begin{document}
\begin{center}
    \LARGE{\textbf{AtomSlicer Web}}

    \vspace{30pt}
    \normalsize{Thingi10K Results}
    
    \vspace{10pt}
    \normalsize{\href{https://github.com/iota97/AtomSlicerWeb}{https://github.com/iota97/AtomSlicerWeb}}
    \vspace{10pt}
\end{center}
""")

def addBox(idx):
    f.write(\
r"""
\resizebox{0.955\linewidth}{!}{%
\begin{tikzpicture}
""")
    
    for y in range(6 if idx > 0 else 5):
        for x in range(6):  
            if idx >= len(ids):
                continue 
            id = ids[idx]
            f.write(f"\\node[inner sep=0pt] at ({x/12:.5f}\\linewidth, {-y/9.7:.5f}\\linewidth)")
            f.write("{"+f"\\includegraphics[width={1/12:.5f}\\linewidth]")
            f.write(r"{../image/"+str(id)+".png}};")
            f.write(f"\\node[font=\\scriptsize] at ({x/12}\\linewidth,{-y/9.7-0.01-1/24}\\linewidth)")
            f.write("{\\href{https://ten-thousand-models.appspot.com/detail.html?file_id="+str(id)+"}{\\underline{"+str(id)+"}}};\n")
            idx += 1
   
    f.write(\
r"""  
\end{tikzpicture}
}
""") 

addBox(0)
for i in range(math.ceil((len(ids)-5*6)/6/6)):
    addBox(5*6+i*6*6)

f.write(\
r"""
\end{document}
""")