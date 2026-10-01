"""Deterministic spherical star data; Radiance long-lat for UE TextureCube import."""
from pathlib import Path
import math, struct, zlib
import numpy as np

OUT = Path(__file__).resolve().parents[2] / 'SourceArt' / 'Sky' / 'DungeonStars_v1'
OUT.mkdir(parents=True, exist_ok=True)
W, H = 4096, 2048
rng = np.random.default_rng(9302026)
sky = np.zeros((H, W), np.float32)
for _ in range(3200):
    lon = rng.uniform(-math.pi, math.pi)
    lat = math.asin(rng.uniform(-1, 1))
    sigma = rng.uniform(0.00065, 0.00105)
    peak = rng.uniform(0.35, 1.0)
    cx = (lon / (2*math.pi) + .5) * W - .5
    cy = (.5 - lat/math.pi) * H - .5
    ry = math.ceil(4*sigma*H/math.pi)+1
    rx = min(W//2, math.ceil(4*sigma*W/(2*math.pi*max(math.cos(lat), .005)))+2)
    ys = np.arange(max(0, math.floor(cy)-ry), min(H, math.ceil(cy)+ry+1))
    xs = np.arange(math.floor(cx)-rx, math.ceil(cx)+rx+1) % W
    phi = (.5-(ys+.5)/H)*math.pi
    theta = ((xs+.5)/W-.5)*2*math.pi
    dot = np.sin(phi[:,None])*math.sin(lat) + np.cos(phi[:,None])*math.cos(lat)*np.cos(theta[None,:]-lon)
    a2 = 2*np.maximum(0, 1-dot)
    patch = peak*np.exp(-a2/(2*sigma*sigma))
    patch[a2 > (4*sigma)**2] = 0
    sky[np.ix_(ys,xs)] = np.maximum(sky[np.ix_(ys,xs)], patch)

mantissa, exponent = np.frexp(sky)
v = np.clip(mantissa*256, 0, 255).astype(np.uint8)
e = np.where(sky > 1e-20, exponent+128, 0).astype(np.uint8)
v[e == 0] = 0
hdr = OUT / 'T_DungeonStars_LongLat_4k.hdr'
with hdr.open('wb') as f:
    f.write(b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2048 +X 4096\n')
    for y in range(H):
        f.write(bytes((2,2,W>>8,W&255)))
        for channel in (v[y],v[y],v[y],e[y]):
            for x in range(0,W,127):
                part = channel[x:x+127]
                f.write(bytes((len(part),)))
                f.write(part.tobytes())

def png(path, pixels):
    height,width = pixels.shape[:2]
    def chunk(tag,data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    raw=b''.join(b'\x00'+row.tobytes() for row in pixels)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

# Six 90-degree rectilinear views sampled from the written environment data.
n=512
t=(np.arange(n)+.5)/n*2-1
xx,yy=np.meshgrid(t,-t)
one=np.ones_like(xx)
views=[]
for axes in [(one,xx,yy),(-one,-xx,yy),(-xx,one,yy),(xx,-one,yy),(xx,yy,one),(xx,-yy,-one)]:
    d=np.stack(axes,-1); d/=np.linalg.norm(d,axis=-1,keepdims=True)
    u=(np.arctan2(d[:,:,1],d[:,:,0])/(2*math.pi)+.5)*W-.5
    q=(.5-np.arcsin(d[:,:,2])/math.pi)*H-.5
    x0=np.floor(u).astype(int); y0=np.floor(q).astype(int)
    fx=u-x0; fy=q-y0
    a=sky[np.clip(y0,0,H-1),x0%W]*(1-fx)+sky[np.clip(y0,0,H-1),(x0+1)%W]*fx
    b=sky[np.clip(y0+1,0,H-1),x0%W]*(1-fx)+sky[np.clip(y0+1,0,H-1),(x0+1)%W]*fx
    g=np.clip((a*(1-fy)+b*fy)**(1/2.2)*255,0,255).astype(np.uint8)
    views.append(np.repeat(g[:,:,None],3,axis=2))
png(OUT/'Six_Directions_Preview.png',np.concatenate([np.concatenate(views[:3],axis=1),np.concatenate(views[3:],axis=1)],axis=0))

# Independent readback of Radiance literal packets and RGBE reconstruction.
with hdr.open('rb') as f:
    while f.readline().strip(): pass
    assert f.readline()==b'-Y 2048 +X 4096\n'
    for y in range(H):
        assert f.read(4)==bytes((2,2,16,0))
        decoded=[]
        for _ in range(4):
            row=bytearray()
            while len(row)<W:
                count=f.read(1)[0]
                assert 0<count<=128
                row.extend(f.read(count))
            assert len(row)==W
            decoded.append(np.frombuffer(row,dtype=np.uint8))
        assert np.array_equal(decoded[0],decoded[1]) and np.array_equal(decoded[1],decoded[2])
        recovered=np.ldexp(decoded[0].astype(np.float32),decoded[3].astype(int)-136)
        assert np.max(np.abs(recovered-sky[y]))<.004
    assert f.read()==b''
print(f'Created and RGBE-readback verified: {hdr}')
print('Preview: +X, -X, +Y / -Y, +Z, -Z')
