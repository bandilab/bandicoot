rel point {
    x: real,
    y: real,
}

p1: point;

rel xpoint{
    x: real,
}

p2: xpoint;

fn s_assign()
{
    p1 *= p2;
}
