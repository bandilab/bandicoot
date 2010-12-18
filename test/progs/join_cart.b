rel point {
    x: real,
    y: real,
}

rel color {
    r: int,
    g: int,
    b: int,
}

p1: point;
p2: color;

fn s_join(): rel { x: real,
                   y: real,
                   r: int,
                   g: int,
                   b: int }
{
    return p1 * p2;
}
