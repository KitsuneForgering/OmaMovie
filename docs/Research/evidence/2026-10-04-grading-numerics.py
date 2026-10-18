import numpy as np
rng = np.random.default_rng(1)

# (a) Lift/gamma/gain -> ASC CDL (slope, offset, power) is exact:
# gain*(x + lift*(1-x)) == x*slope + offset with slope = gain*(1-lift), offset = gain*lift
x = rng.random(100000)
worst = 0
for _ in range(1000):
    lift, gain, gamma = rng.uniform(-0.25, 0.25), rng.uniform(0.5, 2), rng.uniform(0.5, 2)
    lgg = np.clip(gain * (x + lift * (1 - x)), 0, 1) ** gamma
    slope, offset = gain * (1 - lift), gain * lift
    cdl = np.clip(x * slope + offset, 0, 1) ** gamma
    worst = max(worst, np.abs(lgg - cdl).max())
print(f"(a) LGG vs CDL max abs diff over 1000 random grades: {worst:.2e}")

# (b) monotone cubic (Fritsch-Carlson) curve sampled into an N-entry table, read with linear interpolation
def pchip(xs, ys, q):
    xs, ys = np.asarray(xs, float), np.asarray(ys, float)
    h = np.diff(xs); d = np.diff(ys) / h
    m = np.zeros_like(xs)
    m[1:-1] = np.where(d[:-1] * d[1:] > 0, (d[:-1] + d[1:]) / 2, 0)
    m[0], m[-1] = d[0], d[-1]
    for k in range(len(d)):  # Fritsch-Carlson limiter
        if d[k] == 0: m[k] = m[k+1] = 0; continue
        a, b = m[k] / d[k], m[k+1] / d[k]
        s = a*a + b*b
        if s > 9: t = 3 / np.sqrt(s); m[k] = t*a*d[k]; m[k+1] = t*b*d[k]
    i = np.clip(np.searchsorted(xs, q) - 1, 0, len(h) - 1)
    t = (q - xs[i]) / h[i]
    h00, h10, h01, h11 = 2*t**3-3*t**2+1, t**3-2*t**2+t, -2*t**3+3*t**2, t**3-t**2
    return h00*ys[i] + h10*h[i]*m[i] + h01*ys[i+1] + h11*h[i]*m[i+1]
q = np.linspace(0, 1, 200001)
for name, xs, ys in [("S-curve", [0, .25, .5, .75, 1], [0, .12, .5, .88, 1]),
                     ("steep crush", [0, .1, .15, 1], [0, 0, .6, 1])]:
    exact = pchip(xs, ys, q)
    for n in (256, 1024):
        table = pchip(xs, ys, np.linspace(0, 1, n))
        approx = np.interp(q, np.linspace(0, 1, n), table)
        print(f"(b) {name:12s} {n:5d} entries: max err {np.abs(exact-approx).max():.2e} (10-bit step {1/1023:.2e})")
    print(f"    overshoot outside [0,1]: {exact.min() < -1e-12 or exact.max() > 1+1e-12}")

# (c) tetrahedral vs trilinear on a nonlinear 3D transform (saturation boost + gamma + hue twist)
def f(rgb):
    luma = rgb @ np.array([0.2126, 0.7152, 0.0722])
    sat = luma[:, None] + 1.6 * (rgb - luma[:, None])
    out = np.clip(sat, 0, 1) ** 0.8
    return out[:, [1, 2, 0]] * 0.3 + out * 0.7
def lut(n):
    g = np.linspace(0, 1, n)
    b, gg, r = np.meshgrid(g, g, g, indexing="ij")  # red fastest
    return f(np.stack([r.ravel(), gg.ravel(), b.ravel()], 1)).reshape(n, n, n, 3)  # [b][g][r]
def trilinear(L, p):
    n = L.shape[0]; s = p * (n - 1); i = np.minimum(np.floor(s).astype(int), n - 2); t = s - i
    out = 0
    for dz in (0, 1):
        for dy in (0, 1):
            for dx in (0, 1):
                w = (t[:,0] if dx else 1-t[:,0]) * (t[:,1] if dy else 1-t[:,1]) * (t[:,2] if dz else 1-t[:,2])
                out = out + w[:, None] * L[i[:,2]+dz, i[:,1]+dy, i[:,0]+dx]
    return out
def tetra(L, p):
    n = L.shape[0]; s = p * (n - 1); i = np.minimum(np.floor(s).astype(int), n - 2); f_ = s - i
    def at(dx, dy, dz): return L[i[:,2]+dz, i[:,1]+dy, i[:,0]+dx]
    fr, fg, fb = f_[:,0:1], f_[:,1:2], f_[:,2:3]
    c000, c111 = at(0,0,0), at(1,1,1)
    out = np.zeros_like(p)
    cases = [  # (condition, vertex1, vertex2, weights)
        ((fr>=fg)&(fg>=fb), at(1,0,0), at(1,1,0), (1-fr, fr-fg, fg-fb, fb)),
        ((fr>=fb)&(fb>fg),  at(1,0,0), at(1,0,1), (1-fr, fr-fb, fb-fg, fg)),
        ((fb>fr)&(fr>=fg),  at(0,0,1), at(1,0,1), (1-fb, fb-fr, fr-fg, fg)),
        ((fg>fr)&(fr>=fb),  at(0,1,0), at(1,1,0), (1-fg, fg-fr, fr-fb, fb)),
        ((fg>=fb)&(fb>fr),  at(0,1,0), at(0,1,1), (1-fg, fg-fb, fb-fr, fr)),
        ((fb>fg)&(fg>fr),   at(0,0,1), at(0,1,1), (1-fb, fb-fg, fg-fr, fr)),
    ]
    done = np.zeros(len(p), bool)
    for cond, v1, v2, (w0, w1, w2, w3) in cases:
        c = cond[:, 0] & ~done
        out[c] = (w0*c000 + w1*v1 + w2*v2 + w3*c111)[c]
        done |= c
    return out
p = rng.random((400000, 3)); exact = f(p)
for n in (17, 33, 65):
    L = lut(n)
    e_tl = np.abs(trilinear(L, p) - exact).max(); e_te = np.abs(tetra(L, p) - exact).max()
    print(f"(c) {n:2d}^3 LUT: trilinear max err {e_tl:.2e}, tetrahedral {e_te:.2e}")
# neutral axis: greys must stay grey
grey = np.repeat(rng.random((20000, 1)), 3, 1); L = lut(33)
for name, fn in (("trilinear", trilinear), ("tetrahedral", tetra)):
    o = fn(L, grey); print(f"(c) 33^3 {name:11s} on greys: max channel spread {np.ptp(o, axis=1).max():.2e} (exact {np.ptp(f(grey), axis=1).max():.2e})")
