rel 2Dpoint {
    x: real,
    y: real,
}

rel 3Dpoint{
    x: int,
    y: int,
    z: int,
}

p1: 2Dpoint;
p2: 3Dpoint;

fn s_join(): 3Dpoint
{
    return p1 * p2;
}
