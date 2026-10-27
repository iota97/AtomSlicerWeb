import bpy
bpy.context.scene.render.use_lock_interface = True

pathcurve = bpy.path.abspath('//')+'../toolpath/'
pathmesh = bpy.path.abspath('//')+'../mesh/'

try:
    if status == 'OK':
        bpy.ops.wm.ply_import(filepath=pathcurve+f"{id}.ply", directory=pathcurve)
        modifier = bpy.context.active_object.modifiers.new(name='Toolpath', type='NODES')
        modifier.node_group = bpy.data.node_groups['Toolpath']
    
    bpy.ops.wm.stl_import(filepath=pathmesh+f"{id}.stl", directory=pathmesh)
    bpy.ops.view3d.camera_to_view_selected()
    bpy.context.scene.camera.data.ortho_scale = bpy.context.scene.camera.data.ortho_scale * 1.1
    bpy.context.active_object.data.materials.append(bpy.data.materials["Material.001"])
except:
    pass

def place_in_camera_corner(obj=None, corner="BOTTOM_LEFT", margin=0.05, depth=1.0):
    scene = bpy.context.scene
    cam = scene.camera
    obj = obj or bpy.data.objects["Arrow"]
    text_obj = bpy.data.objects["Text"]

    w = cam.data.ortho_scale
    h = w * scene.render.resolution_y / scene.render.resolution_x

    # Scale arrow to 40% of camera width
    bpy.context.view_layer.update()
    sx = obj.dimensions.x
    obj.scale *= (w * 0.4 / sx)
    bpy.context.view_layer.update()
    sx, sy, _ = obj.dimensions

    # Position arrow in camera corner
    x = -w/2 + w*margin + sx/2 if "LEFT" in corner else w/2 - w*margin - sx/2
    y = -h/2 + h*margin + sy/2 if "BOTTOM" in corner else h/2 - h*margin - sy/2

    obj.parent = cam
    obj.location = (x, y, -depth)
    obj.rotation_euler = (0, 0, 0)

    # Update text with arrow width in mm
    text_obj.data.body = f"{sx:.1f} mm"
    bpy.context.view_layer.update()

    # Scale text to 30% of camera width
    text_width = text_obj.dimensions.x
    text_obj.scale *= (w * 0.3 / text_width)
    bpy.context.view_layer.update()

    # Center text above arrow
    text_obj.parent = cam
    text_obj.location = (
        x - text_obj.dimensions.x / 2,
        y + text_obj.dimensions.y/2 - h*margin*0.5,
        -depth
    )
    text_obj.rotation_euler = (0, 0, 0)
    
def add_status_text(status, depth=1.0):
    if status == 'OK':
        return

    scene = bpy.context.scene
    cam = scene.camera

    # Create text object
    bpy.ops.object.text_add()
    text_obj = bpy.context.object
    text_obj.name = "StatusText"
    text_obj.data.body = status

    # Set text size and alignment
    text_obj.data.align_x = 'CENTER'
    text_obj.data.align_y = 'CENTER'
    text_obj.data.size = cam.data.ortho_scale * 0.15

    # Parent to camera and center in the camera view
    text_obj.parent = cam
    text_obj.location = (0, 0, -depth)
    text_obj.rotation_euler = (0, 0, 0)
    text_obj.visible_shadow = False

    # Make text red
    mat = bpy.data.materials.get("StatusMaterial")
    text_obj.data.materials.append(mat)

place_in_camera_corner()

add_status_text(status)

bpy.context.scene.render.filepath = bpy.path.abspath('//')+f'../image/{id}.png'
bpy.ops.render.render(write_still = True)
