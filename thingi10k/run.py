#!/usr/bin/env python3

import os
import subprocess
from tqdm import tqdm
import thingi10k
import numpy as np
from stl import mesh

slicer_path = '../build/slicer'
blender_path = '/snap/bin/blender'

def download_dataset():
    thingi10k.init()
    mesh_count = 0
    os.makedirs('mesh', exist_ok=True)
    for entry in tqdm(thingi10k.dataset(solid = True)):
        try:
            file_path = 'mesh/'+str(entry['file_id'])+'.stl'
            if os.path.exists(file_path):
                mesh_count += 1
                continue
            vertices, facets = thingi10k.load_file(entry['file_path'])
            data = mesh.Mesh(np.zeros(facets.shape[0], dtype=mesh.Mesh.dtype))

            min_size = 20
            max_size = 300

            max_dimension = np.max(np.max(vertices, axis = 0) - np.min(vertices, axis = 0))
            if max_dimension < min_size:
                scale = min_size / max_dimension
            elif max_dimension > max_size:
                scale = max_size / max_dimension
            else:
                scale = 1.0

            vertices -= np.min(vertices, axis = 0)
            vertices *= scale

            for i, f in enumerate(facets):
                for j in range(3):
                    data.vectors[i][j] = vertices[f[j], :]

            data.save(file_path)
            mesh_count += 1
        except:
            pass
    return mesh_count

def run_experiment(mesh_count):
    os.makedirs('gcode', exist_ok=True)
    os.makedirs('toolpath', exist_ok=True)
    os.makedirs('log', exist_ok=True)
    os.makedirs('image', exist_ok=True)

    with open('failed.txt', 'w') as failed:
        for f in tqdm(os.listdir('mesh'), total=mesh_count):
            if f[-4:] != '.stl':
                continue

            input = 'mesh/'+f[:-4]+'.stl'
            gcode = 'gcode/'+f[:-4]+'.gcode'
            toolpath = 'toolpath/'+f[:-4]+'.ply'  
            image = 'image/'+f[:-4]+'.png'

            if os.path.exists(image):
                continue
  
            try:
                p = subprocess.run( [ slicer_path, input, "-d", "0.4", "-o", "2", "-3", gcode, "-p", toolpath, "-t", "30.0", "-G"], timeout = 1200, encoding='utf-8', stdout=subprocess.PIPE)
                if p.returncode != 0:
                    if p.returncode == -6 or p.returncode == -9:
                        failed.write(f[:-4] + ' -> OUT OF MEMORY\n')
                    else:
                        failed.write(f[:-4] + ' -> ' + str(p.returncode) + '\n')
                    failed.flush()
                
                with open('log/'+f[:-4]+'.txt', 'w') as log:
                    for line in p.stdout.split('\n'):
                        if not '%' in line:
                            log.write(line+'\n')

                try:
                    status = ""
                    if p.returncode == 0:
                        status = 'OK'
                    elif p.returncode == -6 or p.returncode == -9:
                        status = 'OUT OF\nMEMORY'
                    else:
                        status = 'CRASH'

                    with open('blender/script.py', 'r') as inscript:
                        with open('blender/script_id.py', 'w') as outscript:
                            outscript.write(f'id = "{f[:-4]}"\n')
                            outscript.write(f'status = "{status}"\n')
                            for line in inscript:
                                outscript.write(line)

                    p = subprocess.run( [ blender_path, 'blender/scene.blend', '--background', '--python', 'blender/script_id.py'], timeout = 300, encoding='utf-8', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    if p.returncode != 0:
                        failed.write(f[:-4] + ' -> BLENDER\n')
                        failed.flush()
                except:
                    pass

            except subprocess.TimeoutExpired:
                failed.write(f[:-4] + ' -> TIMEOUT\n')
                failed.flush()

                try:
                    status = "TIMEOUT"
                    with open('blender/script.py', 'r') as inscript:
                        with open('blender/script_id.py', 'w') as outscript:
                            outscript.write(f'id = "{f[:-4]}"\n')
                            outscript.write(f'status = "{status}"\n')
                            for line in inscript:
                                outscript.write(line)
                
                    p = subprocess.run( [ blender_path, 'blender/scene.blend', '--background', '--python', 'blender/script_id.py'], timeout = 300, encoding='utf-8', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    if p.returncode != 0:
                        failed.write(f[:-4] + ' -> BLENDER\n')
                        failed.flush()
                except:
                    pass

if __name__ == "__main__":
    mesh_count = download_dataset()
    run_experiment(mesh_count)