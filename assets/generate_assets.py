import math
import struct

def generate_torus():
    R = 1.0  # Major radius
    r = 0.35 # Minor radius
    num_major = 24
    num_minor = 16

    vertices = []
    uvs = []
    normals = []

    for i in range(num_major):
        theta = i * 2.0 * math.pi / num_major
        cos_theta = math.cos(theta)
        sin_theta = math.sin(theta)
        
        for j in range(num_minor):
            phi = j * 2.0 * math.pi / num_minor
            cos_phi = math.cos(phi)
            sin_phi = math.sin(phi)
            
            x = (R + r * cos_phi) * cos_theta
            y = r * sin_phi
            z = (R + r * cos_phi) * sin_theta
            vertices.append((x, y, z))
            
            nx = cos_phi * cos_theta
            ny = sin_phi
            nz = cos_phi * sin_theta
            normals.append((nx, ny, nz))
            
            u = i / num_major
            v = j / num_minor
            uvs.append((u, v))

    with open("assets/models/torus.obj", "w") as f:
        f.write("# Overflow Engine 3D Torus\n")
        for v in vertices:
            f.write(f"v {v[0]:.4f} {v[1]:.4f} {v[2]:.4f}\n")
        for uv in uvs:
            f.write(f"vt {uv[0]:.4f} {uv[1]:.4f}\n")
        for n in normals:
            f.write(f"vn {n[0]:.4f} {n[1]:.4f} {n[2]:.4f}\n")
        
        for i in range(num_major):
            next_i = (i + 1) % num_major
            for j in range(num_minor):
                next_j = (j + 1) % num_minor
                
                p1 = i * num_minor + j + 1
                p2 = next_i * num_minor + j + 1
                p3 = next_i * num_minor + next_j + 1
                p4 = i * num_minor + next_j + 1
                
                f.write(f"f {p1}/{p1}/{p1} {p2}/{p2}/{p2} {p3}/{p3}/{p3}\n")
                f.write(f"f {p1}/{p1}/{p1} {p3}/{p3}/{p3} {p4}/{p4}/{p4}\n")

    print("assets/models/torus.obj created successfully!")

def generate_checker_bmp():
    width = 256
    height = 256
    tile_size = 32

    # BMP 24-bit
    row_padding = (4 - (width * 3) % 4) % 4
    image_size = (width * 3 + row_padding) * height
    file_size = 54 + image_size

    # File Header
    file_header = struct.pack('<2sIHHI', b'BM', file_size, 0, 0, 54)
    # Info Header (BITMAPINFOHEADER)
    info_header = struct.pack('<IIIHHIIIIII', 40, width, height, 1, 24, 0, image_size, 2835, 2835, 0, 0)

    with open("assets/textures/uv_checker.bmp", "wb") as f:
        f.write(file_header)
        f.write(info_header)
        
        for y in range(height):
            row = bytearray()
            for x in range(width):
                check = ((x // tile_size) + (y // tile_size)) % 2 == 0
                if check:
                    # Vibrant Cyan / Blue
                    b, g, r = 240, 180, 40
                else:
                    # Dark Slate / Indigo
                    b, g, r = 60, 30, 25
                row.extend([b, g, r])
            row.extend(b'\x00' * row_padding)
            f.write(row)

    print("assets/textures/uv_checker.bmp created successfully!")

if __name__ == '__main__':
    generate_torus()
    generate_checker_bmp()
