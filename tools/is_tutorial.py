# Exit 0 if a tutorial page (light grey panel in the middle) is on screen (tools/atbat.sh).
import sys, numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert('RGB')).astype(int)
h, w, _ = a.shape
r = a[int(h*0.50):int(h*0.60), int(w*0.30):int(w*0.70)]
grey = (np.abs(r[...,0]-r[...,1]) < 12) & (np.abs(r[...,1]-r[...,2]) < 12) & (r.mean(-1) > 150)
sys.exit(0 if grey.mean() > 0.5 else 1)
